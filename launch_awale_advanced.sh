#!/bin/bash

# Configuration par défaut
IP=$(hostname -I | awk '{print $1}')
NB_CLIENTS=2
CLIENT_PREFIX="player"

# Fonction d'aide
show_help() {
    echo "Usage: $0 [-i IP] [-n nb_clients] [-p prefix]"
    echo "  -i : IP address (default: local IP)"
    echo "  -n : Number of clients (default: 2)"
    echo "  -p : Client prefix name (default: player)"
    echo "  -h : Show this help"
}

# Traiter les options
while getopts "i:n:p:h" opt; do
    case $opt in
        i) IP="$OPTARG";;
        n) NB_CLIENTS="$OPTARG";;
        p) CLIENT_PREFIX="$OPTARG";;
        h) show_help; exit 0;;
        ?) show_help; exit 1;;
    esac
done

# Compiler le projet
mkdir -p build
cd build
cmake ..
make

# Lancer le serveur
gnome-terminal --title="Awale Server" -- bash -c "./server; exec bash"

# Attendre que le serveur démarre
sleep 1

# Lancer les clients
for ((i=1; i<=NB_CLIENTS; i++)); do
    gnome-terminal --title="Awale Client ${CLIENT_PREFIX}${i}" -- bash -c "./client $IP ${CLIENT_PREFIX}${i}; exec bash"
    sleep 0.5
done

echo "Lancé avec succès :"
echo "- 1 serveur"
echo "- $NB_CLIENTS clients (${CLIENT_PREFIX}1 à ${CLIENT_PREFIX}${NB_CLIENTS})"
echo "IP utilisée : $IP"