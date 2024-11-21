// awale_save.c
#include "awale_save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <dirent.h>
#include <unistd.h>
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


bool loadGame(AwaleGame *game, const char *saveName, char *player1Name, char *player2Name)
{
    printf("FileName: %s\n", saveName);
    char filename[128];
    snprintf(filename, sizeof(filename), "saves/%s.awale", saveName);

    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        printf("Error: Save file not found\n");
        return false;
    }

    AwaleSaveGame saveGame;
    size_t read = fread(&saveGame, sizeof(AwaleSaveGame), 1, file);
    fclose(file);

    if (read != 1)
    {
        printf("Error: Failed to read save file\n");
        return false;
    }

    // Copier les données
    memcpy(game, &saveGame.game, sizeof(AwaleGame));

    if (player1Name)
    {
        strcpy(player1Name, saveGame.metadata.player1Name);
    }
    if (player2Name)
    {
        strcpy(player2Name, saveGame.metadata.player2Name);
    }

    printf("Game '%s' loaded successfully\n", saveName);
    return true;
}

void listSaves(void)
{
    printf("\nAvailable saves:\n");
    printf("----------------\n");

#ifdef _WIN32
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile("saves\\*.awale", &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                char saveName[64];
                strncpy(saveName, findData.cFileName, sizeof(saveName) - 1);
                char *ext = strstr(saveName, ".awale");
                if (ext)
                    *ext = '\0';

                AwaleSaveGame saveGame;
                char filename[128];
                snprintf(filename, sizeof(filename), "saves/%s.awale", saveName);

                FILE *file = fopen(filename, "rb");
                if (file)
                {
                    fread(&saveGame, sizeof(AwaleSaveGame), 1, file);
                    fclose(file);

                    char timeStr[26];
                    ctime_s(timeStr, sizeof(timeStr), &saveGame.metadata.timestamp);
                    timeStr[24] = '\0';

                    printf("- %s\n", saveName);
                    printf("  Players: %s vs %s\n",
                           saveGame.metadata.player1Name,
                           saveGame.metadata.player2Name);
                    printf("  Date: %s\n\n", timeStr);
                }
            }
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
#else
    DIR *dir = opendir("saves");
    if (dir)
    {
        printf("Directory opened\n");
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strstr(entry->d_name, ".awale"))
            {
                printf("Found save file\n");
                char saveName[64];
                strncpy(saveName, entry->d_name, sizeof(saveName) - 1);
                char *ext = strstr(saveName, ".awale");
                if (ext)
                    *ext = '\0';

                AwaleSaveGame saveGame;
                char filename[PATH_MAX];
                int written = snprintf(filename, sizeof(filename), "saves/%s", entry->d_name);

                if (written < 0 || written >= sizeof(filename))
                {
                    fprintf(stderr, "Error: Filename too long: %s\n", entry->d_name);
                    continue; // ou return avec un code d'erreur approprié
                }

                FILE *file = fopen(filename, "rb");
                if (file)
                {
                    fread(&saveGame, sizeof(AwaleSaveGame), 1, file);
                    fclose(file);

                    char timeStr[26];
                    ctime_r(&saveGame.metadata.timestamp, timeStr);
                    timeStr[24] = '\0';

                    printf("- Nom : %s\n", saveName);
                    printf("  Players: %s vs %s\n",
                           saveGame.metadata.player1Name,
                           saveGame.metadata.player2Name);
                    printf("  Date: %s\n\n", timeStr);
                }
            }
        }
        closedir(dir);
    }
#endif
}
