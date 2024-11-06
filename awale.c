#include "awale.h"
#include "awale_save.h"
#include <stdio.h>
#include <string.h>

// Initialise une nouvelle partie
void initializeGame(AwaleGame* game) {
    // Initialiser le plateau
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < HOUSES; j++) {
            game->board[i][j] = SEEDS;
        }
    }
    
    // Initialiser les autres variables
    game->score[0] = 0;
    game->score[1] = 0;
    game->currentPlayer = 0;
    game->gameOver = false;
    game->lastMove = -1;
    game->winner = -1;
    strcpy(game->message, "Game started");
}

// Vérifie si la partie est terminée
void checkGameOver(AwaleGame* game) {
    int sum1 = 0, sum2 = 0;
    for (int i = 0; i < HOUSES; i++) {
        sum1 += game->board[0][i];
        sum2 += game->board[1][i];
    }
    
    game->gameOver = (sum1 == 0 || sum2 == 0);
    
    if (game->gameOver) {
        // Ajouter les graines restantes aux scores
        game->score[0] += sum1;
        game->score[1] += sum2;
        
        // Déterminer le gagnant
        if (game->score[0] > game->score[1]) {
            game->winner = 0;
            sprintf(game->message, "Player 1 wins with %d points!", game->score[0]);
        } else if (game->score[1] > game->score[0]) {
            game->winner = 1;
            sprintf(game->message, "Player 2 wins with %d points!", game->score[1]);
        } else {
            game->winner = 2;
            sprintf(game->message, "It's a tie with %d points each!", game->score[0]);
        }
    }
}

// Effectue un coup
bool makeMove(AwaleGame* game, int house) {
    // Vérifier si le coup est valide
    if (house < 0 || house >= HOUSES || game->board[game->currentPlayer][house] == 0) {
        strcpy(game->message, "Invalid move");
        return false;
    }

    int seeds = game->board[game->currentPlayer][house];
    game->board[game->currentPlayer][house] = 0;
    int currentHouse = house;
    int currentPlayer = game->currentPlayer;

    // Distribution des graines
    while (seeds > 0) {
        currentHouse++;
        if (currentHouse == HOUSES) {
            currentHouse = 0;
            currentPlayer = 1 - currentPlayer;
        }
        if (currentPlayer != game->currentPlayer || currentHouse != house) {
            game->board[currentPlayer][currentHouse]++;
            seeds--;
        }
    }

    // Capture des graines
    while (currentPlayer != game->currentPlayer && 
           (game->board[currentPlayer][currentHouse] == 2 || 
            game->board[currentPlayer][currentHouse] == 3)) {
        game->score[game->currentPlayer] += game->board[currentPlayer][currentHouse];
        game->board[currentPlayer][currentHouse] = 0;
        currentHouse--;
        if (currentHouse < 0) {
            currentHouse = HOUSES - 1;
            currentPlayer = 1 - currentPlayer;
        }
    }

    game->lastMove = house;
    sprintf(game->message, "Player %d played house %d", game->currentPlayer + 1, house + 1);
    game->currentPlayer = 1 - game->currentPlayer;
    
    checkGameOver(game);
    return true;
}

// Fonction pour convertir l'état du jeu en chaîne de caractères pour le réseau
void serializeGame(const AwaleGame* game, char* buffer, size_t bufferSize) {
    snprintf (buffer, bufferSize,
        "game:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%ld,%s",
        game->board[0][0], game->board[0][1], game->board[0][2],
        game->board[0][3], game->board[0][4], game->board[0][5],
        game->board[1][0], game->board[1][1], game->board[1][2],
        game->board[1][3], game->board[1][4], game->board[1][5],
        game->score[0], game->score[1],
        game->currentPlayer, game->gameOver,
        game->lastMove, game->winner,
        strlen(game->message), game->message);
}

// Fonction pour reconstituer l'état du jeu à partir d'une chaîne de caractères
void deserializeGame(AwaleGame* game, const char* buffer) {
    char prefix[10];
    int messageLen;
    int tempGameOver; // Variable temporaire pour stocker la valeur booléenne

    sscanf(buffer, "%5[^:]:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%[^\n]",
        prefix,
        &game->board[0][0], &game->board[0][1], &game->board[0][2],
        &game->board[0][3], &game->board[0][4], &game->board[0][5],
        &game->board[1][0], &game->board[1][1], &game->board[1][2],
        &game->board[1][3], &game->board[1][4], &game->board[1][5],
        &game->score[0], &game->score[1],
        &game->currentPlayer, &tempGameOver,  // Utiliser la variable temporaire
        &game->lastMove, &game->winner,
        &messageLen, game->message);

    game->gameOver = (tempGameOver != 0); // Convertir l'entier en booléen
}
// Fonction d'affichage
void printGame(const AwaleGame* game) {
    printf("\nPlayer 2: ");
    for (int i = HOUSES - 1; i >= 0; i--) {
        printf("%2d ", game->board[1][i]);
    }
    printf(" | Score: %d", game->score[1]);
    printf("\n         ");
    for (int i = 0; i < HOUSES; i++) {
        printf("   ");
    }
    printf("\nPlayer 1: ");
    for (int i = 0; i < HOUSES; i++) {
        printf("%2d ", game->board[0][i]);
    }
    printf(" | Score: %d", game->score[0]);
    printf("\nMessage: %s\n", game->message);
}

// Fonction principale
/*int main() {
    AwaleGame game;

    printf("1. New Game\n");
    printf("2. Load Game\n");
    printf("Choice: ");

    int choice;
    scanf("%d", &choice);
    while (getchar() != '\n');

    if (choice == 2) {
        handleLoadCommand(&game);
    }
    if (choice == 1) {
    initializeGame(&game);
    }
    char buffer[1024];
    char input[20];

    while (!game.gameOver) {
        printGame(&game);
        int house;
        printf("Player %d, enter a house number (1-%d) or 'save' to save the game: ", 
               game.currentPlayer + 1, HOUSES);

        if (scanf("%19s", input) == 1) {
            // Vérifier si c'est une commande de sauvegarde
            if (strcmp(input, "save") == 0) {
                handleSaveCommand(&game);
                continue;
            } else {
                // Convertir l'entrée en nombre
                house = atoi(input);
                if (house >= 1 && house <= HOUSES) {
                    if (makeMove(&game, house - 1)) {
                        // Sérialisation normale pour le réseau
                        serializeGame(&game, buffer, sizeof(buffer));
                        printf("\nSerialized game state: %s\n", buffer);
                        
                        // Simuler l'envoi/réception réseau
                        AwaleGame receivedGame;
                        deserializeGame(&receivedGame, buffer);
                    }
                } else {
                    printf("Invalid input. Please enter a number between 1 and %d or 'save'.\n", HOUSES);
                }
            }
        }
        while (getchar() != '\n'); // Vider le buffer
    }
    
    printGame(&game);
    return 0;
}*/