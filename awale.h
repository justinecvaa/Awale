#ifndef AWALE_H
#define AWALE_H

#include <stdbool.h>
#include <stddef.h>

// Constantes du jeu
#define HOUSES 6
#define SEEDS 4

// Structure pour représenter l'état du jeu Awale
typedef struct {
    int board[2][HOUSES];    // Plateau de jeu
    int score[2];            // Scores des joueurs
    int currentPlayer;       // Joueur actuel (0 ou 1)
    bool gameOver;           // État de fin de partie
    char message[256];       // Message pour la communication client-serveur
    int lastMove;           // Dernier coup joué
    int winner;             // Gagnant (-1: pas de gagnant, 0: joueur 1, 1: joueur 2, 2: égalité)
} AwaleGame;

// Prototypes des fonctions
/**
 * Initialise une nouvelle partie
 * @param game Pointeur vers la structure du jeu à initialiser
 */
void initializeGame(AwaleGame* game);

/**
 * Vérifie si la partie est terminée
 * @param game Pointeur vers la structure du jeu
 */
void checkGameOver(AwaleGame* game);

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
 * Affiche l'état actuel du jeu
 * @param game Pointeur vers la structure du jeu
 */
void printGame(const AwaleGame* game);

#endif // AWALE_H