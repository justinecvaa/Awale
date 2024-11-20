#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "server.h"
#include "client2.h"
#include "awale.h"
#include <sys/select.h>

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
    for(int j = 0; j < context->actualClients; j++) {
        if(strcmp(context->clients[j].name, challengedName) == 0) {
            if (context->clients[j].challengedBy != -1) {
                write_client(client->sock, "Client is already challenged by someone else. Wait a bit until they answer\n");
                break;
            }
            if (context->clients[j].inGameOpponent != -1) {
                write_client(client->sock, "Client is already in a game. Wait until they finish\n");
                break;
            }
            
            char confirmationMessage[BUF_SIZE];
            snprintf(confirmationMessage, sizeof(confirmationMessage), "Challenge sent to %s\n", context->clients[j].name);
            write_client(client->sock, confirmationMessage);

            char message[BUF_SIZE];
            snprintf(message, sizeof(message), "You have been challenged by %s. Do you accept? (accept/reject)\n", client->name);
            write_client(context->clients[j].sock, message);

            int clientIndex = client - context->clients;
            client->challengedBy = j;
            context->clients[j].challengedBy = clientIndex;
            break;
        }
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
            if (addSpectatorToGame(gameSession, client)) {
                write_client(client->sock, "You are now watching the game.\n");
                GameSession* session = &context->gameSessions[gameSession];
                char serializedGame[BUF_SIZE];
                serializeGame(&session->game, serializedGame, sizeof(serializedGame));
                write_client(client->sock, serializedGame);
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

// Gérer un coup reçu d'un client
static void handleGameMove(int sessionId, Client* client, const char* buffer) {
    GameSession* session = &context->gameSessions[sessionId];
    
    // Vérifier si c'est bien le tour du joueur
    Client* currentPlayer = (session->currentPlayerIndex == 0) ? session->player1 : session->player2;


    if (strncmp(buffer, "save", 4) == 0) {
        // Sauvegarder la partie
        saveGame(&session->game); //TODO : correct that (maybe handleSaveCommand)
        write_client(client->sock, "Game saved.\n");
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
            write_client(session->player1->sock, "Game Over!\n");
            write_client(session->player2->sock, "Game Over!\n");
            session->isActive = 0;
        }
    }
}

// Fonction pour gérer les messages de jeu ou de chat
void handleGameOrChat(Client* client, const char* message) {
    int gameSession = findClientGameSession(client);
    if(gameSession != -1 && strncmp(message, "all ", 4) != 0 && strncmp(message, "private ", 8) != 0) {
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

// Fonction pour gérer les messages privés
void handlePrivateMessage(Client* client, const char* message) {
    char *dest_name = strtok(strdup(message), " ");
    char *private_message = strtok(NULL, "");

    if (dest_name && private_message) {
        int found = 0;
        for (int j = 0; j < context->actualClients; j++) {
            if (strcmp(context->clients[j].name, dest_name) == 0) {
                char formattedMessage[BUF_SIZE];
                snprintf(formattedMessage, sizeof(formattedMessage), "[Private] %s: %s", client->name, private_message);
                write_client(context->clients[j].sock, formattedMessage);
                found = 1;
                break;
            }
        }
        if (!found) {
            write_client(client->sock, "Client not found.\n");
        }
    } else {
        write_client(client->sock, "Usage: private <client_name> <message>\n");
    }
    free(dest_name);
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
    if (session->spectatorCount < MAX_SPECTATORS) {
        session->spectators[session->spectatorCount++] = spectator;
        return 1;  // Ajout réussi
    }
    return 0;  // Pas d'espace pour plus de spectateurs
}

static void handleBiography(Client* client, const char* message) {
    if (strncmp(message, "write", 5) == 0) {
        strncpy(client->biography, message + 6, BUF_SIZE - 1);
        write_client(client->sock, "Biography updated.\n");
    } else if (strncmp(message, "read", 4) == 0) {

        message += 4;
        if (strlen(message) == 0) {
            write_client(client->sock, "Your biography:\n");
            write_client(client->sock, client->biography);
        } else {
            message++;  // Sauter l'espace
            char * clientName = strtok(strdup(message), "\n");
            char * response;
            for (int i = 0; i < context->actualClients; i++) {
                if (strcmp(context->clients[i].name, message) == 0) {
                    response = "Biography of ";
                    strncat(response, clientName, BUF_SIZE - strlen(response) - 1);
                    strncat(response, ":\n", BUF_SIZE - strlen(response) - 1);
                    strncat(response, context->clients[i].biography, BUF_SIZE - strlen(response) - 1);
                    write_client(client->sock, response);
                    return;
                    //TODO : deviner pourquoi ça marche pas mais read soi meme marche
                    // Ya a peu pres le meme bug a d'autres endroits donc go corriger la bas aussi
                
                }
            }
            write_client(client->sock, "Client not found.\n");
        }
    }
    // TODO : Implement biography
    write_client(client->sock, "Biography not implemented yet.\n");
}

static void addFriend(Client* client, const char* friendName) {
    // Vérifiez si l'ami est déjà dans la liste
    for (int i = 0; i < client->friendCount; i++) {
        if (strcmp(client->friendList[i], friendName) == 0) {
            write_client(client->sock, "Friend already added.\n");
            return;  // Si l'ami est déjà dans la liste, arrêtez la fonction
        }
    }

    for (int j = 0; j < context->actualClients; j++) {
        if (strcmp(context->clients[j].name, friendName) == 0) {
            if (client->friendCount >= MAX_FRIENDS) {
                write_client(client->sock, "Your friend list is full.\n");
                break;
            }
            
            char confirmationMessage[BUF_SIZE];
            snprintf(confirmationMessage, sizeof(confirmationMessage), "Friend request sent to %s\n", context->clients[j].name);
            write_client(client->sock, confirmationMessage);

            char message[BUF_SIZE];
            snprintf(message, sizeof(message), "%s sent you a friend request. Do you accept? (accept/reject)\n", client->name);
            write_client(context->clients[j].sock, message);

            // Enregistrez l'index de l'autre client dans les champs friendRequestBy
            context->clients[j].friendRequestBy = client - context->clients;
            client->friendRequestBy = j;

            break;
        }
    }
}

void friendResponse(Client* client, const char* response) {
    if (strcmp(response, "accept") == 0) {
        // Copie le nom du client qui a envoyé la demande dans la liste d'amis
        strncpy(client->friendList[client->friendCount], context->clients[client->friendRequestBy].name, BUF_SIZE - 1);
        client->friendCount++;
        char confirmationMessage[BUF_SIZE];
        sprintf(confirmationMessage, "Friend %s added to your friend list.\n", context->clients[client->friendRequestBy].name);
        write_client(client->sock, confirmationMessage);

        // Copie le nom du client qui a accepté dans la liste d'amis de l'autre client
        Client *friend = &context->clients[client->friendRequestBy];
        strncpy(friend->friendList[friend->friendCount], client->name, BUF_SIZE - 1);
        friend->friendCount++;

        char confirmationMessage2[BUF_SIZE];
        sprintf(confirmationMessage2, "Friend %s added to your friend list.\n", client->name);
        write_client(friend->sock, confirmationMessage2);
        client->friendRequestBy = -1;
        friend->friendRequestBy = -1;
    }
    else if (strcmp(response, "reject") == 0) {
        write_client(client->sock, "Friend request declined.\n");
        client->friendRequestBy = -1;
        context->clients[client->friendRequestBy].friendRequestBy = -1;
    }
}


static void listFriends(Client* client) {
    write_client(client->sock, "Friends:\n");
    for (int i = 0; i < client->friendCount; i++) {
        write_client(client->sock, client->friendList[i]);
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
    if(isNameTaken(c.name)) {
        write_client(c.sock, "Name already taken. Please choose another name:\n");
        // Lire un nouveau nom du client
        if(read_client(c.sock, &msg) == -1) {   //TODO : faire en sorte que ça ne soit pas bloquant
            return;
        }
        strncpy(c.name, msg.content, BUF_SIZE - 1);
    }
    c.biography[0] = 0;
    c.challengedBy = -1;
    c.inGameOpponent = -1;
    c.friendCount = 0;
    c.friendRequestBy = -1;

    context->clients[context->actualClients] = c;
    context->actualClients++;
}


void processClientMessage(Client* client, const char* message) {
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
        //TODO : implement biography : read others and write your own
    }
    else if (strcmp(message, "friends") == 0) {
        //TODO : implement friends : list friends
        listFriends(client);
        //write_client(client->sock, "Friends not implemented yet.\n");
    }
    else if(strncmp(message, "friend", 6) == 0) {
        // TODO : Implement add and remove friend
        char * friendName = message + 7;
        addFriend(client, friendName);
        //write_client(client->sock, "Friend not implemented yet.\n");
    }
    else if(strncmp(message, "unfriend", 8) == 0) {
        char * friendName = message + 9;
        unfriend(client, friendName);
    }
    else if(strncmp(message, "privacy", 7) == 0) {
        //TODO : implement privacy : set privacy private or public, then change for spectators
        write_client(client->sock, "Privacy not implemented yet.\n");
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

static void remove_client(Client *clients, int to_remove, int *actual) {
    memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
    (*actual)--;
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