#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "../client/client2.h"
#include "../awale/awale.h"
#include "../message.h"
#include "data.h"

// Server context initialization
void initServerContext(ServerContext* context, SOCKET serverSocket);
void initGameSessions(ServerContext* context);

// Game related utilities
int findFreeGameSession(ServerContext* context);
int findClientGameSession(Client* client, ServerContext* context);
int findSpectatorGameSession(Client* spectator, ServerContext* context);
int createGameSession(Client* player1, Client* player2, ServerContext* context);
void checkGameTimeouts(ServerContext* context);
int addSpectatorToGame(int sessionId, Client* spectator, ServerContext* context);
void removeSpectatorFromGame(int sessionId, Client* spectator, ServerContext* context);

// Client management utilities
int isNameTaken(const char* name, ServerContext* context);
void updateElo(Client* player1, Client* player2, int winner);
void remove_client(Client *clients, int to_remove, int *actual);
const char* privacyToString(enum Privacy privacy);


#endif
