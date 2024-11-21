#ifndef AWALE_H
#define AWALE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// Constantes du jeu
#define HOUSES 6
#define SEEDS 4

// Structure pour représenter un mouvement dans le jeu Awale
typedef struct
{
    // Métadonnées pour chaque mouvement
    time_t timestamp;
    char playerName[64];  // Nom du joueur ayant effectué le mouvement
    int move;             // Le mouvement effectué (par exemple, le numéro du trou d'où les graines ont été prises)
    int board[2][HOUSES];         // Par exemple, l'état du plateau après le mouvement
    int score[2];                 // Scores des joueurs après le mouvement
} AwaleMove;


// Structure pour représenter l'état du jeu Awale
typedef struct {
    int board[2][HOUSES];    // Plateau de jeu
    int score[2];            // Scores des joueurs
    char playerNames[2][50];    // Noms des joueurs
    int currentPlayer;       // Joueur actuel (0 ou 1)
    bool gameOver;           // État de fin de partie
    char message[256];       // Message pour la communication client-serveur
    int turnCount;           // Nombre de tours joués
    int lastMove;           // Dernier coup joué
    int winner;             // Gagnant (-1: pas de gagnant, 0: joueur 1, 1: joueur 2, 2: égalité)
    AwaleMove *moveHistory;  // Pointeur vers un tableau d'historique des mouvements
    size_t moveCount;        // Nombre de mouvements effectués
} AwaleGame;


// Prototypes des fonctions
/**
 * Initialise une nouvelle partie
 * @param game Pointeur vers la structure du jeu à initialiser
 */
void initializeGame(AwaleGame* game, const char* player1Name, const char* player2Name, const int currentPlayer);

/**
 * Vérifie si la partie est terminée
 * @param game Pointeur vers la structure du jeu
 */
void checkGameOver(AwaleGame* game);

/**
 * @brief Verifies if a move is valid in the given game state.
 *
 * This function checks if a move from the source position to the destination position
 * is valid according to the rules of the game. It takes into account the current state
 * of the game board, the positions of the pieces, and the rules governing piece movement.
 *
 * @param game Pointeur vers la structure du jeu
 * @param house Index de la maison choisie (0 à HOUSES-1)
 * @return true if the move is valid, false otherwise.
 */
bool verifyMove(AwaleGame* game, int house);

/**
 * Effectue un coup
 * @param game Pointeur vers la structure du jeu
 * @param house Index de la maison choisie (0 à HOUSES-1)
 * @return true si le coup est valide, false sinon
 */
bool makeMove(AwaleGame* game, int house);

/**
 * Convertit l'état du jeu en chaîne de caractères pour le réseau
 * @param game Pointeur vers la structure du jeu
 * @param buffer Buffer pour stocker la chaîne résultante
 * @param bufferSize Taille du buffer
 */
void serializeGame(const AwaleGame* game, char* buffer, size_t bufferSize);

/**
 * Reconstitue l'état du jeu à partir d'une chaîne de caractères
 * @param game Pointeur vers la structure du jeu
 * @param buffer Chaîne de caractères contenant l'état sérialisé
 */
void deserializeGame(AwaleGame* game, const char* buffer);

/**
 * Affiche l'état actuel du jeu pour les joueurs
 * @param game Pointeur vers la structure du jeu
 * @param playerName Nom du joueur qui affiche
 */
void printGame(const AwaleGame* game, char* playerName);

/**
 * Affiche l'état actuel du jeu pour le viewer
 * @param game Pointeur vers la structure du jeu
 */
void printGameViewer(const AwaleGame* game);

#endif // AWALE_H