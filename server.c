#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "server.h"
#include "client2.h"
#include "awale.h"
#include <sys/select.h>
#include "awale_save.h"

static ServerContext* context;

//TODO : finish changing functions to use the new context

// ************************************************************************************************
// Game related Functions
// ************************************************************************************************


// Initialisation des sessions de jeu
static void initGameSessions(void) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        context->gameSessions[i].isActive = 0;
        context->gameSessions[i].player1 = NULL;
        context->gameSessions[i].player2 = NULL;
        context->gameSessions[i].spectatorCount = 0;
        context->gameSessions[i].waitingForMove = 0;
    }
}

// Fonction d'initialisation du contexte serveur
void initServerContext(SOCKET serverSocket) {
    context = malloc(sizeof(ServerContext));
    context->serverSocket = serverSocket;
    context->actualClients = 0;
    context->maxSocket = serverSocket;
    context->isRunning = true;
    initGameSessions();
}

// Fonction pour envoyer la liste des clients
void sendClientList(Client* client) {
    char client_list[BUF_SIZE] = "Connected clients:\n";
    for(int j = 0; j < context->actualClients; j++) {
        strncat(client_list, context->clients[j].name, BUF_SIZE - strlen(client_list) - 1);
        strncat(client_list, "\n", BUF_SIZE - strlen(client_list) - 1);
    }
    write_client(client->sock, client_list);
}

// Fonction pour envoyer la liste des parties en cours
void sendGamesList(Client* client) {
    char game_list[BUF_SIZE] = "Active games:\n";
    for (int j = 0; j < MAX_GAME_SESSIONS; j++) {
        if (context->gameSessions[j].isActive) {
            strncat(game_list, context->gameSessions[j].player1->name, BUF_SIZE - strlen(game_list) - 1);
            strncat(game_list, " vs ", BUF_SIZE - strlen(game_list) - 1);
            strncat(game_list, context->gameSessions[j].player2->name, BUF_SIZE - strlen(game_list) - 1);
            if(context->gameSessions[j].player1->privacy == PUBLIC && context->gameSessions[j].player2->privacy == PUBLIC) {
                strncat(game_list, " (public)", BUF_SIZE - strlen(game_list) - 1);
            }
            else if (context->gameSessions[j].player1->privacy == FRIENDS || context->gameSessions[j].player2->privacy == FRIENDS) {
                strncat(game_list, " (friends)", BUF_SIZE - strlen(game_list) - 1);
            }
            else {
                strncat(game_list, " (private)", BUF_SIZE - strlen(game_list) - 1);
            }
            strncat(game_list, "\n", BUF_SIZE - strlen(game_list) - 1);
        }
    }
    write_client(client->sock, game_list);
}

// Trouver une session de jeu disponible
static int findFreeGameSession(void) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(!context->gameSessions[i].isActive) {
            return i;
        }
    }
    return -1;
}

// Trouver la session de jeu d'un client
static int findClientGameSession(Client* client) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(context->gameSessions[i].isActive && 
           (context->gameSessions[i].player1 == client || context->gameSessions[i].player2 == client)) {
            return i;
        }
    }
    return -1;
}

// Trouver la session de jeu d'un spectateur
static int findSpectatorGameSession(Client* spectator) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(context->gameSessions[i].isActive) {
            for(int j = 0; j < context->gameSessions[i].spectatorCount; j++) {
                if(context->gameSessions[i].spectators[j] == spectator) {
                    return i;
                }
            }
        }
    }
    return -1;
}


// Créer une nouvelle partie
static int createGameSession(Client* player1, Client* player2) {
    int sessionId = findFreeGameSession();
    if(sessionId == -1) return -1;
    
    GameSession* session = &context->gameSessions[sessionId];
    session->isActive = 1;
    session->player1 = player1;
    session->player2 = player2;
    session->currentPlayerIndex = rand() % 2;
    session->waitingForMove = 1;
    session->lastActivity = time(NULL);
    
    initializeGame(&session->game, player1->name, player2->name);
    return sessionId;
}

// Fonction pour gérer un défi
void handleChallenge(Client* client, const char* challengedName) {
    if (!challengedName || strlen(challengedName) == 0) {
        write_client(client->sock, "Usage: challenge <player_name>\n");
        return;
    }

    // Vérifier qu'on ne se défie pas soi-même
    if (strcmp(client->name, challengedName) == 0) {
        write_client(client->sock, "You cannot challenge yourself.\n");
        return;
    }

    // Vérifier si le client est déjà en jeu
    if (client->inGameOpponent != -1) {
        write_client(client->sock, "You are already in a game.\n");
        return;
    }

    // Vérifier si le client est déjà en attente d'un défi
    if (client->challengedBy != -1) {
        write_client(client->sock, "You already have a challenge request.\n");
        return;
    }

    bool found = false;
    for(int j = 0; j < context->actualClients; j++) {
        if(strcmp(context->clients[j].name, challengedName) == 0) {
            found = true;
            if (context->clients[j].challengedBy != -1) {
                write_client(client->sock, "This player is already challenged by someone else.\n");
                return;
            }
            if (context->clients[j].inGameOpponent != -1) {
                write_client(client->sock, "This player is already in a game.\n");
                return;
            }
            
            char confirmationMessage[BUF_SIZE];
            snprintf(confirmationMessage, sizeof(confirmationMessage), 
                    "Challenge sent to %s\n", challengedName);
            write_client(client->sock, confirmationMessage);

            char challengeMessage[BUF_SIZE];
            snprintf(challengeMessage, sizeof(challengeMessage), 
                    "You have been challenged by %s. Do you accept? (accept/reject)\n", 
                    client->name);
            write_client(context->clients[j].sock, challengeMessage);

            int clientIndex = client - context->clients;
            context->clients[j].challengedBy = clientIndex;
            client->challengedBy = j;
            break;
        }
    }

    if (!found) {
        char errorMsg[BUF_SIZE];
        snprintf(errorMsg, sizeof(errorMsg), "Player '%s' not found.\n", challengedName);
        write_client(client->sock, errorMsg);
    }
}

// Fonction pour gérer une demande de spectateur
void handleWatchRequest(Client* client, const char* request) {
    char *player1_name = strtok(strdup(request), " ");
    char *player2_name = strtok(NULL, "");

    if (player1_name && player2_name) {
        int gameSession = -1;

        for (int j = 0; j < MAX_GAME_SESSIONS; j++) {
            if (context->gameSessions[j].isActive) {
                if ((strcmp(context->gameSessions[j].player1->name, player1_name) == 0 && 
                    strcmp(context->gameSessions[j].player2->name, player2_name) == 0) ||
                    (strcmp(context->gameSessions[j].player1->name, player2_name) == 0 && 
                    strcmp(context->gameSessions[j].player2->name, player1_name) == 0)) {
                    gameSession = j;
                    break;
                }
            }
        }

        if (gameSession != -1) {
            int addResult = addSpectatorToGame(gameSession, client);
            if (addResult == 1) {
                write_client(client->sock, "You are now watching the game.\n");
                GameSession* session = &context->gameSessions[gameSession];
                char serializedGame[BUF_SIZE];
                serializeGame(&session->game, serializedGame, sizeof(serializedGame));
                write_client(client->sock, serializedGame);
            } else if (addResult == 0) {
                write_client(client->sock, "You are not allowed to watch this game.\n");
            } else {
                write_client(client->sock, "No room for spectators in this game.\n");
            }
        } else {
            write_client(client->sock, "No such game found between the specified players.\n");
        }
    }
    free(player1_name); // Libérer la mémoire allouée par strdup
}

// Fonction pour gérer la déconnexion d'un client
void handleClientDisconnect(int clientIndex) {
    Client* client = &context->clients[clientIndex];
    int gameSession = findClientGameSession(client);
    if(gameSession != -1) {
        GameSession* session = &context->gameSessions[gameSession];
        Client* otherPlayer = (session->player1 == client) ? session->player2 : session->player1;
        write_client(otherPlayer->sock, "Your opponent disconnected. Game Over!\n");
        session->isActive = 0;
    }
    
    closesocket(client->sock);
    remove_client(context->clients, clientIndex, &context->actualClients);
}

void handleSaveStateCommand(Client* client, const char* message, AwaleGame *game) 
{
    if (message == NULL || strlen(message) == 0)
    {
        write_client(client->sock, "Usage: save state <save_name>\n");
        return;
    }
    if (saveGame(game, message+1, game->playerNames[0], game->playerNames[1]))
    {
        char response[BUF_SIZE];
        snprintf(response, sizeof(response), "State of game saved successfully as '%s'\n", message+1);
        write_client(client->sock, response); 
    }
    else
    {
        write_client(client->sock, "Failed to save state game.\n");
    }
}


void handleSaveGameCommand(Client* client, const char* message, AwaleGame *game) 
{
    if (message == NULL || strlen(message) == 0)
    {
        write_client(client->sock, "Usage: save <save_name>\n");
        return;
    }
    if (saveCompleteGame(game, message+1, game->playerNames[0], game->playerNames[1], game->moveHistory, game->moveCount))
    {
        char response[BUF_SIZE];
        snprintf(response, sizeof(response), "Game saved successfully as '%s'\n", message+1);
        write_client(client->sock, response); 
    }
    else
    {
        write_client(client->sock, "Failed to save game.\n");
    }
}


void handleLoadCommand(GameSession* session, Client* client, const char* message){
    if (message == NULL || strlen(message) == 0)
    {
        write_client(client->sock, "Usage: load <save_name>\n");
        return;
    }
    write_client(session->player2->sock, "Loading game...\n");
    write_client(session->player1->sock, "Loading game...\n");
    if (loadGame(&session->game, message+1, session->player1->name, session->player2->name))
    {
        char response[BUF_SIZE];
        snprintf(response, sizeof(response), "Game loaded successfully from '%s'\n", message+1);
        write_client(session->player2->sock, response); 
        write_client(session->player1->sock, response);

        char serializedGame[BUF_SIZE];
        serializeGame(&session->game, serializedGame, sizeof(serializedGame));
        write_client(session->player1->sock, serializedGame);
        write_client(session->player2->sock, serializedGame);
    }
    else
    {
        write_client(client->sock, "Failed to load game.\n");
    }
}


// Gérer un coup reçu d'un client
static void handleGameMove(int sessionId, Client* client, const char* buffer) {
    GameSession* session = &context->gameSessions[sessionId];
    
    // Vérifier si c'est bien le tour du joueur
    Client* currentPlayer = (session->currentPlayerIndex == 0) ? session->player1 : session->player2;
    printf("turnCount : %d\n", session->game.turnCount);


    if (strncmp(buffer, "save state", 10) == 0) {
        // Sauvegarder la partie
        handleSaveStateCommand(client, buffer + 10, &session->game);
        //write_client(client->sock, "Game saved.\n");
        return;
    }

    if (strncmp(buffer, "save game", 9) == 0) {
        // Sauvegarder la partie
        handleSaveGameCommand(client, buffer + 9, &session->game);
        //write_client(client->sock, "Game saved.\n");
        return;
    }

    if (strncmp(buffer, "load", 4) == 0){
        if(session->game.turnCount > 0){
            write_client(client->sock, "Game already started. You can't load a game now.\n");
            return;
        }
        // Charger une partie
        handleLoadCommand(session, client, buffer + 4);
        return;
    }

    if(strncmp(buffer,"list saves", 10) == 0){
        listSaves();
        return;
    }

    if (strncmp(buffer, "quit", 4) == 0) {
        // Quitter la partie
        char *winner = (strcmp(client->name, session->player1->name) == 0) ? session->player2->name : session->player1->name;
        session->game.winner = (strcmp(client->name, session->player1->name) == 0) ? 0 : 1;
        session->isActive = 0;
        updateElo(session->player1, session->player2, session->game.winner);
        char message[BUF_SIZE];
        session->player1->inGameOpponent = -1;
        session->player2->inGameOpponent = -1;
        session->player1->challengedBy = -1;
        session->player2->challengedBy = -1;
        snprintf(message, sizeof(message), "%s left the game. %s won!\n", client->name, winner);
        write_client(session->player1->sock, message);
        write_client(session->player2->sock, message);

        printUpdatedElo(session->player1, session->player2);
        return;
    }
    
    if(client != currentPlayer || !session->waitingForMove) {
        write_client(client->sock, "It's not your turn!\n");
        return;
    }

    int house = atoi(buffer);
    if(!verifyMove(&session->game, house - 1)) {
        write_client(client->sock, "Invalid move. Please enter a number between 1 and 6.\n");
        return;
    }

    // Effectuer le coup
    if(makeMove(&session->game, house - 1)) {
        session->game.turnCount++;
        char serializedGame[BUF_SIZE];
        serializeGame(&session->game, serializedGame, sizeof(serializedGame));
        
        // Envoyer l'état du jeu aux deux joueurs
        write_client(session->player1->sock, serializedGame);
        write_client(session->player2->sock, serializedGame);

        // Envoyer aux spectateurs
        for (int i = 0; i < session->spectatorCount; i++) {
            write_client(session->spectators[i]->sock, serializedGame);
        }
        
        // Changer de joueur
        session->currentPlayerIndex = 1 - session->currentPlayerIndex;
        currentPlayer = (session->currentPlayerIndex == 0) ? session->player1 : session->player2;
        write_client(currentPlayer->sock, "Your turn!\n");
        
        session->lastActivity = time(NULL);
        
        // Vérifier si la partie est terminée
        if(session->game.gameOver) {
            handleGameOver(session);
            updateElo(session->player1, session->player2, session->game.winner);
            printUpdatedElo(session->player1, session->player2);
        }
    }
}

void handleGameOver(GameSession* session) {
    char message[BUF_SIZE];
    if(session->game.winner == 0) {
        snprintf(message, sizeof(message), "Game Over! %s wins with %d points!\n", session->player1->name, session->game.score[0]);
    } else if(session->game.winner == 1) {
        snprintf(message, sizeof(message), "Game Over! %s wins with %d points!\n", session->player2->name, session->game.score[1]);
    } else {
        snprintf(message, sizeof(message), "Game Over! It's a tie with %d points each!\n", session->game.score[0]);
    }
    write_client(session->player1->sock, message);
    write_client(session->player2->sock, message);
    session->isActive = 0;
}

void printUpdatedElo(Client* player1, Client* player2) {
    char message[BUF_SIZE];
    snprintf(message, sizeof(message), "Updated ELO ratings: %s: %d\n", player1->name, player1->elo);
    write_client(player1->sock, message);
    snprintf(message, sizeof(message), "Updated ELO ratings: %s: %d\n", player2->name, player2->elo);
    write_client(player2->sock, message);
}

void updateElo(Client* player1, Client* player2, int winner) {
    // Winner is 0 if player 1 wins, 1 if player 2 wins, 2 if it's a tie
    int k = 32;
    double exponent1 = (double)(player2->elo - player1->elo) / 400;
    double expected1 = 1 / (1 + pow(10, exponent1));
    double expected2 = 1 / (1 + pow(10, -exponent1));
    
    double score1, score2;
    if (winner == 0) {
        score1 = 1.0;
        score2 = 0.0;
    } else if (winner == 1) {
        score1 = 0.0;
        score2 = 1.0;
    } else {
        score1 = 0.5;
        score2 = 0.5;
    }
    
    int newElo1 = player1->elo + k * (score1 - expected1);
    int newElo2 = player2->elo + k * (score2 - expected2);
    player1->elo = newElo1;
    player2->elo = newElo2;
}

// Fonction pour gérer les messages de jeu ou de chat
void handleGameOrChat(Client* client, const char* message) {
    int gameSession = findClientGameSession(client);
    if(gameSession != -1 && strncmp(message, "all ", 4) != 0 && strncmp(message, "private ", 8) != 0 && strncmp(message, "accept", 6) != 0 && strncmp(message, "reject", 6) != 0) {
        handleGameMove(gameSession, client, message);                         //TODO add something to save all moves in a game to watch it later
    } else {
        handleChatMessage(client, message);
    }
}

// Fonction pour gérer les messages de chat
void handleChatMessage(Client* client, const char* message) {
    if(strncmp(message, "private ", 8) == 0) {
        handlePrivateMessage(client, message + 8);
    } else if(strncmp(message, "all ", 4) == 0) {
        send_message_to_all_clients(context->clients, *client, context->actualClients, message + 4, 0);
    } else if(client->challengedBy != -1) {
        handleChallengeResponse(client, message);
    } else if(client->friendRequestBy != 0) {
        friendResponse(client, message);
    }
}

void handlePrivateMessage(Client* client, const char* message) {
    // Vérifier si le message est vide
    if (!message || strlen(message) == 0) {
        write_client(client->sock, "Usage: private <client_name> <message>\n");
        return;
    }

    char buffer[BUF_SIZE];
    strncpy(buffer, message, BUF_SIZE - 1);
    buffer[BUF_SIZE - 1] = '\0';

    // Extraire le nom du destinataire
    char* dest_name = strtok(buffer, " ");
    if (!dest_name) {
        write_client(client->sock, "Usage: private <client_name> <message>\n");
        return;
    }

    if (strcmp(dest_name, client->name) == 0) {
        write_client(client->sock, "You cannot send a private message to yourself.\n");
        return;
    }

    // Extraire le message - on utilise strtok(NULL, "") pour obtenir le reste de la chaîne
    char* private_message = strtok(NULL, "");
    if (!private_message) {
        write_client(client->sock, "Usage: private <client_name> <message>\n");
        return;
    }

    // Ignorer les espaces au début du message
    while (*private_message == ' ') {
        private_message++;
    }

    // Vérifier si le message est vide après avoir enlevé les espaces
    if (strlen(private_message) == 0) {
        write_client(client->sock, "Usage: private <client_name> <message>\n");
        return;
    }

    // Chercher le destinataire
    int found = 0;
    for (int j = 0; j < context->actualClients; j++) {
        if (strcmp(context->clients[j].name, dest_name) == 0) {
            char formattedMessage[BUF_SIZE];

            // Confirmation au sender
            snprintf(formattedMessage, sizeof(formattedMessage), 
                    "[Private to %s] %s\n", dest_name, private_message);
            write_client(client->sock, formattedMessage);

            // Envoi du message au destinataire
            snprintf(formattedMessage, sizeof(formattedMessage), 
                    "[Private from %s] %s\n", client->name, private_message);
            write_client(context->clients[j].sock, formattedMessage);

            found = 1;
            break;
        }
    }

    if (!found) {
        char errorMsg[BUF_SIZE];
        snprintf(errorMsg, sizeof(errorMsg), "Client '%s' not found.\n", dest_name);
        write_client(client->sock, errorMsg);
    }
}

// Fonction pour gérer les réponses aux défis
void handleChallengeResponse(Client* client, const char* response) {
    if(strcmp(response, "accept") == 0) {
        int opponentIndex = client->challengedBy;
        int sessionId = createGameSession(client, &context->clients[opponentIndex]);
        if(sessionId != -1) {
            startGame(sessionId, client, &context->clients[opponentIndex]);
        }
        client->challengedBy = -1;
        context->clients[opponentIndex].challengedBy = -1;

        client->inGameOpponent = opponentIndex;
        context->clients[opponentIndex].inGameOpponent = client - context->clients;
    }
    else if(strcmp(response, "reject") == 0) {
        write_client(context->clients[client->challengedBy].sock, "Challenge declined.\n");
        context->clients[client->challengedBy].challengedBy = -1;
        client->challengedBy = -1;
    }
}

// Fonction pour démarrer une partie
void startGame(int sessionId, Client* client1, Client* client2) {
    GameSession* session = &context->gameSessions[sessionId];
    char serializedGame[BUF_SIZE];
    serializeGame(&session->game, serializedGame, sizeof(serializedGame));
    
    write_client(client1->sock, "Game starting! ");
    write_client(client2->sock, "Game starting! ");
    
    if(session->currentPlayerIndex == 0) {
        write_client(client1->sock, "Your turn!\n");
        write_client(client2->sock, "Waiting for opponent...\n");
    } else {
        write_client(client2->sock, "Your turn!\n");
        write_client(client1->sock, "Waiting for opponent...\n");
    }
    
    write_client(client1->sock, serializedGame);
    write_client(client2->sock, serializedGame);
}

// Fonction pour vérifier les timeouts des parties
void checkGameTimeouts() {
    time_t currentTime = time(NULL);
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(context->gameSessions[i].isActive) {
            if(currentTime - context->gameSessions[i].lastActivity > 300) {
                GameSession* session = &context->gameSessions[i];
                write_client(session->player1->sock, "Game timed out!\n");
                write_client(session->player2->sock, "Game timed out!\n");
                session->isActive = 0;
                session->player1->inGameOpponent = -1;
                session->player2->inGameOpponent = -1;
            }
        }
    }
}

// Supprimer un spectateur de la session de jeu
static void removeSpectatorFromGame(int sessionId, Client* spectator) {
    GameSession* session = &(context->gameSessions[sessionId]);
    for (int i = 0; i < session->spectatorCount; i++) {
        if (session->spectators[i] == spectator) {
            // Décaler les spectateurs restants
            memmove(&session->spectators[i], &session->spectators[i + 1],
                    (session->spectatorCount - i - 1) * sizeof(Client*));
            session->spectatorCount--;
            break;
        }
    }
}

// Ajouter un spectateur à une session de jeu
static int addSpectatorToGame(int sessionId, Client* spectator) {
    GameSession* session = &(context->gameSessions[sessionId]);
    if(session->player1->privacy == PRIVATE || session->player2->privacy == PRIVATE) {
        return 0;  // Pas de spectateurs pour les privées
    }
    else if (session->player1->privacy == FRIENDS || session->player2->privacy == FRIENDS) {
        for (int i = 0; i < session->player1->friendCount; i++) {
            if (strcmp(session->player1->friendList[i], spectator->name) == 0) {
                session->spectators[session->spectatorCount++] = spectator;
                return 1;  // Ajout réussi
            }
        }
        for (int i = 0; i < session->player2->friendCount; i++) {
            if (strcmp(session->player2->friendList[i], spectator->name) == 0) {
                session->spectators[session->spectatorCount++] = spectator;
                return 1;  // Ajout réussi
            }
        }
        return 0;  // Pas d'amis dans la partie
    }
    else {
        if (session->spectatorCount < MAX_SPECTATORS) {
            session->spectators[session->spectatorCount++] = spectator;
            return 1;  // Ajout réussi
        }
    }   
    return 2;  // Pas d'espace pour plus de spectateurs
}

static void handleBiography(Client* client, const char* message) {
    // Pour écrire sa propre biographie
    if (strncmp(message, "write", 5) == 0) {
        if (strlen(message) <= 6) {
            write_client(client->sock, "Usage: biography write <your biography text>\n");
            return;
        }
        // Copier le message en sautant "write " (6 caractères)
        strncpy(client->biography, message + 6, BUF_SIZE - 1);
        client->biography[BUF_SIZE - 1] = '\0';  // Assurer la terminaison
        write_client(client->sock, "Biography updated.\n");
    } 
    // Pour lire une biographie
    else if (strncmp(message, "read", 4) == 0) {
        char response[BUF_SIZE];
        response[0] = '\0';

        // Si pas de nom spécifié, lire sa propre biographie
        if (strlen(message) <= 5) {
            snprintf(response, BUF_SIZE, "Your biography:\n%s\n", 
                    (client->biography[0] != '\0') ? client->biography : "No biography set.");
            write_client(client->sock, response);
            return;
        }

        // Extraire le nom du client dont on veut lire la biographie
        // Sauter "read " (5 caractères) et enlever les espaces au début
        const char* targetName = message + 5;
        while (*targetName == ' ') targetName++;
        
        // Chercher le client ciblé
        for (int i = 0; i < context->actualClients; i++) {
            if (strcmp(context->clients[i].name, targetName) == 0) {
                snprintf(response, BUF_SIZE, "Biography of %s:\n%s\n", 
                        context->clients[i].name,
                        (context->clients[i].biography[0] != '\0') ? 
                            context->clients[i].biography : "No biography set.");
                write_client(client->sock, response);
                return;
            }
        }
        
        snprintf(response, BUF_SIZE, "Client '%s' not found.\n", targetName);
        write_client(client->sock, response);
    }
    else {
        write_client(client->sock, "Usage: biography write <text> OR biography read [name]\n");
    }
}

static void addFriend(Client* client, const char* friendName) {
    if (!friendName || strlen(friendName) == 0) {
        write_client(client->sock, "Usage: friend <player_name>\n");
        return;
    }

    // Vérifier qu'on ne s'ajoute pas soi-même
    if (strcmp(client->name, friendName) == 0) {
        write_client(client->sock, "You cannot add yourself as a friend.\n");
        return;
    }

    // Vérifier si l'ami est déjà dans la liste
    for (int i = 0; i < client->friendCount; i++) {
        if (strcmp(client->friendList[i], friendName) == 0) {
            write_client(client->sock, "This player is already in your friend list.\n");
            return;
        }
    }

    bool found = false;
    for (int j = 0; j < context->actualClients; j++) {
        if (strcmp(context->clients[j].name, friendName) == 0) {
            found = true;
            if (client->friendCount >= MAX_FRIENDS) {
                write_client(client->sock, "Your friend list is full.\n");
                return;
            }
            
            if (context->clients[j].friendRequestBy != -1) {
                write_client(client->sock, "This player already has a pending friend request.\n");
                return;
            }

            char confirmationMessage[BUF_SIZE];
            snprintf(confirmationMessage, sizeof(confirmationMessage), 
                    "Friend request sent to %s\n", friendName);
            write_client(client->sock, confirmationMessage);

            char requestMessage[BUF_SIZE];
            snprintf(requestMessage, sizeof(requestMessage), 
                    "%s sent you a friend request. Do you accept? (accept/reject)\n", 
                    client->name);
            write_client(context->clients[j].sock, requestMessage);

            context->clients[j].friendRequestBy = client - context->clients;
            client->friendRequestBy = j;
            break;
        }
    }

    if (!found) {
        char errorMsg[BUF_SIZE];
        snprintf(errorMsg, sizeof(errorMsg), "Player '%s' not found.\n", friendName);
        write_client(client->sock, errorMsg);
    }
}

void friendResponse(Client* client, const char* response) {
    if (!response || client->friendRequestBy == -1) {
        return;
    }

    if (strcmp(response, "accept") == 0) {
        Client* requester = &context->clients[client->friendRequestBy];
        if (client->friendCount >= MAX_FRIENDS || requester->friendCount >= MAX_FRIENDS) {
            write_client(client->sock, "Friend list is full.\n");
            write_client(requester->sock, "Friend list is full.\n");
        } else {
            // Ajouter à la liste d'amis du receveur
            strncpy(client->friendList[client->friendCount], requester->name, BUF_SIZE - 1);
            client->friendList[client->friendCount][BUF_SIZE - 1] = '\0';
            client->friendCount++;

            // Ajouter à la liste d'amis du demandeur
            strncpy(requester->friendList[requester->friendCount], client->name, BUF_SIZE - 1);
            requester->friendList[requester->friendCount][BUF_SIZE - 1] = '\0';
            requester->friendCount++;

            char confirmMsg1[BUF_SIZE], confirmMsg2[BUF_SIZE];
            snprintf(confirmMsg1, sizeof(confirmMsg1), "Friend %s added to your friend list.\n", 
                    requester->name);
            snprintf(confirmMsg2, sizeof(confirmMsg2), "Friend %s added to your friend list.\n", 
                    client->name);
            
            write_client(client->sock, confirmMsg1);
            write_client(requester->sock, confirmMsg2);
        }
    }
    else if (strcmp(response, "reject") == 0) {
        if (client->friendRequestBy >= 0 && client->friendRequestBy < context->actualClients) {
            write_client(context->clients[client->friendRequestBy].sock, 
                        "Friend request declined.\n");
        }
        write_client(client->sock, "Friend request declined.\n");
    }

    // Réinitialiser les états de demande d'ami
    if (client->friendRequestBy >= 0 && client->friendRequestBy < context->actualClients) {
        context->clients[client->friendRequestBy].friendRequestBy = -1;
    }
    client->friendRequestBy = -1;
}

static void listFriends(Client* client) {
    if (client->friendCount == 0) {
        write_client(client->sock, "You have no friends in your list.\n");
        return;
    }

    write_client(client->sock, "Friends list:\n");
    for (int i = 0; i < client->friendCount; i++) {
        char friendStatus[BUF_SIZE];
        bool isOnline = false;
        
        // Vérifier si l'ami est en ligne
        for (int j = 0; j < context->actualClients; j++) {
            if (strcmp(context->clients[j].name, client->friendList[i]) == 0) {
                isOnline = true;
                break;
            }
        }

        snprintf(friendStatus, sizeof(friendStatus), "- %s (%s)\n", 
                client->friendList[i], isOnline ? "online" : "offline");
        write_client(client->sock, friendStatus);
    }
}


static void unfriend(Client* client, const char* friendName) {
    // Chercher l'ami dans la liste du client
    for (int i = 0; i < client->friendCount; i++) {
        if (strcmp(client->friendList[i], friendName) == 0) {
            // Supprimer l'ami de la liste du client
            memmove(&client->friendList[i], &client->friendList[i + 1],
                    (client->friendCount - i - 1) * sizeof(char[BUF_SIZE]));
            client->friendCount--;
            char confirmationMessage[BUF_SIZE];
            snprintf(confirmationMessage, sizeof(confirmationMessage), "Friend %s removed from your friend list.\n", friendName);
            write_client(client->sock, confirmationMessage);

            // Chercher l'ami dans la liste de l'autre client
            Client *friend = NULL;
            for (int j = 0; j < context->actualClients; j++) {
                if (strcmp(context->clients[j].name, friendName) == 0) {
                    friend = &context->clients[j];
                    break;
                }
            }

            if (friend != NULL) {
                // Supprimer le client de la liste des amis de l'autre client
                for (int i = 0; i < friend->friendCount; i++) {
                    if (strcmp(friend->friendList[i], client->name) == 0) {
                        memmove(&friend->friendList[i], &friend->friendList[i + 1],
                                (friend->friendCount - i - 1) * sizeof(char[BUF_SIZE]));
                        friend->friendCount--;
                        break;
                    }
                }

                // Envoyer un message à l'autre client
                char message[BUF_SIZE];
                snprintf(message, sizeof(message), "%s removed you from their friend list.\n", client->name);
                write_client(friend->sock, message);
            }

            return;
        }
    }

    // Si l'ami n'est pas trouvé dans la liste du client
    write_client(client->sock, "Friend not found.\n");
}


static void handleHelp(Client* client) {
    write_client(client->sock, "Available commands:\n");
    write_client(client->sock, "list: List all connected clients\n");
    write_client(client->sock, "games: List all active games\n");
    write_client(client->sock, "challenge <name>: Challenge another client to a game\n");
    write_client(client->sock, "watch <player1> <player2>: Watch a game between two players\n");
    write_client(client->sock, "unwatch: Stop watching a game\n");
    write_client(client->sock, "biography: Read your biography\n");
    write_client(client->sock, "biography write <biography>: Write your biography\n");
    write_client(client->sock, "biography read <name>: Read the biography of another client\n");
    write_client(client->sock, "friends: List your friends\n");
    write_client(client->sock, "friend <name>: Add a friend\n");
    write_client(client->sock, "unfriend <name>: Remove a friend\n");
    write_client(client->sock, "privacy <private/friends/public>: Set your privacy settings\n");
    write_client(client->sock, "privacy: View your privacy settings\n");
    write_client(client->sock, "all <message>: Send a message to all clients\n");
    write_client(client->sock, "private <name> <message>: Send a private message to a client\n");
    write_client(client->sock, "help: Display this help message\n");
    write_client(client->sock, "quit: Exit a game\n");
    write_client(client->sock, "save state <save_name>: Save the state of the game\n");
    write_client(client->sock, "save game <save_name>: Save the game\n");
}


static void handlePrivacy(Client* client, const char* message) {
    // Si aucun message n'est passé, on affiche la confidentialité actuelle du client
    if (message == NULL || strlen(message) == 0) {
        char privacyMessage[BUF_SIZE];
        snprintf(privacyMessage, BUF_SIZE, "Privacy settings: %s\n", privacyToString(client->privacy));
        write_client(client->sock, privacyMessage);
        return;
    }
    // Si un message est passé, on définit la confidentialité en fonction de ce message
    if (strncmp(message, " private", 8) == 0) {
        client->privacy = PRIVATE;  // Affecter l'énumération à privacy
        write_client(client->sock, "Privacy set to private.\n");
    } else if (strncmp(message, " friends", 8) == 0) {
        client->privacy = FRIENDS;
        write_client(client->sock, "Privacy set to friends only.\n");
    } else if (strncmp(message, " public", 7) == 0) {
        client->privacy = PUBLIC;
        write_client(client->sock, "Privacy set to public.\n");
    } else {
        write_client(client->sock, "Usage: privacy <private/friends/public>\n");
    }
}

const char* privacyToString(enum Privacy privacy) {
    switch (privacy) {
        case PRIVATE:
            return "PRIVATE";
        case FRIENDS:
            return "FRIENDS";
        case PUBLIC:
            return "PUBLIC";
        default:
            return "UNKNOWN";
    }
}


static int isNameTaken(const char* name) {
    for(int i = 0; i < context->actualClients; i++) {
        if(strcmp(context->clients[i].name, name) == 0) {
            return 1;  // Nom déjà pris
        }
    }
    return 0;  // Nom disponible
}

static void handleNewConnection(){
    struct message msg;
    SOCKADDR_IN csin = {0};
            socklen_t sinsize = sizeof csin;
            int csock = accept(context->serverSocket, (SOCKADDR *)&csin, &sinsize);
            if(csock == SOCKET_ERROR) {
                perror("accept()");
                return;
            }
    if(read_client(csock, &msg) <= 0) {
        return;
    }

    /* after connecting the client sends its name */
    context->maxSocket = csock > context->maxSocket ? csock : context->maxSocket;

    // Demander et vérifier si le nom est déjà pris
    Client c = {csock};
    strncpy(c.name, msg.content, BUF_SIZE - 1);

    // Vérification du nom
    c.validName = 1;
    if(isNameTaken(c.name)) {
        write_client(c.sock, "Name already taken. Please choose another name:\n");
        write_client(c.sock, "Name: ");
        c.validName = 0;
    }
    c.biography[0] = 0;
    c.challengedBy = -1;
    c.inGameOpponent = -1;
    c.friendCount = 0;
    c.friendRequestBy = -1;
    c.privacy = PUBLIC;
    c.elo = 1000;

    context->clients[context->actualClients] = c;
    context->actualClients++;
}

void handleExit(Client* client) {

    write_client(client->sock, "Goodbye!\n");

    int clientIndex = -1;
    for (int i = 0; i < context->actualClients; i++) {
        if (&context->clients[i] == client) {
            clientIndex = i;
            break;
        }
    }

    // Ensure client exists before processing
    if (clientIndex == -1) {
        return;
    }

    // Mark client as invalid (or any other state management)
    client->sock = INVALID_SOCKET;  // Mark the socket as closed

    if (client->inGameOpponent != -1) {
        // Trouver la session de jeu
        int gameSession = findClientGameSession(client);
        if (gameSession != -1) {
            GameSession* session = &context->gameSessions[gameSession];
            Client* otherPlayer = (session->player1 == client) ? session->player2 : session->player1;
            write_client(otherPlayer->sock, "Your opponent disconnected. Game Over!\n");
            otherPlayer->inGameOpponent = -1;
            otherPlayer->challengedBy = -1;
            session->isActive = 0;
        }
    }

    // Clean up session details, remove from game/spectator lists, etc.
    int spectatingSession = findSpectatorGameSession(client);
    if (spectatingSession != -1) {
        removeSpectatorFromGame(spectatingSession, client);
    }

    // Close socket
    closesocket(client->sock);

    // Remove the client from the client list
    remove_client(context->clients, clientIndex, &context->actualClients);
}



void processClientMessage(Client* client, const char* message) {
    if(client->sock == INVALID_SOCKET) {
        // Client socket is closed, skip processing
        return;
    }

    if(!client->validName) {
        // Vérifier si le nom est déjà pris
        if(isNameTaken(message)) {
            write_client(client->sock, "Name already taken. Please choose another name:\n");
            write_client(client->sock, "Name: ");
            return;
        }
        strncpy(client->name, message, BUF_SIZE - 1);
        client->validName = 1;
        write_client(client->sock, "Welcome to the server!\n");
    }
    else
    if(strcmp(message, "list") == 0) {
        sendClientList(client);
    }
    else if(strcmp(message, "games") == 0) {
        sendGamesList(client);
    }
    else if(strncmp(message, "challenge ", 10) == 0) {
        char * challengedName = message + 10;
        challengedName[strcspn(challengedName, "\n")] = 0;
        handleChallenge(client, challengedName);
    }
    else if(strncmp(message, "watch ", 6) == 0) {
        handleWatchRequest(client, message + 6);
    }
    else if(strncmp(message, "unwatch", 7) == 0) {
        int gameSession = findSpectatorGameSession(client);
        if(gameSession != -1) {
            removeSpectatorFromGame(gameSession, client);
            write_client(client->sock, "You are no longer watching the game.\n");
        } else {
            write_client(client->sock, "You are not watching any game.\n");
        }
    }
    else if(strncmp(message, "biography", 9) == 0) {
        handleBiography(client, message + 10);
    }
    else if (strcmp(message, "friends") == 0) {
        listFriends(client);
    }
    else if(strncmp(message, "friend", 6) == 0) {
        char * friendName = message + 7;
        addFriend(client, friendName);
    }
    else if(strncmp(message, "unfriend", 8) == 0) {
        char * friendName = message + 9;
        unfriend(client, friendName);
    }
    else if(strncmp(message, "privacy", 7) == 0) {
        char* privacy = message + 7;
        handlePrivacy(client, privacy);
    }
    else if (strncmp(message, "help", 4) == 0) {
        handleHelp(client);
    }
    else if (strncmp(message, "exit", 4) == 0) {
        handleExit(client);
    }
    else {
        handleGameOrChat(client, message);
    }
}

// Main application loop
static void app(void) {
    SOCKET sock = init_connection();
    initServerContext(sock);
    struct message msg;
    fd_set rdfs;

    while(context->isRunning) {
        FD_ZERO(&rdfs);
        FD_SET(STDIN_FILENO, &rdfs);
        FD_SET(sock, &rdfs);

        for(int i = 0; i < context->actualClients; i++) {
            FD_SET(context->clients[i].sock, &rdfs);
            context->maxSocket = context->clients[i].sock > context->maxSocket ? context->clients[i].sock : context->maxSocket;
        }

        if(select(context->maxSocket + 1, &rdfs, NULL, NULL, NULL) == -1) {
            perror("select()");
            exit(errno);
        }

        if(FD_ISSET(STDIN_FILENO, &rdfs)) {
            break;
        }
        else if(FD_ISSET(sock, &rdfs)) {
            handleNewConnection();
        }
        else {
            for(int i = 0; i < context->actualClients; i++) {
                if(FD_ISSET(context->clients[i].sock, &rdfs)) {
                    Client* client = &context->clients[i];

                    if(client->sock == INVALID_SOCKET) {
                        continue;  // Skip this client as their socket is closed
                    }

                    int c = read_client(client->sock, &msg);
                    
                    if(c == 0) {
                        handleClientDisconnect(i);
                        continue;
                    }
                    
                    processClientMessage(client, msg.content);
                }
            }
        }

        checkGameTimeouts();
    }

    clear_clients(context->clients, context->actualClients);
    end_connection(sock);
    free(context);
}


// ************************************************************************************************
// Server related functions
// ************************************************************************************************

static int init_connection(void) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN sin = {0};

    if(sock == INVALID_SOCKET) {
        perror("socket()");
        exit(errno);
    }

    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(PORT);
    sin.sin_family = AF_INET;

    if(bind(sock, (SOCKADDR *)&sin, sizeof sin) == SOCKET_ERROR) {
        perror("bind()");
        exit(errno);
    }

    if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR) {
        perror("listen()");
        exit(errno);
    }

    return sock;
}


static void end_connection(int sock) {
    closesocket(sock);
}

static void clear_clients(Client *clients, int actual) {
    for(int i = 0; i < actual; i++) {
        closesocket(clients[i].sock);
    }
}

static int read_client(SOCKET sock, struct message *msg)
{
    // Lire d'abord la taille
    int size_read = recv(sock, &msg->size, sizeof(uint32_t), 0);
    if (size_read != sizeof(uint32_t)) {
        if (size_read == 0) return 0;  // Connexion fermée
        perror("recv() size");
        return -1;
    }

    // Vérifier que la taille est valide
    if (msg->size >= BUF_SIZE) {
        fprintf(stderr, "Message trop grand: %u bytes\n", msg->size);
        return -1;
    }

    // Lire ensuite exactement le nombre d'octets indiqué
    int content_read = 0;
    int remaining = msg->size;

    while (remaining > 0) {
        int n = recv(sock, msg->content + content_read, remaining, 0);
        if (n <= 0) {
            if (n == 0) return 0;  // Connexion fermée
            perror("recv() content");
            return -1;
        }
        content_read += n;
        remaining -= n;
    }

    // Ajouter le caractère nul de fin
    msg->content[msg->size] = 0;

    return content_read;
}


static void write_client(SOCKET sock, const char *buffer)
{
    struct message msg;
    msg.size = strlen(buffer);
    
    if (msg.size >= BUF_SIZE) {
        fprintf(stderr, "Message trop grand\n");
        return;
    }
    
    strcpy(msg.content, buffer);
    
    // Envoyer la taille
    if (send(sock, &msg.size, sizeof(uint32_t), 0) != sizeof(uint32_t)) {
        perror("send() size");
        return;
    }
    
    // Envoyer le contenu
    if (send(sock, msg.content, msg.size, 0) != msg.size) {
        perror("send() content");
        return;
    }
}

static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server) {
    char message[BUF_SIZE];
    message[0] = 0;
    
    for(int i = 0; i < actual; i++) {
        if(sender.sock != clients[i].sock) {
            if(from_server == 0) {
                strncpy(message, sender.name, BUF_SIZE - 1);
                strncat(message, " : ", sizeof message - strlen(message) - 1);
            }
            strncat(message, buffer, sizeof message - strlen(message) - 1);
            write_client(clients[i].sock, message);
        }
    }
}

void remove_client(Client* clients, int indexToRemove, int* actualClients) {
    // Vérifier si l'index est valide
    if (indexToRemove < 0 || indexToRemove >= *actualClients) {
        return;
    }

    // Utiliser une approche de décalage mémoire plus sûre
    memmove(&clients[indexToRemove], &clients[indexToRemove + 1], 
            (*actualClients - indexToRemove - 1) * sizeof(Client));

    // Décrémenter le nombre de clients
    (*actualClients)--;
}

static void init(void) {
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0) {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void) {
#ifdef WIN32
   WSACleanup();
#endif
}


int main(int argc, char **argv) {
    init();
    app();
    end();
    return EXIT_SUCCESS;
}