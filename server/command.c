#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "command.h"
#include "util.h"
#include "data.h"
#include "../client/client2.h"
#include "../awale/awale.h"
#include "../awale/awale_save.h"
#include "../message.h"


// Fonction qui attribue le message d'un joueur à une commande -- server
void processClientMessage(Client* client, const char* message, ServerContext* context) {
    if(client->sock == INVALID_SOCKET) {
        // Client socket is closed, skip processing
        return;
    }

    if(!client->validName) {
        // Vérifier si le nom est déjà pris
        if(isNameTaken(message, context)) {
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
        handleSendClientList(client, context);
    }
    else if(strcmp(message, "games") == 0) {
        handleSendGamesList(client, context);
    }
    else if(strncmp(message, "challenge ", 10) == 0) {
        char * challengedName = message + 10;
        challengedName[strcspn(challengedName, "\n")] = 0;
        handleChallenge(client, challengedName, context);
    }
    else if(strncmp(message, "watch ", 6) == 0) {
        handleWatchRequest(client, message + 6, context);
    }
    else if(strncmp(message, "unwatch", 7) == 0) {
        int gameSession = findSpectatorGameSession(client, context);
        if(gameSession != -1) {
            removeSpectatorFromGame(gameSession, client, context);
            write_client(client->sock, "You are no longer watching the game.\n");
        } else {
            write_client(client->sock, "You are not watching any game.\n");
        }
    }
    else if(strncmp(message, "biography", 9) == 0) {
        handleBiography(client, message + 10, context);
    }
    else if (strcmp(message, "friends") == 0) {
        handleListFriends(client, context);
    }
    else if(strncmp(message, "friend", 6) == 0) {
        char * friendName = message + 7;
        handleAddFriend(client, friendName, context);
    }
    else if(strncmp(message, "unfriend", 8) == 0) {
        char * friendName = message + 9;
        handleUnfriend(client, friendName, context);
    }
    else if(strncmp(message, "privacy", 7) == 0) {
        char* privacy = message + 7;
        handlePrivacy(client, privacy, context);
    }
    else if (strncmp(message, "help", 4) == 0) {
        handleHelp(client);
    }
    else if (strncmp(message, "exit", 4) == 0) {
        handleExit(client, context);
    }
    else if (strncmp(message, "elo", 3) == 0) {
        char eloMessage[BUF_SIZE];
        snprintf(eloMessage, sizeof(eloMessage), "Your ELO rating: %d\n", client->elo);
        write_client(client->sock, eloMessage);
    }
    else if(strncmp(message, "re", 2) == 0){
        char * response = message + 3;
        handleAnswer(client, response, context);
    }
    else {
        handleGameOrChat(client, message, context);
    }
}

// Fonction pour envoyer la liste des clients -- command
void handleSendClientList(Client* client, ServerContext* context) {
    char client_list[BUF_SIZE] = "Connected clients:\n";
    for(int j = 0; j < context->actualClients; j++) {
        strncat(client_list, context->clients[j].name, BUF_SIZE - strlen(client_list) - 1);
        strncat(client_list, "\n", BUF_SIZE - strlen(client_list) - 1);
    }
    write_client(client->sock, client_list);
}

// Fonction pour envoyer la liste des parties en cours -- command
void handleSendGamesList(Client* client, ServerContext* context) {
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

// Fonction pour gérer un défi -- command
void handleChallenge(Client* client, const char* challengedName, ServerContext* context) {
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

// Fonction pour gérer une demande de spectateur -- command
void handleWatchRequest(Client* client, const char* request, ServerContext* context) {
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
            int addResult = addSpectatorToGame(gameSession, client, context);
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

// Fonction pour gérer la déconnexion d'un client -- command
void handleClientDisconnect(int clientIndex, ServerContext* context) {
    Client* client = &context->clients[clientIndex];
    int gameSession = findClientGameSession(client, context);
    if(gameSession != -1) {
        GameSession* session = &context->gameSessions[gameSession];
        Client* otherPlayer = (session->player1 == client) ? session->player2 : session->player1;
        write_client(otherPlayer->sock, "Your opponent disconnected. Game Over!\n");
        session->isActive = 0;
    }
    
    closesocket(client->sock);
    remove_client(context->clients, clientIndex, &context->actualClients);
}

// Fonction pour gérer les messages de jeu ou de chat -- command
void handleGameOrChat(Client* client, const char* message, ServerContext* context) {
    int gameSession = findClientGameSession(client, context);
    if(gameSession != -1 && strncmp(message, "all ", 4) != 0 && strncmp(message, "private ", 8) != 0 && strncmp(message, "accept", 6) != 0 && strncmp(message, "reject", 6) != 0) {
        handleGameMove(gameSession, client, message, context);                         //TODO add something to save all moves in a game to watch it later
    } else {
        handleChatMessage(client, message, context);
    }
}

// Fonction pour gérer les messages de chat -- command
void handleChatMessage(Client* client, const char* message, ServerContext* context) {
    if(strncmp(message, "private ", 8) == 0) {
        handlePrivateMessage(client, message + 8, context);
    } else if(strncmp(message, "all ", 4) == 0) {
        send_message_to_all_clients(context->clients, *client, context->actualClients, message + 4, 0);
    } else if(client->challengedBy != -1) {
        handleChallengeResponse(client, message, context);
    } else if(client->friendRequestBy != 0) {
        handleFriendResponse(client, message, context);
    }
}

// Fonction pour gérer les messages privés -- command
void handlePrivateMessage(Client* client, const char* message, ServerContext* context) {
    // Vérifier si le message est vide
    if (!message || strlen(message) == 0) {
        write_client(client->sock, "Usage: private <client_name> <message> or private friends <message>\n");
        return;
    }

    char buffer[BUF_SIZE];
    strncpy(buffer, message, BUF_SIZE - 1);
    buffer[BUF_SIZE - 1] = '\0';

    // Extraire le nom du destinataire
    char* dest_name = strtok(buffer, " ");
    if (!dest_name) {
        write_client(client->sock, "Usage: private <client_name> <message> or private friends <message>\n");
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


    if (strcmp(dest_name, "friends") == 0) {
        handlePrivateFriendsMessage(client, private_message, context);
    } else {
        // Chercher le destinataire
        int found = 0;
        for (int j = 0; j < context->actualClients; j++) {
            if (strcmp(context->clients[j].name, dest_name) == 0) {
                char formattedMessage[BUF_SIZE];
                client->lastInterlocutorIndex = j;
                context->clients[j].lastInterlocutorIndex = client - context->clients;

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
}


// Fonction pour gérer les messages privés aux amis -- command
void handlePrivateFriendsMessage(Client* client, const char* message, ServerContext* context) {
    if (client->friendCount == 0) {
        write_client(client->sock, "You have no friends to send a message to.\n");
        return;
    }

    char formattedMessage[BUF_SIZE];
    snprintf(formattedMessage, sizeof(formattedMessage), 
            "[Private to friends] %s\n", message);
    write_client(client->sock, formattedMessage);
    char friendMessage[BUF_SIZE];
    snprintf(friendMessage, sizeof(friendMessage), 
            "[Private from %s] %s\n", client->name, message);

    // Parcourir la liste des clients pour trouver les amis
    for (int i = 0; i < client->friendCount; i++) {
        // Pour chaque ami dans la liste d'amis
        for (int j = 0; j < context->actualClients; j++) {
            // Si on trouve un client qui correspond au nom d'ami
            if (strcmp(context->clients[j].name, client->friendList[i]) == 0) {
                write_client(context->clients[j].sock, friendMessage);
                client->lastInterlocutorIndex = j;
                context->clients[j].lastInterlocutorIndex = client - context->clients;
                break;  // Passer à l'ami suivant une fois trouvé
            }
        }
    }
}

// Fonction pour gérer les réponses aux défis --command
void handleChallengeResponse(Client* client, const char* response, ServerContext* context) {
    if(strcmp(response, "accept") == 0) {
        int opponentIndex = client->challengedBy;
        int sessionId = createGameSession(client, &context->clients[opponentIndex], context);
        if(sessionId != -1) {
            startGame(sessionId, client, &context->clients[opponentIndex], context);
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

// Gérer un coup reçu d'un client -- command
void handleGameMove(int sessionId, Client* client, const char* buffer, ServerContext* context) {
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

// Fonction pour faire une sauvegarde d'une partie à un moment donné -- command
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
        write_client(client->sock, "Usage: save game <save_name>\n");
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

// Fonction pour charger une partie sauvegardée -- command
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

// Fonction pour gérer l'écriture ou lecture de biographie -- command
void handleBiography(Client* client, const char* message, ServerContext* context) {
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

// Fonction pour gérer les demandes d'amis -- command 
void handleAddFriend(Client* client, const char* friendName, ServerContext* context) {
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

// Fonction pour gérer les réponses aux demandes d'amis -- command
void handleFriendResponse(Client* client, const char* response, ServerContext* context) {
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

// Fonction pour lister les amis -- command
void handleListFriends(Client* client, ServerContext* context) {
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

// Fonction pour gérer les demandes de suppression d'amis -- command
void handleUnfriend(Client* client, const char* friendName, ServerContext* context) {
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

// Fonction pour gérer la confidentialité -- command
void handlePrivacy(Client* client, const char* message, ServerContext* context) {
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

// Fonction pour gérer le message d'aide -- command
void handleHelp(Client* client) {
    write_client(client->sock, "Available commands:");
    write_client(client->sock, "list: List all connected clients");
    write_client(client->sock, "games: List all active games");
    write_client(client->sock, "challenge <name>: Challenge another client to a game");
    write_client(client->sock, "watch <player1> <player2>: Watch a game between two players");
    write_client(client->sock, "unwatch: Stop watching a game");
    write_client(client->sock, "biography: Read your biography");
    write_client(client->sock, "biography write <biography>: Write your biography");
    write_client(client->sock, "biography read <name>: Read the biography of another client");
    write_client(client->sock, "friends: List your friends");
    write_client(client->sock, "friend <name>: Add a friend");
    write_client(client->sock, "unfriend <name>: Remove a friend");
    write_client(client->sock, "privacy <private/friends/public>: Set your privacy settings");
    write_client(client->sock, "privacy: View your privacy settings");
    write_client(client->sock, "all <message>: Send a message to all clients");
    write_client(client->sock, "private <name> <message>: Send a private message to a client");
    write_client(client->sock, "re <message>: Send a message to the last person you talked to");
    write_client(client->sock, "help: Display this help message");
    write_client(client->sock, "quit: Exit a game");
    write_client(client->sock, "save state <save_name>: Save the state of the game");
    write_client(client->sock, "save game <save_name>: Save the game");
    write_client(client->sock, "load <save_name>: Load a game");
    write_client(client->sock, "list saves: List all saved games");
    write_client(client->sock, "elo: View your ELO rating");
    write_client(client->sock, "exit: Disconnect from the server");
}

// Fonction pour gérer les déconnexions volontaires -- command
void handleExit(Client* client, ServerContext* context) {

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
        int gameSession = findClientGameSession(client, context);
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
    int spectatingSession = findSpectatorGameSession(client, context);
    if (spectatingSession != -1) {
        removeSpectatorFromGame(spectatingSession, client, context);
    }

    // Close socket
    closesocket(client->sock);

    // Remove the client from the client list
    remove_client(context->clients, clientIndex, &context->actualClients);
}

// Fonction pour gérer les nouvelles connexions-- command
void handleNewConnection(ServerContext* context){
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
    if(isNameTaken(c.name, context)) {
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
    c.lastInterlocutorIndex = -1;

    context->clients[context->actualClients] = c;
    context->actualClients++;
}

// Fonction pour démarrer une partie -- not sure
void startGame(int sessionId, Client* client1, Client* client2, ServerContext* context) {
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

// Fonction qui affiche l'elo mis à jour des joueurs -- util
void printUpdatedElo(Client* player1, Client* player2) {
    char message[BUF_SIZE];
    snprintf(message, sizeof(message), "Updated ELO ratings: %s: %d\n", player1->name, player1->elo);
    write_client(player1->sock, message);
    snprintf(message, sizeof(message), "Updated ELO ratings: %s: %d\n", player2->name, player2->elo);
    write_client(player2->sock, message);
}

// Fonction pour gérer la fin d'une partie -- command (should be changed to util)
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

// Fonction qui gère la réponse à un message privé
void handleAnswer(Client* client, const char* response, ServerContext* context) {
    // Vérifier si le message est vide
    if (!response || strlen(response) == 0) {
        write_client(client->sock, "Usage: re <message>\n");
        return;
    }
    
    
    if(client->lastInterlocutorIndex == -1) {
        write_client(client->sock, "No private message to answer to.\n");
        return;
    }
    Client* receiver = &context->clients[client->lastInterlocutorIndex];
    if (receiver->sock == INVALID_SOCKET) {
        write_client(client->sock, "The user is no longer connected.\n");
        return;
    }
    //send it to the receiver
    char formattedMessage[BUF_SIZE];

    // Confirmation au sender
    snprintf(formattedMessage, sizeof(formattedMessage), 
            "[Private to %s] %s\n", receiver->name, response);
    write_client(client->sock, formattedMessage);

    // Envoi du message au destinataire
    snprintf(formattedMessage, sizeof(formattedMessage), 
            "[Private from %s] %s\n", client->name, response);
    write_client(receiver->sock, formattedMessage);
}




