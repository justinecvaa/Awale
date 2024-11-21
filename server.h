#ifndef SERVER_H
#define SERVER_H

#ifdef WIN32
    #include <winsock2.h>
#elif defined (linux) || defined (__linux__)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h> /* close */
    #include <netdb.h> /* gethostbyname */
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket(s) close(s)
    typedef int SOCKET;
    typedef struct sockaddr_in SOCKADDR_IN;
    typedef struct sockaddr SOCKADDR;
    typedef struct in_addr IN_ADDR;
#else
    #error not defined for this platform
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "client2.h"
#include "awale.h"
#include "message.h"

#define CRLF        "\r\n"
#define PORT        1977
#define MAX_CLIENTS 100
#define MAX_GAME_SESSIONS 50
#define MAX_SPECTATORS 10  // Limite du nombre de spectateurs par partie

// Structure pour gérer l'état d'une partie en cours
typedef struct {
    int isActive;
    Client* player1;
    Client* player2;
    Client* spectators[MAX_SPECTATORS];  // Tableau des spectateurs
    int spectatorCount;                  // Nombre actuel de spectateurs
    AwaleGame game;
    int currentPlayerIndex;  // 0 pour player1, 1 pour player2
    int waitingForMove;      // 1 si en attente d'un coup
    time_t lastActivity;     // Pour gérer le timeout
} GameSession;

// Structure pour gérer le contexte du serveur
typedef struct {
    SOCKET serverSocket;
    Client clients[MAX_CLIENTS];
    int actualClients;
    GameSession gameSessions[MAX_GAME_SESSIONS];
    fd_set readfs;
    int maxSocket;
    time_t lastCleanupTime;
    bool isRunning;
} ServerContext;

// Function declarations
static void init(void);
static void end(void);
static void app(void);
static int init_connection(void);
static void end_connection(int sock);
static int read_client(SOCKET sock, struct message *msg);
static void write_client(SOCKET sock, const char *buffer);
static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server);
static void remove_client(Client *clients, int to_remove, int *actual); 
static void clear_clients(Client *clients, int actual);

// Game-related function declarations
static void initGameSessions(void);
static int findFreeGameSession(void);
static int findClientGameSession(Client* client);
static int findSpectatorGameSession(Client* spectator);
static int createGameSession(Client* player1, Client* player2);
static void handleGameMove(int sessionId, Client* client, const char* buffer);
static int isNameTaken(const char *name);
static void updateElo(Client* player1, Client* player2, int winner);
static void handleNewConnection();
static void processClientMessage(Client* client, const char* message);
static void handleChallenge(Client* client, const char* challengedName);
static void handleWatchRequest(Client* client, const char* request);
static void handleClientDisconnect(int clientIndex);
static void handleGameOrChat(Client* client, const char* message);
static void handleChatMessage(Client* client, const char* message);
static void handlePrivateMessage(Client* client, const char* message);
static void handleChallengeResponse(Client* client, const char* response);
static void handleBiography(Client* client, const char* message);
static void startGame(int sessionId, Client* client1, Client* client2);
static void checkGameTimeouts(void);
static void removeSpectatorFromGame(int sessionId, Client* spectator);
static int addSpectatorToGame(int sessionId, Client* spectator);
const char* privacyToString(enum Privacy privacy);
static void addFriend(Client* client, const char* friendName);
static void friendResponse(Client* client, const char* response);
static void listFriends(Client* client);
static void unfriend(Client* client, const char* friendName);
static void handlePrivacy(Client* client, const char* privacy);
static void handleHelp(Client* client);
static void handleSaveStateCommand(Client* client, const char* message, AwaleGame *game); 
void handleExit(Client* client);

#endif /* SERVER_H */
