#include <stdio.h>
#include <stdbool.h>

#define HOUSES 6
#define SEEDS 4

void initializeBoard(int board[2][HOUSES]) {
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < HOUSES; j++) {
            board[i][j] = SEEDS;
        }
    }
}

void printBoard(int board[2][HOUSES], int score[2]) {
    printf("Player 2: ");
    for (int i = HOUSES - 1; i >= 0; i--) {
        printf("%2d ", board[1][i]);
    }
    printf(" | Score: %d", score[1]);
    printf("\n ");
    for (int i = 0; i < HOUSES; i++) {
        printf(" ");
    }
    printf("\nPlayer 1: ");
    for (int i = 0; i < HOUSES; i++) {
        printf("%2d ", board[0][i]);
    }
    printf(" | Score: %d", score[0]);
    printf("\n");
}

bool isGameOver(int board[2][HOUSES]) {
    int sum1 = 0, sum2 = 0;
    for (int i = 0; i < HOUSES; i++) {
        sum1 += board[0][i];
        sum2 += board[1][i];
    }
    return sum1 == 0 || sum2 == 0;
}

void makeMove(int board[2][HOUSES], int score[2], int player, int house) {
    int seeds = board[player][house];
    board[player][house] = 0;
    int currentHouse = house;
    int currentPlayer = player;
    
    while (seeds > 0) {
        currentHouse++;
        if (currentHouse == HOUSES) {
            currentHouse = 0;
            currentPlayer = 1 - currentPlayer;
        }
        if (currentPlayer != player || currentHouse != house) {
            board[currentPlayer][currentHouse]++;
            seeds--;
        }
    }
    
    while (currentPlayer != player && (board[currentPlayer][currentHouse] == 2 || board[currentPlayer][currentHouse] == 3)) {
        score[player] += board[currentPlayer][currentHouse];
        board[currentPlayer][currentHouse] = 0;
        currentHouse--;
        if (currentHouse < 0) {
            currentHouse = HOUSES - 1;
            currentPlayer = 1 - currentPlayer;
        }
    }
}

int main() {
    int board[2][HOUSES];
    initializeBoard(board);
    int score[2] = {0, 0};
    int currentPlayer = 0;
    char input[100];
    int c;
    
    while (!isGameOver(board)) {
        printBoard(board, score);
        int house;
        bool validMove = false;
        
        while (!validMove) {
            printf("Player %d, choose a house (1-%d): ", currentPlayer + 1, HOUSES);
            
            // Ne vider le buffer que s'il y a des caractères en attente
            if (scanf("%d", &house) != 1) {
                // Vider le buffer seulement si la lecture a échoué
                while ((c = getchar()) != '\n' && c != EOF);
                printf("Please enter a valid number.\n");
                continue;
            }
            
            house--; // Convert to 0-based index
            if (house >= 0 && house < HOUSES && board[currentPlayer][house] > 0) {
                validMove = true;
                // Vider le caractère newline restant
                while ((c = getchar()) != '\n' && c != EOF);
            } else {
                printf("Invalid move. Try again.\n");
                // Vider le caractère newline restant
                while ((c = getchar()) != '\n' && c != EOF);
            }
        }
        
        makeMove(board, score, currentPlayer, house);
        currentPlayer = 1 - currentPlayer;
    }
    
    printf("Game over!\n");
    printBoard(board, score);
    
    // Afficher le gagnant
    if (score[0] > score[1]) {
        printf("Player 1 wins with %d points!\n", score[0]);
    } else if (score[1] > score[0]) {
        printf("Player 2 wins with %d points!\n", score[1]);
    } else {
        printf("It's a tie with %d points each!\n", score[0]);
    }
    
    return 0;
}