#include "awale.h"
#include "awale_save.h"
#include <stdio.h>
#include <string.h>

// Initialise une nouvelle partie
void initializeGame(AwaleGame* game, const char* player1Name, const char* player2Name, const int currentPlayer) {
    // Initialiser le plateau
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < HOUSES; j++) {
            game->board[i][j] = SEEDS;
        }
    }
    
    // Initialiser les autres variables
    game->score[0] = 0;
    game->score[1] = 0;
    game->currentPlayer = 1 - currentPlayer;
    game->gameOver = false;
    game->lastMove = -1;
    game->winner = -1;
    game->turnCount = 0;
    game->moveHistory = malloc(MAX_MOVES * sizeof(AwaleMove));;
    game->moveCount = 0;
    strcpy(game->playerNames[0], player1Name);
    strcpy(game->playerNames[1], player2Name);
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
// Effectue un coup
bool makeMove(AwaleGame *game, int house) {

    if (!verifyMove(game, house)) {
        return false;
    }


    if (game == NULL) {
        printf("Debug - Error: game pointer is NULL\n");
        return false;
    }

    // Vérification des limites
    if (house < 0 || house >= 6) {
        printf("Debug - Error: house index out of bounds: %d\n", house);
        return false;
    }

    // Déterminer la rangée du joueur actuel et de l'adversaire
    int currentRow = game->currentPlayer;
    int otherRow = 1 - currentRow;

    // Vérifier si la case est vide
    if (game->board[currentRow][house] == 0) {
        printf("Debug - Error: selected house is empty\n");
        return false;
    }

    // Sauvegarder le nombre de graines
    int seeds = game->board[currentRow][house];
    game->board[currentRow][house] = 0;
    printf("Debug - Player %d selected house %d with %d seeds\n", currentRow + 1, house, seeds);

    // Position actuelle
    int currentHouse = house;
    int currentSide = currentRow;

    // Distribuer les graines
    while (seeds > 0) {
        // Avancer à la prochaine case
        currentHouse++;
        if (currentHouse >= 6) {
            currentHouse = 0;
            currentSide = 1 - currentSide;
        }

        // Ne pas semer dans la case de départ
        if (currentSide == currentRow && currentHouse == house) {
            continue;
        }

        // Semer une graine
        game->board[currentSide][currentHouse]++;
        seeds--;
        printf("Debug - Seed placed in house %d on side %d, remaining seeds: %d\n", currentHouse, currentSide, seeds);
    }

    // Capture des graines
    while (currentSide == otherRow && 
           (game->board[currentSide][currentHouse] == 2 || 
            game->board[currentSide][currentHouse] == 3)) {
        
        // Capturer les graines
        game->score[currentRow] += game->board[currentSide][currentHouse];
        printf("Debug - Capturing %d seeds from house %d on side %d\n", game->board[currentSide][currentHouse], currentHouse, currentSide);
        game->board[currentSide][currentHouse] = 0;

        // Reculer d'une case
        currentHouse--;
        if (currentHouse < 0) {
            currentHouse = 5; // Retourner à la dernière case
            currentSide = 1 - currentSide; // Changer de côté
        }
    }

    char message[256];
    snprintf(message, sizeof(message), "%s played house %d", game->playerNames[game->currentPlayer], house + 1);
    strcpy(game->message, message);
    printf("Debug - %s\n", message);

    // Vérifier si le jeu est terminé
    bool gameEnded = true;
    for (int i = 0; i < 6; i++) {
        if (game->board[0][i] > 0 || game->board[1][i] > 0) {
            gameEnded = false;
            break;
        }
    }

    if (gameEnded) {
        game->gameOver = true;
        // Déterminer le gagnant
        if (game->score[0] > game->score[1]) {
            game->winner = 0;
        } else if (game->score[1] > game->score[0]) {
            game->winner = 1;
        } else {
            game->winner = -1; // Match nul
        }
        printf("Debug - Game over. Winner: %d\n", game->winner);
    }

    // Mise à jour du joueur courant après vérification
    if (!game->gameOver) {
        game->currentPlayer = 1 - game->currentPlayer;
        printf("Debug - Next player: %d\n", game->currentPlayer + 1);
    }

    // Mettre à jour l'histoire des mouvements
    if (game->moveCount < MAX_MOVES) {
        AwaleMove* move = &game->moveHistory[game->moveCount];
        move->timestamp = time(NULL);
        // Utiliser le bon index de joueur pour le nom
        strcpy(move->playerName, game->playerNames[currentRow]); // currentRow au lieu de game->currentPlayer
        move->move = house;
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < HOUSES; j++) {
                move->board[i][j] = game->board[i][j];
            }
        }
        move->score[0] = game->score[0];
        move->score[1] = game->score[1];
        game->moveCount++;
        printf("Debug - Move recorded. Total moves: %d\n", game->moveCount);
    }

    game->lastMove = house;

    return true;
}


// Fonction pour convertir l'état du jeu en chaîne de caractères pour le réseau
void serializeGame(const AwaleGame* game, char* buffer, size_t bufferSize) {
    snprintf (buffer, bufferSize,
        "game:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%ld,%s,%s,%s",
        game->board[0][0], game->board[0][1], game->board[0][2],
        game->board[0][3], game->board[0][4], game->board[0][5],
        game->board[1][0], game->board[1][1], game->board[1][2],
        game->board[1][3], game->board[1][4], game->board[1][5],
        game->score[0], game->score[1],
        game->currentPlayer, game->gameOver,
        game->lastMove, game->winner,
        strlen(game->message), game->message, 
        game->playerNames[0], game->playerNames[1]);
}

// Fonction pour reconstituer l'état du jeu à partir d'une chaîne de caractères
void deserializeGame(AwaleGame* game, const char* buffer) {
    char prefix[10];
    int messageLen;
    int tempGameOver; // Variable temporaire pour stocker la valeur booléenne

    sscanf(buffer, "%5[^:]:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%[^,],%[^,],%[^\n]",
        prefix,
        &game->board[0][0], &game->board[0][1], &game->board[0][2],
        &game->board[0][3], &game->board[0][4], &game->board[0][5],
        &game->board[1][0], &game->board[1][1], &game->board[1][2],
        &game->board[1][3], &game->board[1][4], &game->board[1][5],
        &game->score[0], &game->score[1],
        &game->currentPlayer, &tempGameOver,  // Utiliser la variable temporaire
        &game->lastMove, &game->winner,
        &messageLen, game->message, 
        game->playerNames[0], game->playerNames[1]);

    game->gameOver = (tempGameOver != 0); // Convertir l'entier en booléen
}
// Fonction d'affichage
void printGame(const AwaleGame* game, char* playerName) {
    int currentPlayer;
    //printf("\nName given in funtion%s \n: ", playerName);
    //printf("\nFirst Name in game%s \n Second Name in game %s\n: ", game->playerNames[0], game->playerNames[1]);
    if(strcmp(playerName, game->playerNames[0]) == 0){
        currentPlayer = 0;
    } else {
        currentPlayer = 1;
    }
    //printf("Joueur actuel: %d\n", currentPlayer);

    printf("\n%s: ", game->playerNames[1 - currentPlayer]);
    for (int i = HOUSES - 1; i >= 0; i--) {
        printf("%2d ", game->board[currentPlayer][i]);
    }
    printf(" | Score: %d", game->score[currentPlayer]);
    printf("\n         ");
    for (int i = 0; i < HOUSES; i++) {
        printf("   ");
    }
    printf("\n%s: ", game->playerNames[currentPlayer]);
    for (int i = 0; i < HOUSES; i++) {
        printf("%2d ", game->board[1 - currentPlayer][i]);
    }
    printf(" | Score: %d", game->score[1 - currentPlayer]);
    printf("\nMessage: %s\n", game->message);
}

// Fonction d'affichage Viewer
void printGameViewer(const AwaleGame* game) {
    printf("\n%s: ", game->playerNames[1]);
    for (int i = HOUSES - 1; i >= 0; i--) {
        printf("%2d ", game->board[1][i]);
    }
    printf(" | Score: %d", game->score[1]);
    printf("\n         ");
    for (int i = 0; i < HOUSES; i++) {
        printf("   ");
    }
    printf("\n%s: ", game->playerNames[0]);
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