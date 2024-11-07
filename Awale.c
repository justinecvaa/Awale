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
    
    game->gameOver = (game->score[0] > 24 || game->score[1] > 24);

    //Verifier si un joueur est en famine (aucune graine et l'autre joueur ne peut pas le nourrir)
    bool famine1 = false, famine2 = false;
    if ((sum1 == 0 || sum2 == 0) && !(sum1 == 0 && sum2 == 0)) {
        if (sum1 == 0) {
            famine1 = true;
            for (int i = 0; i < HOUSES; i++) { // On verifie si une des cases du joueur 2 peut nourrir le joueur 1
                if (game->board[1][i] >= HOUSES - i) {
                    famine1 = false;
                    break;
                }
                
            }
        } else {
            famine2 = true;
            for (int i = 0; i < HOUSES; i++) {
                if (game->board[0][i] >= HOUSES - i) {
                    famine2 = false;
                    break;
                }
            }
        }
    }
    if (famine1 || famine2) { // Si un joueur est en famine, la partie est terminée et on rajoute toutes les graines au score de l'autre joueur
        if (famine1) { //Si le joueur 1 est en famine, on donne toutes les graines restantes au joueur 2
            for (int i = 0; i < HOUSES; i++) {
                game->score[1] += game->board[1][i];
                game->board[1][i] = 0;
            }
        } else { //Si le joueur 2 est en famine, on donne toutes les graines restantes au joueur 1
            for (int i = 0; i < HOUSES; i++) {
                game->score[0] += game->board[0][i];
                game->board[0][i] = 0;
            }
        }
        game->gameOver = true;
    }
    
    if (game->gameOver) {
        
        // Déterminer le gagnant
        if (game->score[0] > game->score[1]) {
            game->winner = 0;
            sprintf(game->message, "Player 1 wins %swith %d points", famine2 ? "by famine " : "", game->score[0]);
        } else if (game->score[1] > game->score[0]) {
            game->winner = 1;
            sprintf(game->message, "Player 2 wins %swith %d points!", famine1 ? "by famine " : "", game->score[1]);
        } else {
            game->winner = 2;
            sprintf(game->message, "It's a tie with %d points each!", game->score[0]);
        }
    }
}


bool verifyMove(AwaleGame* game, int house) {
    int opponentSeeds = 0;
    for (int i = 0; i < HOUSES; i++) {
        opponentSeeds += game->board[1 - game->currentPlayer][i];
    }
    if (opponentSeeds == 0) { // Si l'adversaire n'a pas de graines, le joueur doit le nourrir si possible
        if (game->board[game->currentPlayer][house] < HOUSES - house) { // Vérifier si le joueur peut nourrir l'adversaire
            strcpy(game->message, "You must feed the opponent");
            return false;
        }
    }
    if (house < 0 || house >= HOUSES || game->board[game->currentPlayer][house] == 0) {
        strcpy(game->message, "Invalid move");
        return false;
    }
    return true;
}
// Effectue un coup
bool makeMove(AwaleGame* game, int house) {
    // Vérifier si le coup est valide

    if (!verifyMove(game, house)) {
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

    bool allSeeds = false;
    //Si le coup doit prendre toutes les graines adverses, il ne prend rien
    if(currentHouse == 5 && currentPlayer != game->currentPlayer){ //Si la derniere graine tombe dans la maison 5 adverse, on verifie combien de cases on prend après
        allSeeds = true;
        while (allSeeds && currentHouse >= 0) {
            if (game->board[currentPlayer][currentHouse] != 2 && game->board[currentPlayer][currentHouse] != 3) { //Si on trouve une case qu'on ne peut pas prendre, on part en prise normale
                allSeeds = false;
            }
            currentHouse--;
        }
    }


    // Capture des graines
    while (!allSeeds && currentPlayer != game->currentPlayer && 
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
    printf("\nOpponent: ");
    for (int i = HOUSES - 1; i >= 0; i--) {
        printf("%2d ", game->board[1 - game->currentPlayer][i]);
    }
    printf(" | Score: %d", game->score[1 - game->currentPlayer]);
    printf("\n         ");
    for (int i = 0; i < HOUSES; i++) {
        printf("   ");
    }
    printf("\nYou: ");
    for (int i = 0; i < HOUSES; i++) {
        printf("%2d ", game->board[game->currentPlayer][i]);
    }
    printf(" | Score: %d", game->score[game->currentPlayer]);
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