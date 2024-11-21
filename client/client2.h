#ifndef CLIENT_H
#define CLIENT_H

#include "../message.h"

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
#define MAX_FRIENDS 20

enum Privacy
{
   PUBLIC,
   FRIENDS,
   PRIVATE
};


typedef struct
{
   SOCKET sock;
   int validName;
   char name[BUF_SIZE];
   int challengedBy;
   int inGameOpponent;
   char biography[BUF_SIZE];
   enum Privacy privacy;
   char friendList[MAX_FRIENDS][BUF_SIZE];
   int friendCount;
   int friendRequestBy;
   int elo;
   int lastInterlocutorIndex;
}Client;

#endif /* guard */
