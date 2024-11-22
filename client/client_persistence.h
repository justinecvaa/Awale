#ifndef CLIENT_PERSISTENCE_H
#define CLIENT_PERSISTENCE_H

#include "../client/client2.h"
#include <stdbool.h>
#include <limits.h>


// Structure pour stocker les données persistantes d'un client
typedef struct {
    char name[BUF_SIZE];
    char biography[BUF_SIZE];
    char friendList[MAX_FRIENDS][BUF_SIZE];
    int friendCount;
    enum Privacy privacy;
    int elo;
} ClientData;

// Sauvegarder les données d'un client
bool saveClientData(const Client* client);

// Charger les données d'un client
bool loadClientData(const char* name, ClientData* data);

// Sauvegarder tous les clients
bool saveAllClients(const Client* clients, int clientCount);

#endif // CLIENT_PERSISTENCE_H