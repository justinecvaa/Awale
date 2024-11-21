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
    context->serverSocket = serverSocket;
    context->actualClients = 0;
    context->maxSocket = serverSocket;
    context->isRunning = true;
    initGameSessions(context);
}

// Initialisation des sessions de jeu
void initGameSessions(ServerContext* context) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        context->gameSessions[i].isActive = 0;
        context->gameSessions[i].player1 = NULL;
        context->gameSessions[i].player2 = NULL;
        context->gameSessions[i].spectatorCount = 0;
        context->gameSessions[i].waitingForMove = 0;
    }
}

// Trouver une session de jeu disponible
int findFreeGameSession(ServerContext* context) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(!context->gameSessions[i].isActive) {
            return i;
        }
    }
    return -1;
}

// Trouver la session de jeu d'un client
int findClientGameSession(Client* client, ServerContext* context) {
    for(int i = 0; i < MAX_GAME_SESSIONS; i++) {
        if(context->gameSessions[i].isActive && 
           (context->gameSessions[i].player1 == client || context->gameSessions[i].player2 == client)) {
            return i;
        }
    }
    return -1;
}

// Trouver la session de jeu d'un spectateur
int findSpectatorGameSession(Client* spectator, ServerContext* context) {
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
int createGameSession(Client* player1, Client* player2, ServerContext* context) {
    int sessionId = findFreeGameSession(context);
    if(sessionId == -1) return -1;
    
    GameSession* session = &context->gameSessions[sessionId];
    session->isActive = 1;
    session->player1 = player1;
    session->player2 = player2;
    session->currentPlayerIndex = rand() % 2;
    session->waitingForMove = 1;
    session->lastActivity = time(NULL);
    
    initializeGame(&session->game, player1->name, player2->name, session->currentPlayerIndex);
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
int addSpectatorToGame(int sessionId, Client* spectator, ServerContext* context) {
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
void removeSpectatorFromGame(int sessionId, Client* spectator, ServerContext* context) {
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
int isNameTaken(const char* name, ServerContext* context) {
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
    if (winner == 0) { // Player 1 wins
        score1 = 1.0;
        score2 = 0.0;
    } else if (winner == 1) { // Player 2 wins
        score1 = 0.0;
        score2 = 1.0;
    } else { // Tie
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


// SERVER UTILS


int read_client(SOCKET sock, struct message *msg)
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

void write_client(SOCKET sock, const char *buffer)
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

void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server) {
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