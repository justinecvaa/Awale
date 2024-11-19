#ifndef CLIENT_H
#define CLIENT_H

#include "message.h"

typedef struct
{
   SOCKET sock;
   char name[BUF_SIZE];
   int challengedBy;
   int inGameOpponent;
   char biography[BUF_SIZE];
}Client;

#endif /* guard */
