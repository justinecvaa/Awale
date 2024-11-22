#include "client_persistence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define CLIENTS_DIR "clients"

// Assure que le répertoire clients existe
static void ensureClientsDirectoryExists(void) {
    struct stat st = {0};
    if (stat(CLIENTS_DIR, &st) == -1) {
        #ifdef _WIN32
            mkdir(CLIENTS_DIR);
        #else
            mkdir(CLIENTS_DIR, 0777);
        #endif
    }
}

bool saveClientData(const Client* client) {
    ensureClientsDirectoryExists();
    
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s/%s.json", CLIENTS_DIR, client->name);
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error: Could not create client file\n");
        return false;
    }

    // Écrire les données au format JSON
    fprintf(file, "{\n");
    fprintf(file, "  \"name\": \"%s\",\n", client->name);
    fprintf(file, "  \"biography\": \"%s\",\n", client->biography);
    fprintf(file, "  \"elo\": %d,\n", client->elo);
    fprintf(file, "  \"privacy\": %d,\n", client->privacy);
    
    // Sauvegarder la liste d'amis
    fprintf(file, "  \"friends\": [\n");
    for (int i = 0; i < client->friendCount; i++) {
        fprintf(file, "    \"%s\"%s\n", 
                client->friendList[i], 
                i < client->friendCount - 1 ? "," : "");
    }
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");

    fclose(file);
    return true;
}

bool loadClientData(const char* name, ClientData* data) {
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s/%s.json", CLIENTS_DIR, name);
    
    FILE* file = fopen(filename, "r");
    if (!file) {
        // Pas d'erreur si le fichier n'existe pas - nouveau client
        return false;
    }

    // Buffer pour lire le fichier
    char buffer[4096];
    size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[bytesRead] = '\0';
    fclose(file);

    // Initialiser les données par défaut
    memset(data, 0, sizeof(ClientData));
    strncpy(data->name, name, BUF_SIZE - 1);
    data->elo = 1000;
    data->privacy = PUBLIC;

    // Parser le JSON manuellement (version simple)
    char* pos = buffer;
    char value[BUF_SIZE];
    
    // Chercher et extraire la biographie
    if ((pos = strstr(buffer, "\"biography\": \"")) != NULL) {
        pos += 13; // Longueur de "biography": ""
        int i = 0;
        while (*pos && *pos != '"' && i < BUF_SIZE - 1) {
            data->biography[i++] = *pos++;
        }
        data->biography[i] = '\0';
    }

    // Chercher et extraire l'elo
    if ((pos = strstr(buffer, "\"elo\": ")) != NULL) {
        data->elo = atoi(pos + 7);
    }

    // Chercher et extraire la privacy
    if ((pos = strstr(buffer, "\"privacy\": ")) != NULL) {
        data->privacy = atoi(pos + 10);
    }

    // Chercher et extraire les amis
    if ((pos = strstr(buffer, "\"friends\": [")) != NULL) {
        pos += 11; // Longueur de "friends": [
        while (*pos && *pos != ']' && data->friendCount < MAX_FRIENDS) {
            while (*pos && (*pos == ' ' || *pos == '\n' || *pos == ',')) pos++;
            if (*pos == '"') {
                pos++; // Sauter le guillemet ouvrant
                int i = 0;
                while (*pos && *pos != '"' && i < BUF_SIZE - 1) {
                    value[i++] = *pos++;
                }
                value[i] = '\0';
                if (strlen(value) > 0) {
                    strncpy(data->friendList[data->friendCount++], value, BUF_SIZE - 1);
                }
            }
        }
    }

    return true;
}

bool saveAllClients(const Client* clients, int clientCount) {
    ensureClientsDirectoryExists();
    bool success = true;
    
    for (int i = 0; i < clientCount; i++) {
        if (!saveClientData(&clients[i])) {
            success = false;
        }
    }
    
    return success;
}