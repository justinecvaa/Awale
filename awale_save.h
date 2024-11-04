// awale_save.h
#ifndef AWALE_SAVE_H
#define AWALE_SAVE_H

#include <stdbool.h>
#include <time.h>
#include "awale.h"  // Inclut la définition de AwaleGame

// Structure pour les métadonnées de la sauvegarde
typedef struct {
    time_t timestamp;          // Horodatage de la sauvegarde
    char saveName[64];         // Nom de la sauvegarde
    char player1Name[32];      // Nom du joueur 1
    char player2Name[32];      // Nom du joueur 2
} SaveMetadata;

// Structure complète de sauvegarde
typedef struct {
    AwaleGame game;           // État du jeu
    SaveMetadata metadata;    // Métadonnées
} AwaleSaveGame;

// Fonctions publiques pour la gestion des sauvegardes
bool saveGame(const AwaleGame* game, const char* saveName, const char* player1Name, const char* player2Name);
bool loadGame(AwaleGame* game, const char* saveName, char* player1Name, char* player2Name);
void listSaves(void);
void handleSaveCommand(AwaleGame* game);
void handleLoadCommand(AwaleGame* game);

#endif // AWALE_SAVE_H