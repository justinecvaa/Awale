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
    
    char filename[MAX_PATH_SIZE];
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
    char filename[MAX_PATH_SIZE];
    snprintf(filename, sizeof(filename), "%s/%s.json", CLIENTS_DIR, name);
    
    FILE* file = fopen(filename, "r");
    if (!file) return false;

    // Lire tout le fichier
    char buffer[4096];
    size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[bytesRead] = '\0';
    fclose(file);

    // Initialiser la structure
    memset(data, 0, sizeof(ClientData));
    strncpy(data->name, name, BUF_SIZE - 1);

    // Parser le JSON
    char* pos = buffer;
    char* line;
    char value[BUF_SIZE];

    // Biographie
    if ((pos = strstr(buffer, "\"biography\": \"")) != NULL) {
        pos += strlen("\"biography\": \"");
        int i = 0;
        while (*pos && *pos != '"' && i < BUF_SIZE - 1) {
            data->biography[i++] = *pos++;
        }
        data->biography[i] = '\0';
    }

    // ELO
    if ((pos = strstr(buffer, "\"elo\": ")) != NULL) {
        sscanf(pos + strlen("\"elo\": "), "%d", &data->elo);
    }

    // Privacy
    if ((pos = strstr(buffer, "\"privacy\": ")) != NULL) {
        sscanf(pos + strlen("\"privacy\": "), "%d", &data->privacy);
    }

    // Liste d'amis
    if ((pos = strstr(buffer, "\"friends\": [")) != NULL) {
    pos += strlen("\"friends\": [");
    char* end = strstr(pos, "]");
    if (!end) return false;
    
    data->friendCount = 0;
    
    while (pos < end && data->friendCount < MAX_FRIENDS) {
        // Trouver le prochain guillemet ouvrant
        pos = strchr(pos, '"');
        if (!pos || pos >= end) break;
        pos++; // Sauter le guillemet
        
        // Lire jusqu'au guillemet fermant
        char* quote = strchr(pos, '"');
        if (!quote || quote >= end) break;
        
        // Copier le nom de l'ami
        size_t len = quote - pos;
        if (len > 0 && len < BUF_SIZE) {
            strncpy(data->friendList[data->friendCount], pos, len);
            data->friendList[data->friendCount][len] = '\0';
            data->friendCount++;
        }
        
        pos = quote + 1;
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