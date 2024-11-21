#ifndef SERVER_H
#define SERVER_H


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "../client/client2.h"
#include "../awale/awale.h"
#include "../message.h"
#include "data.h"
#include "command.h"
#include "util.h"

#ifdef WIN32
    #include <winsock2.h>
#elif defined (linux) || defined (__linux__)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
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


static void init(void);
static void end(void);
static void app(void);
static int init_connection(void);
static void end_connection(int sock);
static void clear_clients(Client *clients, int actual);

#endif /* SERVER_H */
