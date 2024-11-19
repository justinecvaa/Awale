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
static GameSession gameSessions[MAX_GAME_SESSIONS];


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

// Vérifie si le nom est déjà pris par un autre client
static int isNameTaken(const char* name, Client* clients, int actual) {
    for(int i = 0; i < actual; i++) {
        if(strcmp(clients[i].name, name) == 0) {
            return 1;  // Nom déjà pris
        }
    }
    return 0;  // Nom disponible
}

// Initialize server context
void initServerContext(SOCKET serverSocket) {
    context->serverSocket = serverSocket;
    context->actualClients = 0;
    context->maxSocket = 0;
    context->lastCleanupTime = time(NULL);
    context->isRunning = true;
    
    // Initialize game sessions
    initGameSessions();
}

void closeServer() {
    // Notify all clients of server shutdown
    for (int i = 0; i < context->actualClients; i++) {
        write_client(context->clients[i].sock, "Server is shutting down...\n");
    }
    
    // Close server socket first
    if (context->serverSocket != INVALID_SOCKET) {
        closesocket(context->serverSocket);
        context->serverSocket = INVALID_SOCKET;
    }
    
    // Give clients a moment to receive shutdown message
    sleep(1);
    
    // Then close client connections
    for (int i = 0; i < context->actualClients; i++) {
        closesocket(context->clients[i].sock);
    }
    
    context->isRunning = false;
}


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

// Trouver une session de jeu disponible
static int findFreeGameSession(void) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(!gameSessions[i].isActive) {
            return i;
        }
    }
    return -1;
}

// Trouver la session de jeu d'un client
static int findClientGameSession(Client* client) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(gameSessions[i].isActive && 
           (gameSessions[i].player1 == client || gameSessions[i].player2 == client)) {
            return i;
        }
    }
    return -1;
}

// Trouver la session de jeu d'un spectateur
static int findSpectatorGameSession(Client* spectator) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(gameSessions[i].isActive) {
            for(int j = 0; j < gameSessions[i].spectatorCount; j++) {
                if(gameSessions[i].spectators[j] == spectator) {
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
    
    GameSession* session = &gameSessions[sessionId];
    session->isActive = 1;
    session->player1 = player1;
    session->player2 = player2;
    session->currentPlayerIndex = rand() % 2;
    session->waitingForMove = 1;
    session->lastActivity = time(NULL);
    
    initializeGame(&session->game, player1->name, player2->name);
    return sessionId;
}

// Gérer un coup reçu d'un client
static void handleGameMove(int sessionId, Client* client, const char* buffer) {
    GameSession* session = &gameSessions[sessionId];
    
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



// Ajouter un spectateur à une session de jeu
static int addSpectatorToGame(int sessionId, Client* spectator) {
    GameSession* session = &gameSessions[sessionId];
    if (session->spectatorCount < MAX_SPECTATORS) {
        session->spectators[session->spectatorCount++] = spectator;
        return 1;  // Ajout réussi
    }
    return 0;  // Pas d'espace pour plus de spectateurs
}

// Supprimer un spectateur de la session de jeu
static void removeSpectatorFromGame(int sessionId, Client* spectator) {
    GameSession* session = &gameSessions[sessionId];
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



static void app(void) {
    SOCKET sock = init_connection();
    initServerContext(sock);
    struct message msg; 
    int actual = 0;
    int max = sock;
    Client clients[MAX_CLIENTS];
    fd_set rdfs;
    
    initGameSessions();

    while(1) {
        FD_ZERO(&rdfs);
        FD_SET(STDIN_FILENO, &rdfs);
        FD_SET(sock, &rdfs);

        for(int i = 0; i < actual; i++) {
            FD_SET(clients[i].sock, &rdfs);
            max = clients[i].sock > max ? clients[i].sock : max;
        }

        if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1) {
            perror("select()");
            exit(errno);
        }

        if(FD_ISSET(STDIN_FILENO, &rdfs)) {
            break;
        }
        else if(FD_ISSET(sock, &rdfs)) {
            SOCKADDR_IN csin = {0};
            socklen_t sinsize = sizeof csin;
            int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
            if(csock == SOCKET_ERROR) {
                perror("accept()");
                continue;
            }

            if(read_client(csock, &msg) == -1) {
                continue;
            }

            /* after connecting the client sends its name */
            max = csock > max ? csock : max;

            // Demander et vérifier si le nom est déjà pris
            Client c = {csock};
            strncpy(c.name, msg.content, BUF_SIZE - 1);

            // Vérification du nom
            if(isNameTaken(c.name, clients, actual)) {
                write_client(c.sock, "Name already taken. Please choose another name:\n");
                // Lire un nouveau nom du client
                if(read_client(c.sock, &msg) == -1) {   //TODO : faire en sorte que ça ne soit pas bloquant
                    continue;
                }
                strncpy(c.name, msg.content, BUF_SIZE - 1);
            }
            c.biography[0] = 0;
            c.challengedBy = -1;
            c.inGameOpponent = -1;

            clients[actual] = c;
            actual++;
        }
        else { 
            for(int i = 0; i < actual; i++) {
                if(FD_ISSET(clients[i].sock, &rdfs)) {
                    Client* client = &clients[i];
                    int c = read_client(client->sock, &msg);
                    
                    if(c == 0) {
                        // Déconnexion du client
                        int gameSession = findClientGameSession(client);
                        if(gameSession != -1) {
                            // Informer l'autre joueur et terminer la partie
                            GameSession* session = &gameSessions[gameSession];
                            Client* otherPlayer = (session->player1 == client) ? 
                                                 session->player2 : session->player1;
                            write_client(otherPlayer->sock, "Your opponent disconnected. Game Over!\n");
                            session->isActive = 0;
                        }
                        
                        closesocket(clients[i].sock);
                        remove_client(clients, i, &actual);
                        continue;
                    }

                    if(strcmp(msg.content, "list") == 0) {
                        char client_list[BUF_SIZE] = "Connected clients:\n";
                        for(int j = 0; j < actual; j++) {
                            strncat(client_list, clients[j].name, BUF_SIZE - strlen(client_list) - 1);
                            strncat(client_list, "\n", BUF_SIZE - strlen(client_list) - 1);
                        }
                        write_client(client->sock, client_list);
                    }
                    else if (strcmp(msg.content, "games") == 0) {
                        char game_list[BUF_SIZE] = "Active games:\n";
                        for (int j = 0; j < MAX_GAME_SESSIONS; j++) {
                            if (gameSessions[j].isActive) {
                                strncat(game_list, gameSessions[j].player1->name, BUF_SIZE - strlen(game_list) - 1);
                                strncat(game_list, " vs ", BUF_SIZE - strlen(game_list) - 1);
                                strncat(game_list, gameSessions[j].player2->name, BUF_SIZE - strlen(game_list) - 1);
                                strncat(game_list, "\n", BUF_SIZE - strlen(game_list) - 1);
                            }
                        }
                        write_client(client->sock, game_list);
                    }
                    else if(strncmp(msg.content, "challenge ", 10) == 0) { 
                        char *challenged_name = msg.content + 10;
                        challenged_name[strcspn(challenged_name, "\n")] = 0;

                        for(int j = 0; j < actual; j++) {
                            if(strcmp(clients[j].name, challenged_name) == 0) {
                                if (clients[j].challengedBy != -1) {
                                    write_client(client->sock, "Client is already challenged by someone else. Wait a bit until he answers\n");
                                    break;
                                }
                                if (clients[j].inGameOpponent != -1) {
                                    write_client(client->sock, "Client is already in a game. Wait until he finishes\n");
                                    break;
                                }
                                char confirmationMessage[BUF_SIZE];
                                snprintf(confirmationMessage, sizeof(confirmationMessage), "Challenge sent to %s\n", clients[j].name);
                                write_client(client->sock, confirmationMessage);

                                char message[BUF_SIZE];
                                snprintf(message, sizeof(message), "You have been challenged by %s. Do you accept? (accept/reject)\n", client->name);
                                write_client(clients[j].sock, message);


                                client->challengedBy = j;
                                clients[j].challengedBy = i;                                           
                                break;
                            }
                        }
                    }
                    else { 
                        // Vérifier si le client est dans une partie
                        int gameSession = findClientGameSession(client);
                        if(gameSession != -1 && strncmp(msg.content, "all ", 4) != 0 && strncmp(msg.content, "private ", 8) != 0) {
                            handleGameMove(gameSession, client, msg.content);
                        }
                        //TODO add something to save all moves in a game to watch it later
                        else {
                            // Vérification si c'est un message privé
                            if(strncmp(msg.content, "private ", 8) == 0) {
                                // Extraire le destinataire et le message
                                char *message_start = msg.content + 8;
                                char *dest_name = strtok(message_start, " ");
                                char *private_message = strtok(NULL, "");

                                if (dest_name && private_message) {
                                    int found = 0;
                                    for (int j = 0; j < actual; j++) {
                                        if (strcmp(clients[j].name, dest_name) == 0) {
                                            // Créer le message privé et l'envoyer sur une seule ligne
                                            char message[BUF_SIZE];
                                            snprintf(message, sizeof(message), "[Private] %s: %s", client->name, private_message);
                                            
                                            write_client(clients[j].sock, message);
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
                            } else if (strncmp(msg.content, "all ", 4) == 0) {
                                // Si ce n'est pas un message privé, on l'envoie à tous les clients
                                char *message_start = msg.content + 4;
                                send_message_to_all_clients(clients, *client, actual, message_start, 0);
                            } else if(strncmp(msg.content, "watch ", 6) == 0) {
                                char *watchRequest = msg.content + 6;
                                char *player1_name = strtok(watchRequest, " ");
                                char *player2_name = strtok(NULL, "");

                                if (player1_name && player2_name) {
                                    int gameSession = -1;

                                    // Chercher une session de jeu correspondant aux deux joueurs
                                    for (int j = 0; j < MAX_GAME_SESSIONS; j++) {
                                        if (gameSessions[j].isActive) {
                                            if ((strcmp(gameSessions[j].player1->name, player1_name) == 0 && 
                                                strcmp(gameSessions[j].player2->name, player2_name) == 0) ||
                                                (strcmp(gameSessions[j].player1->name, player2_name) == 0 && 
                                                strcmp(gameSessions[j].player2->name, player1_name) == 0)) {
                                                gameSession = j;
                                                break;
                                            }
                                        }
                                    }

                                    if (gameSession != -1) {
                                        if (addSpectatorToGame(gameSession, client)) {
                                            write_client(client->sock, "You are now watching the game.\n");

                                            // Envoyer l'état actuel de la partie au spectateur
                                            GameSession* session = &gameSessions[gameSession];
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
                            } else if (strncmp(msg.content, "unwatch", 7) == 0) {
                                int gameSession = findSpectatorGameSession(client);
                                if (gameSession != -1) {
                                    removeSpectatorFromGame(gameSession, client);
                                    write_client(client->sock, "You are no longer watching the game.\n");
                                } else {
                                    write_client(client->sock, "You are not watching any game.\n");
                                }
                            } else if(strncmp(msg.content, "biography", 9) == 0){
                                //TODO : implement biography : read others and write your own
                                write_client(client->sock, "Biography not implemented yet.\n");
                            } // TODO : Implement add and remove friend
                            else if (strncmp(msg.content, "friend", 6) == 0) {
                                write_client(client->sock, "Friend not implemented yet.\n");
                            } else if (strncmp(msg.content, "unfriend", 8) == 0) {
                                write_client(client->sock, "Unfriend not implemented yet.\n");
                            } else if(strncmp(msg.content, "privacy", 7) == 0){
                                //TODO : implement privacy : set privacy private or public, then change for spectators
                                write_client(client->sock, "Privacy not implemented yet.\n");
                            } else if (client->challengedBy != -1){ // Let it always be the last one, or right before game
                                if(strcmp(msg.content, "accept") == 0){
                                    int opponentIndex = client->challengedBy;
                                    int sessionId = createGameSession(client, &clients[opponentIndex]);
                                    if(sessionId != -1) {
                                        GameSession* session = &gameSessions[sessionId];
                                        char serializedGame[BUF_SIZE];
                                        serializeGame(&session->game, serializedGame, sizeof(serializedGame));
                                        
                                        write_client(client->sock, "Game starting! ");
                                        write_client(clients[opponentIndex].sock, "Game starting! ");
                                        
                                        if(session->currentPlayerIndex == 0) {
                                            write_client(client->sock, "Your turn!\n");
                                            write_client(clients[opponentIndex].sock, "Waiting for opponent...\n");
                                        } else {
                                            write_client(clients[opponentIndex].sock, "Your turn!\n");
                                            write_client(client->sock, "Waiting for opponent...\n");
                                        }
                                        
                                        write_client(client->sock, serializedGame);
                                        write_client(clients[opponentIndex].sock, serializedGame);
                                    }
                                    client->challengedBy = -1;
                                    clients[opponentIndex].challengedBy = -1;

                                    client->inGameOpponent = opponentIndex;
                                    clients[opponentIndex].inGameOpponent = i;
                                    break;
                                }
                                else if(strcmp(msg.content, "reject") == 0){
                                    write_client(clients[client->challengedBy].sock, "Challenge declined.\n");
                                    client->challengedBy = -1;
                                    clients[client->challengedBy].challengedBy = -1;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Vérifier les timeouts des parties
        time_t currentTime = time(NULL);
        for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
            if(gameSessions[i].isActive) {
                if(currentTime - gameSessions[i].lastActivity > 300) { // 5 minutes timeout
                    write_client(gameSessions[i].player1->sock, "Game timed out!\n");
                    write_client(gameSessions[i].player2->sock, "Game timed out!\n");
                    gameSessions[i].isActive = 0;

                    gameSessions[i].player1->inGameOpponent = -1;
                    gameSessions[i].player2->inGameOpponent = -1;
                }
            }
        }
    }

    clear_clients(clients, actual);
    end_connection(sock);
}

static void clear_clients(Client *clients, int actual) {
    for(int i = 0; i < actual; i++) {
        closesocket(clients[i].sock);
    }
}

static void remove_client(Client *clients, int to_remove, int *actual) {
    memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
    (*actual)--;
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

int main(int argc, char **argv) {
    init();
    app();
    end();
    return EXIT_SUCCESS;
}