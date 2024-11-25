# Awale Network Game

Un jeu d'Awale multijoueur en réseau avec fonctionnalités sociales et système de classement ELO.

## Description

Ce projet implémente une version réseau du jeu traditionnel Awale (aussi connu sous le nom de Oware ou Mancala). Il permet à plusieurs joueurs de se connecter à un serveur central pour jouer des parties, communiquer entre eux et suivre leur progression.

## Fonctionnalités

### Jeu
- Implémentation complète des règles de l'Awale
- Système de matchmaking avec défis
- Système de classement ELO
- Possibilité de sauvegarder/charger des parties
- Mode spectateur pour observer les parties en cours

### Social
- Liste d'amis
- Messagerie privée
- Système de confidentialité (public/amis/privé)
- Biographies personnalisables
- Chat global

### Gestion
- Système de commandes intuitif
- Liste des joueurs connectés
- Liste des parties en cours
- Sauvegarde des états de jeu
- Sauvegarde des jeux complets
- Chargement d'un état de jeu
- Gestion automatique des déconnexions
- Persistance des clients

## Compilation

### Prérequis
- CMake (version 3.10 minimum)
- Compilateur C (avec support C11)
- Bibliothèques de développement réseau
  - Windows: WinSock2
  - Linux: aucune bibliothèque supplémentaire requise

### Instructions de compilation

1. Créez un dossier de build et positionnez-vous dedans :
```bash
mkdir build
cd build
```

2. Générez les fichiers de build avec CMake :
```bash
cmake ..
```

3. Compilez le projet :
```bash
make
```

Cela générera deux exécutables :
- `server` : le serveur de jeu
- `client` : le client pour se connecter au serveur

## Utilisation

### 1. Lancer manuellement
#### Démarrer le serveur
```bash
./server
```
Le serveur démarre par défaut sur le port 1977.

#### Connecter un client
```bash
./client <adresse_serveur> <pseudo>
```
Exemple :
```bash
./client localhost player1
```

### 2. Lancer via script shell
Un script est fourni pour compiler puis lancer directement le serveur et plusieurs client :
```bash
./launch_awale.sh [-i IP] [-n nb_clients] [-p prefix] [-h]
  -i : IP address (default: local IP)
  -n : Number of clients (default: 2)
  -p : Client prefix name (default: player)
  -h : Show this help
```
Ce script va lancer `nb_clients` clients qui se connectent au server au port `IP`.Leur nom commence par `prefix` et est suivi par un chiffre pour les différencier.

Un script permettant de fermer le serveur et tous les clients est également fourni :
```bash
./stop_game.sh
```

### Commandes disponibles

- `list` : Liste les clients connectés
- `games` : Liste les parties en cours
- `challenge <nom>` : Défie un joueur
- `watch <joueur1> <joueur2>` : Observe une partie
- `unwatch` : Arrête d'observer une partie
- `biography write <texte>` : Définit votre biographie
- `biography read [nom]` : Lit une biographie
- `friends` : Liste vos amis
- `friend <nom>` : Ajoute un ami
- `unfriend <nom>` : Retire un ami
- `privacy <private/friends/public>` : Définit la confidentialité
- `privacy` : Lit votre confidentialité
- `all <message>` : Envoie un message à tous
- `private <nom> <message>` : Envoie un message privé
- `private friends <message>` : Message à tous les amis
- `re <message>` : Répond au dernier message privé
- `elo` : Affiche votre classement
- `help` : Affiche l'aide
- `list saves` : Liste les sauvegardes
- `exit` : Déconnexion du serveur

### Commandes en jeu

- `1-6` : Joue un coup (numéro de la case)
- `save state <nom>` : Sauvegarde l'état actuel
- `save game <nom>` : Sauvegarde la partie
- `load <nom>` : Charge une partie
- `quit` : Quitte une partie

## Structure du projet

```
.
├── awale/          # Logique du jeu
├── client/         # Code du client
├── server/         # Code du serveur
├── CMakeLists.txt  # Configuration CMake
└── message.h       # Définitions communes
```

## Licence

Ce projet est sous licence libre.
