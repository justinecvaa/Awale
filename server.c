#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "server.h"
#include "client2.h"
#include "awale.h"
#include <sys/select.h>


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

// Initialisation des sessions de jeu
static void initGameSessions(void) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        gameSessions[i].isActive = 0;
        gameSessions[i].player1 = NULL;
        gameSessions[i].player2 = NULL;
        gameSessions[i].waitingForMove = 0;
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

static void app(void) {
    SOCKET sock = init_connection();
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

            Client c = {csock};
            strncpy(c.name, msg.content, BUF_SIZE - 1);
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
                    else if(strncmp(msg.content, "challenge ", 10) == 0) { // TODO : Changer cela pour que ça ne soit plus bloquant
                        char *challenged_name = msg.content + 10;
                        challenged_name[strcspn(challenged_name, "\n")] = 0;

                        for(int j = 0; j < actual; j++) {
                            if(strcmp(clients[j].name, challenged_name) == 0) {
                                char message[BUF_SIZE];
                                snprintf(message, sizeof(message), "You have been challenged by %s. Do you accept? (yes/no)\n", client->name);
                                write_client(clients[j].sock, message);

                                c = read_client(clients[j].sock, &msg);
                                //buffer[c] = '\0';
                                
                                if(strcmp(msg.content, "yes") == 0) {
                                    int sessionId = createGameSession(client, &clients[j]);
                                    if(sessionId != -1) {
                                        GameSession* session = &gameSessions[sessionId];
                                        char serializedGame[BUF_SIZE];
                                        serializeGame(&session->game, serializedGame, sizeof(serializedGame));
                                        
                                        write_client(client->sock, "Game starting! ");
                                        write_client(clients[j].sock, "Game starting! ");
                                        
                                        if(session->currentPlayerIndex == 0) {
                                            write_client(client->sock, "Your turn!\n");
                                            write_client(clients[j].sock, "Waiting for opponent...\n");
                                        } else {
                                            write_client(clients[j].sock, "Your turn!\n");
                                            write_client(client->sock, "Waiting for opponent...\n");
                                        }
                                        
                                        write_client(client->sock, serializedGame);
                                        write_client(clients[j].sock, serializedGame);
                                    }
                                } else {
                                    write_client(client->sock, "Challenge declined.\n");
                                    write_client(clients[j].sock, "Challenge declined.\n");
                                }
                                break;
                            }
                        }
                    }
                    else { 
                        // Vérifier si le client est dans une partie
                        int gameSession = findClientGameSession(client);
                        if(gameSession != -1) {
                            handleGameMove(gameSession, client, msg.content);
                        }
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
                            } else {
                                // Si ce n'est pas un message privé, on l'envoie à tous les clients
                                send_message_to_all_clients(clients, *client, actual, msg.content, 0);
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