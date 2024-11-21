#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "util.h"
#include "data.h"
#include "../client/client2.h"
#include "../awale/awale.h"
#include "../awale/awale_save.h"
#include "../message.h"


// Fonction d'initialisation du contexte serveur -- util
void initServerContext(ServerContext* context, SOCKET serverSocket) {
    context = malloc(sizeof(ServerContext));
    context->serverSocket = serverSocket;
    context->actualClients = 0;
    context->maxSocket = serverSocket;
    context->isRunning = true;
    initGameSessions(context);
}

// Initialisation des sessions de jeu
static void initGameSessions(ServerContext* context) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        context->gameSessions[i].isActive = 0;
        context->gameSessions[i].player1 = NULL;
        context->gameSessions[i].player2 = NULL;
        context->gameSessions[i].spectatorCount = 0;
        context->gameSessions[i].waitingForMove = 0;
    }
}

// Trouver une session de jeu disponible
static int findFreeGameSession(ServerContext* context) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(!context->gameSessions[i].isActive) {
            return i;
        }
    }
    return -1;
}

// Trouver la session de jeu d'un client
static int findClientGameSession(Client* client, ServerContext* context) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(context->gameSessions[i].isActive && 
           (context->gameSessions[i].player1 == client || context->gameSessions[i].player2 == client)) {
            return i;
        }
    }
    return -1;
}

// Trouver la session de jeu d'un spectateur
static int findSpectatorGameSession(Client* spectator, ServerContext* context) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(context->gameSessions[i].isActive) {
            for(int j = 0; j < context->gameSessions[i].spectatorCount; j++) {
                if(context->gameSessions[i].spectators[j] == spectator) {
                    return i;
                }
            }
        }
    }
    return -1;
}

// Créer une nouvelle partie
static int createGameSession(Client* player1, Client* player2, ServerContext* context) {
    int sessionId = findFreeGameSession(context);
    if(sessionId == -1) return -1;
    
    GameSession* session = &context->gameSessions[sessionId];
    session->isActive = 1;
    session->player1 = player1;
    session->player2 = player2;
    session->currentPlayerIndex = rand() % 2;
    session->waitingForMove = 1;
    session->lastActivity = time(NULL);
    
    initializeGame(&session->game, player1->name, player2->name);
    return sessionId;
}

void checkGameTimeouts(ServerContext* context) {
    time_t currentTime = time(NULL);
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(context->gameSessions[i].isActive) {
            if(currentTime - context->gameSessions[i].lastActivity > 300) {
                GameSession* session = &context->gameSessions[i];
                write_client(session->player1->sock, "Game timed out!\n");
                write_client(session->player2->sock, "Game timed out!\n");
                session->isActive = 0;
                session->player1->inGameOpponent = -1;
                session->player2->inGameOpponent = -1;
            }
        }
    }
}

// Ajouter un spectateur à une session de jeu -- util
static int addSpectatorToGame(int sessionId, Client* spectator, ServerContext* context) {
    GameSession* session = &(context->gameSessions[sessionId]);
    if(session->player1->privacy == PRIVATE || session->player2->privacy == PRIVATE) {
        return 0;  // Pas de spectateurs pour les privées
    }
    else if (session->player1->privacy == FRIENDS || session->player2->privacy == FRIENDS) {
        for (int i = 0; i < session->player1->friendCount; i++) {
            if (strcmp(session->player1->friendList[i], spectator->name) == 0) {
                session->spectators[session->spectatorCount++] = spectator;
                return 1;  // Ajout réussi
            }
        }
        for (int i = 0; i < session->player2->friendCount; i++) {
            if (strcmp(session->player2->friendList[i], spectator->name) == 0) {
                session->spectators[session->spectatorCount++] = spectator;
                return 1;  // Ajout réussi
            }
        }
        return 0;  // Pas d'amis dans la partie
    }
    else {
        if (session->spectatorCount < MAX_SPECTATORS) {
            session->spectators[session->spectatorCount++] = spectator;
            return 1;  // Ajout réussi
        }
    }   
    return 2;  // Pas d'espace pour plus de spectateurs
}

// Supprimer un spectateur de la session de jeu -- util
static void removeSpectatorFromGame(int sessionId, Client* spectator, ServerContext* context) {
    GameSession* session = &(context->gameSessions[sessionId]);
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

// Fonction pour vérifier la dispo d'un nom -- util
static int isNameTaken(const char* name, ServerContext* context) {
    for(int i = 0; i < context->actualClients; i++) {
        if(strcmp(context->clients[i].name, name) == 0) {
            return 1;  // Nom déjà pris
        }
    }
    return 0;  // Nom disponible
}

// Fonction pour mettre à jour l'elo des joueurs -- util
void updateElo(Client* player1, Client* player2, int winner) {
    // Winner is 0 if player 1 wins, 1 if player 2 wins, 2 if it's a tie
    int k = 32;
    double exponent1 = (double)(player2->elo - player1->elo) / 400;
    double expected1 = 1 / (1 + pow(10, exponent1));
    double expected2 = 1 / (1 + pow(10, -exponent1));
    
    double score1, score2;
    if (winner == 0) {
        score1 = 1.0;
        score2 = 0.0;
    } else if (winner == 1) {
        score1 = 0.0;
        score2 = 1.0;
    } else {
        score1 = 0.5;
        score2 = 0.5;
    }
    
    int newElo1 = player1->elo + k * (score1 - expected1);
    int newElo2 = player2->elo + k * (score2 - expected2);
    player1->elo = newElo1;
    player2->elo = newElo2;
}

void remove_client(Client* clients, int indexToRemove, int* actualClients) {
    // Vérifier si l'index est valide
    if (indexToRemove < 0 || indexToRemove >= *actualClients) {
        return;
    }

    // Utiliser une approche de décalage mémoire plus sûre
    memmove(&clients[indexToRemove], &clients[indexToRemove + 1], 
            (*actualClients - indexToRemove - 1) * sizeof(Client));

    // Décrémenter le nombre de clients
    (*actualClients)--;
}

// Convertir l'énumération de confidentialité en chaîne -- util
const char* privacyToString(enum Privacy privacy) {
    switch (privacy) {
        case PRIVATE:
            return "PRIVATE";
        case FRIENDS:
            return "FRIENDS";
        case PUBLIC:
            return "PUBLIC";
        default:
            return "UNKNOWN";
    }
}