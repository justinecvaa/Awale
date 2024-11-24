// awale_save.c
#include "awale_save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#endif
#include <linux/limits.h>

// Fonction interne pour créer le dossier de sauvegarde
static void ensureSaveDirectoryExists(void)
{
    struct stat st = {0};
    if (stat("saves", &st) == -1)
    {
        mkdir("saves", 0777);
    }
}

// Fonction interne pour générer le fichier JSON
static void writeJsonFile(const char *saveName, const AwaleGame *game, const SaveMetadata *metadata)
{
    char jsonFilename[128];
    snprintf(jsonFilename, sizeof(jsonFilename), "saves/%s.json", saveName);

    FILE *jsonFile = fopen(jsonFilename, "w");
    if (jsonFile)
    {
        fprintf(jsonFile, "{\n");
        fprintf(jsonFile, "  \"metadata\": {\n");
        fprintf(jsonFile, "    \"saveName\": \"%s\",\n", metadata->saveName);
        fprintf(jsonFile, "    \"timestamp\": %ld,\n", metadata->timestamp);
        fprintf(jsonFile, "    \"player1\": \"%s\",\n", metadata->player1Name);
        fprintf(jsonFile, "    \"player2\": \"%s\"\n", metadata->player2Name);
        fprintf(jsonFile, "  },\n");
        fprintf(jsonFile, "  \"gameState\": {\n");
        fprintf(jsonFile, "    \"board\": [\n");
        fprintf(jsonFile, "      [");

        for (int i = 0; i < HOUSES; i++)
        {
            fprintf(jsonFile, "%d%s", game->board[0][i], i < HOUSES - 1 ? ", " : "");
        }

        fprintf(jsonFile, "],\n      [");

        for (int i = 0; i < HOUSES; i++)
        {
            fprintf(jsonFile, "%d%s", game->board[1][i], i < HOUSES - 1 ? ", " : "");
        }

        fprintf(jsonFile, "]\n    ],\n");
        fprintf(jsonFile, "    \"scores\": [%d, %d],\n", game->score[0], game->score[1]);
        fprintf(jsonFile, "    \"currentPlayer\": %d,\n", game->currentPlayer);
        fprintf(jsonFile, "    \"gameOver\": %s,\n", game->gameOver ? "true" : "false");
        fprintf(jsonFile, "    \"lastMove\": %d,\n", game->lastMove);
        fprintf(jsonFile, "    \"winner\": %d\n", game->winner);
        fprintf(jsonFile, "  }\n");
        fprintf(jsonFile, "}\n");

        fclose(jsonFile);
    }
}


// Fonction interne pour générer le fichier JSON
static void writeJsonFileMoves(const char *saveName, const AwaleGame *game, const SaveMetadata *metadata)
{
    char jsonFilename[128];
    snprintf(jsonFilename, sizeof(jsonFilename), "saves/%s.json", saveName);

    FILE *jsonFile = fopen(jsonFilename, "w");
    if (jsonFile)
    {
        // Écriture des métadonnées
        fprintf(jsonFile, "{\n");
        fprintf(jsonFile, "  \"metadata\": {\n");
        fprintf(jsonFile, "    \"saveName\": \"%s\",\n", metadata->saveName);
        fprintf(jsonFile, "    \"timestamp\": %ld,\n", metadata->timestamp);
        fprintf(jsonFile, "    \"player1\": \"%s\",\n", metadata->player1Name);
        fprintf(jsonFile, "    \"player2\": \"%s\"\n", metadata->player2Name);
        fprintf(jsonFile, "  },\n");

        // Écriture des mouvements
        fprintf(jsonFile, "  \"moves\": [\n");
        for (size_t i = 0; i < game->moveCount; i++)
        {
            // Écriture des informations sur le mouvement
            fprintf(jsonFile, "    {\n");
            fprintf(jsonFile, "      \"timeStamp\": %ld,\n", game->moveHistory[i].timestamp);
            fprintf(jsonFile, "      \"player\": \"%s\",\n", game->moveHistory[i].playerName); // playerName est une chaîne, donc il doit être écrit comme un string dans le JSON
            fprintf(jsonFile, "      \"house\": %d,\n", game->moveHistory[i].move);

            // État du jeu après le mouvement
            fprintf(jsonFile, "      \"gameState\": {\n");
            fprintf(jsonFile, "        \"board\": [\n");
            fprintf(jsonFile, "          [");

            // Tableau du joueur 1
            for (int j = 0; j < HOUSES; j++)
            {
                fprintf(jsonFile, "%d%s", game->moveHistory[i].board[0][j], j < HOUSES - 1 ? ", " : "");
            }

            fprintf(jsonFile, "],\n          [");

            // Tableau du joueur 2
            for (int j = 0; j < HOUSES; j++)
            {
                fprintf(jsonFile, "%d%s", game->moveHistory[i].board[1][j], j < HOUSES - 1 ? ", " : "");
            }

            fprintf(jsonFile, "]\n        ],\n");
            fprintf(jsonFile, "        \"scores\": [%d, %d],\n", game->moveHistory[i].score[0], game->moveHistory[i].score[1]);
            fprintf(jsonFile, "        \"gameOver\": %s,\n", game->gameOver ? "true" : "false");
            fprintf(jsonFile, "        \"winner\": %d\n", game->winner);
            fprintf(jsonFile, "      }\n");

            // Si ce n'est pas le dernier mouvement, ajouter une virgule
            fprintf(jsonFile, "    }%s\n", i < game->moveCount - 1 ? "," : "");
        }

        // Fermeture du tableau de mouvements et de l'objet JSON
        fprintf(jsonFile, "  ]\n");
        fprintf(jsonFile, "}\n");

        fclose(jsonFile);
    }
}


bool saveGame(const AwaleGame *game, const char *saveName, const char *player1Name, const char *player2Name)
{
    ensureSaveDirectoryExists();

    AwaleSaveGame saveGame;
    memcpy(&saveGame.game, game, sizeof(AwaleGame));

    // Remplir les métadonnées
    saveGame.metadata.timestamp = time(NULL);
    strncpy(saveGame.metadata.saveName, saveName, sizeof(saveGame.metadata.saveName) - 1);
    strncpy(saveGame.metadata.player1Name, player1Name, sizeof(saveGame.metadata.player1Name) - 1);
    strncpy(saveGame.metadata.player2Name, player2Name, sizeof(saveGame.metadata.player2Name) - 1);
    saveGame.metadata.isCompleteGame = false; // Indique que c'est une sauvegarde d'état

    // Créer le nom du fichier de sauvegarde
    char filename[128];
    snprintf(filename, sizeof(filename), "saves/%s.awale", saveName);

    // Sauvegarder le fichier binaire
    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        printf("Error: Could not create save file\n");
        return false;
    }

    size_t written = fwrite(&saveGame, sizeof(AwaleSaveGame), 1, file);
    fclose(file);

    if (written != 1)
    {
        printf("Error: Failed to write save file\n");
        return false;
    }

    // Générer le fichier JSON
    writeJsonFile(saveName, game, &saveGame.metadata);

    printf("Game saved successfully as '%s'\n", saveName);
    return true;
}

bool saveCompleteGame(const AwaleGame *game, const char *saveName, const char *player1Name, const char *player2Name, const AwaleMove *moves, size_t moveCount)
{
    ensureSaveDirectoryExists();

    // Créer la structure de sauvegarde
    AwaleSaveGame saveGame;
    
    // Sauvegarder l'état actuel du jeu
    memcpy(&saveGame.game, game, sizeof(AwaleGame));

    // Sauvegarder l'historique des mouvements
    saveGame.moveCount = moveCount;
    for (size_t i = 0; i < moveCount; i++)
    {
        saveGame.moves[i] = moves[i];
    }

    // Remplir les métadonnées
    saveGame.metadata.timestamp = time(NULL);
    strncpy(saveGame.metadata.saveName, saveName, sizeof(saveGame.metadata.saveName) - 1);
    strncpy(saveGame.metadata.player1Name, player1Name, sizeof(saveGame.metadata.player1Name) - 1);
    strncpy(saveGame.metadata.player2Name, player2Name, sizeof(saveGame.metadata.player2Name) - 1);
    saveGame.metadata.isCompleteGame = true; // Indique que c'est une sauvegarde complète

    // Créer le nom du fichier de sauvegarde
    char filename[128];
    snprintf(filename, sizeof(filename), "saves/%s.awale", saveName);

    // Sauvegarder le fichier binaire
    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        printf("Error: Could not create save file\n");
        return false;
    }

    size_t written = fwrite(&saveGame, sizeof(AwaleSaveGame), 1, file);
    fclose(file);

    if (written != 1)
    {
        printf("Error: Failed to write save file\n");
        return false;
    }

    // Générer le fichier JSON
    writeJsonFileMoves(saveName, game, &saveGame.metadata);

    printf("Game and history saved successfully as '%s'\n", saveName);
    return true;
}

bool loadGame(AwaleGame *game, const char *saveName, char *player1Name, char *player2Name, bool *wasSwapped) {
    printf("FileName: %s\n", saveName);
    char filename[128];
    snprintf(filename, sizeof(filename), "saves/%s.awale", saveName);
    
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Save file not found\n");
        return false;
    }
    
    AwaleSaveGame saveGame;
    size_t read = fread(&saveGame, sizeof(AwaleSaveGame), 1, file);
    fclose(file);
    
    if (read != 1) {
        printf("Error: Failed to read save file\n");
        return false;
    }
    
    if (saveGame.metadata.isCompleteGame) {
        printf("Error: The save file '%s' is a complete game, not a game state\n", saveName);
        return false;
    }
    
    bool player1Found = (strcmp(player1Name, saveGame.metadata.player1Name) == 0) || 
                       (strcmp(player1Name, saveGame.metadata.player2Name) == 0);
    bool player2Found = (strcmp(player2Name, saveGame.metadata.player1Name) == 0) || 
                       (strcmp(player2Name, saveGame.metadata.player2Name) == 0);
    
    if (!player1Found || !player2Found) {
        printf("Error: One or both players are not found in the save file\n");
        return false;
    }
    
    // Copie l'état du jeu
    memcpy(game, &saveGame.game, sizeof(AwaleGame));
    
    // Détermine si les joueurs sont dans un ordre différent de la sauvegarde
    bool needSwap = (strcmp(player1Name, saveGame.metadata.player2Name) == 0);
    
   
    if (needSwap) {
    printf("Swapping players\n");
    // Échange les noms des joueurs
    strcpy(game->playerNames[0], player2Name);
    strcpy(game->playerNames[1], player1Name);
    
    // NE PAS échanger les scores, ils doivent rester associés à leur position sur le plateau
    // int tempScore = game->score[0];
    // game->score[0] = game->score[1];
    // game->score[1] = tempScore;
    
    // Inverse le joueur courant car les positions sont inversées
    game->currentPlayer = 1 - game->currentPlayer;
    
    } else {
        strcpy(game->playerNames[0], player1Name);
        strcpy(game->playerNames[1], player2Name);
    }
    
    // On garde toujours le plateau dans la même orientation que la sauvegarde originale
    memcpy(game->board, saveGame.game.board, sizeof(game->board));
    
    *wasSwapped = needSwap;
    game->moveCount = 0;
    game->moveHistory = malloc(MAX_MOVES * sizeof(AwaleMove));
    
    printf("Game '%s' loaded successfully\n", saveName);
    printf("Current player to play: %s\n", game->playerNames[game->currentPlayer]);
    return true;
}

// Modified SaveMetadata loading function
SaveMetadata* listSaves(int* nbSaves) {
    ensureSaveDirectoryExists();
    *nbSaves = 0;
    SaveMetadata* savesList = NULL;
    int currentSize = 0;

#ifdef _WIN32
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile("saves\\*.awale", &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                currentSize++;
                SaveMetadata* temp = realloc(savesList, currentSize * sizeof(SaveMetadata));
                if (temp == NULL) {
                    free(savesList);
                    FindClose(hFind);
                    return NULL;
                }
                savesList = temp;
                SaveMetadata* currentSave = &savesList[currentSize - 1];
                
                // Initialize all fields to 0/empty
                memset(currentSave, 0, sizeof(SaveMetadata));
                
                // Copy filename without extension
                char saveName[64] = {0};
                strncpy(saveName, findData.cFileName, sizeof(saveName) - 1);
                char* ext = strstr(saveName, ".awale");
                if (ext) *ext = '\0';
                
                strncpy(currentSave->saveName, saveName, sizeof(currentSave->saveName) - 1);
                
                // Read the binary save file
                char filename[128];
                snprintf(filename, sizeof(filename), "saves\\%s.awale", saveName);
                FILE* file = fopen(filename, "rb");
                
                if (file) {
                    AwaleSaveGame saveGame;
                    fread(&saveGame, sizeof(AwaleSaveGame), 1, file);
                    fclose(file);

                    // Extract metadata from the read save game
                    currentSave->timestamp = saveGame.metadata.timestamp;
                    strncpy(currentSave->player1Name, saveGame.metadata.player1Name, sizeof(currentSave->player1Name) - 1);
                    strncpy(currentSave->player2Name, saveGame.metadata.player2Name, sizeof(currentSave->player2Name) - 1);
                }
            }
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir("saves");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".awale")) {
                currentSize++;
                SaveMetadata* temp = realloc(savesList, currentSize * sizeof(SaveMetadata));
                if (temp == NULL) {
                    free(savesList);
                    closedir(dir);
                    return NULL;
                }
                savesList = temp;
                SaveMetadata* currentSave = &savesList[currentSize - 1];
                
                // Initialize all fields to 0/empty
                memset(currentSave, 0, sizeof(SaveMetadata));
                
                // Copy filename without extension
                char saveName[64] = {0};
                strncpy(saveName, entry->d_name, sizeof(saveName) - 1);
                char* ext = strstr(saveName, ".awale");
                if (ext) *ext = '\0';
                
                strncpy(currentSave->saveName, saveName, sizeof(currentSave->saveName) - 1);
                
                // Read the binary save file
                char filename[128];
                snprintf(filename, sizeof(filename), "saves/%s.awale", saveName);
                FILE* file = fopen(filename, "rb");
                
                if (file) {
                    AwaleSaveGame saveGame;
                    fread(&saveGame, sizeof(AwaleSaveGame), 1, file);
                    fclose(file);

                    // Extract metadata from the read save game
                    currentSave->timestamp = saveGame.metadata.timestamp;
                    strncpy(currentSave->player1Name, saveGame.metadata.player1Name, sizeof(currentSave->player1Name) - 1);
                    strncpy(currentSave->player2Name, saveGame.metadata.player2Name, sizeof(currentSave->player2Name) - 1);
                }
            }
        }
        closedir(dir);
    }
#endif

    *nbSaves = currentSize;
    return savesList;
}