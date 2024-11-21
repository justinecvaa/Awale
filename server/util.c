#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "server.h"
#include "client2.h"
#include "awale.h"
#include <sys/select.h>
#include "awale_save.h"
#include "data.h"

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

