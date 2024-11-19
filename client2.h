#ifndef CLIENT_H
#define CLIENT_H

#include "message.h"

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
   char name[BUF_SIZE];
   int challengedBy;
   int inGameOpponent;
   char biography[BUF_SIZE];
   enum Privacy privacy;
   char friendList[MAX_FRIENDS][BUF_SIZE];
   int friendCount;
}Client;

#endif /* guard */
