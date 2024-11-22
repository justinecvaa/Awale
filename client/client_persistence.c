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
    // Vérification des pointeurs
    printf("Name pointer: %p\n", (void*)name);
    printf("Data pointer: %p\n", (void*)data);
    
    if (!data) {
        printf("Error: data pointer is NULL\n");
        return false;
    }
    if (!name) {
        printf("Error: name pointer is NULL\n");
        return false;
    }

    char filename[MAX_PATH_SIZE];
    snprintf(filename, sizeof(filename), "%s/%s.json", CLIENTS_DIR, name);
        printf("Loading client data from %s\n", filename);
    
    // Avant d'ouvrir le fichier, vérifions la structure
    printf("Size of ClientData: %zu\n", sizeof(ClientData));
    FILE* file = fopen(filename, "r");
    if (!file) {
        // Pas d'erreur si le fichier n'existe pas - nouveau client
        printf("Client data not found\n");
        return false;
    }
    printf("Client data found\n");
    // Buffer pour lire le fichier
    char buffer[4096];
    size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[bytesRead] = '\0';
    fclose(file);
    printf("Buffer: %s\n", buffer);


printf("Attempting to access data structure...\n");
    volatile char* test = (volatile char*)data;
    printf("Can access data pointer\n");
    
    // Test d'accès à chaque membre
    printf("Testing name access...\n");
    data->name[0] = '\0';
    printf("Name initialized\n");
    
    printf("Testing biography access...\n");
    data->biography[0] = '\0';
    printf("Biography initialized\n");

    printf("Testing friendCount access...\n");
    data->friendCount = 0;
    printf("Friend count initialized\n");

    printf("Testing elo access...\n");
    data->elo = 1000;
    printf("Elo initialized\n");

    printf("Testing privacy access...\n");
    data->privacy = PUBLIC;  // Assurez-vous que PUBLIC est bien défini dans l'enum
    printf("Privacy initialized\n");

    printf("Testing friendList access...\n");
    for (int i = 0; i < MAX_FRIENDS; i++) {
        data->friendList[i][0] = '\0';
    }
    printf("Friend list initialized\n");


    // Parser le JSON manuellement (version simple)
    char* pos = buffer;
    char value[BUF_SIZE];
    
    printf("ça marche1\n");
    // Chercher et extraire la biographie
    if ((pos = strstr(buffer, "\"biography\": \"")) != NULL) {
        pos += 13; // Longueur de "biography": ""
        int i = 0;
        while (*pos && *pos != '"' && i < BUF_SIZE - 1) {
            data->biography[i++] = *pos++;
        }
        data->biography[i] = '\0';
    }

    printf("ça marche2\n");
    // Chercher et extraire l'elo
    if ((pos = strstr(buffer, "\"elo\": ")) != NULL) {
        data->elo = atoi(pos + 7);
    }

    printf("ça marche3\n");
    // Chercher et extraire la privacy
    if ((pos = strstr(buffer, "\"privacy\": ")) != NULL) {
        data->privacy = atoi(pos + 10);
    }

    printf("ça marche4\n");
    // Chercher et extraire les amis
    
    if ((pos = strstr(buffer, "\"friends\": [")) != NULL) {
        while(*pos != ']') {
            pos++;
            printf("%c\n", *pos);
        }
    }

if ((pos = strstr(buffer, "\"friends\": [")) != NULL) {
    pos += 11; // Longueur de "friends": [
    while (*pos && *pos != ']' && data->friendCount < MAX_FRIENDS) {
        // Sauter les espaces, retours à la ligne et virgules
        while (*pos && (*pos == ' ' || *pos == '\n' || *pos == ',')) 
            pos++;
            
        if (*pos == '"') {
            pos++; // Sauter le guillemet ouvrant
            int i = 0;
            while (*pos && *pos != '"' && i < BUF_SIZE - 1) {
                value[i++] = *pos++;
            }
            if (*pos == '"') pos++; // Sauter le guillemet fermant
            value[i] = '\0';
            
            if (strlen(value) > 0) {
                strncpy(data->friendList[data->friendCount++], value, BUF_SIZE - 1);
            }
        } else {
            pos++; // Important : avancer le pointeur même si ce n'est pas un guillemet
        }
    }
}
    printf("ça marche5\n");
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