#ifndef DATA_H
#define DATA_H


#ifdef WIN32
    #include <winsock2.h>
#elif defined (linux) || defined (__linux__)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/select.h>
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
#include "../client/client2.h"
#include "../awale/awale.h"
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

#endif /* DATA_H */
