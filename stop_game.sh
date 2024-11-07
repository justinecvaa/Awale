#!/bin/bash

# Fonction d'aide
show_help() {
    echo "Usage: $0 [-p prefix]"
    echo " -p : Client prefix name (default: player)"
    echo " -h : Show this help"
}

# Configuration par défaut
CLIENT_PREFIX="player"

# Traiter les options
while getopts "p:h" opt; do
    case $opt in
        p) CLIENT_PREFIX="$OPTARG";;
        h) show_help; exit 0;;
        ?) show_help; exit 1;;
    esac
done

echo "Arrêt des processus en cours..."

# 1. Arrêter les clients d'abord
echo "1. Arrêt des clients..."
# Trouver et arrêter tous les processus client
client_pids=$(ps aux | grep "./client" | grep -v "grep" | awk '{print $2}')
if [ ! -z "$client_pids" ]; then
    echo "Processus clients trouvés : $client_pids"
    for pid in $client_pids; do
        kill -15 $pid 2>/dev/null || kill -9 $pid 2>/dev/null
        echo "Client (PID: $pid) arrêté"
    done
else
    echo "Aucun processus client trouvé"
fi

# 2. Attendre un peu avant d'arrêter le serveur
sleep 1

# 3. Arrêter le serveur
echo "2. Arrêt du serveur..."
server_pid=$(ps aux | grep "./server" | grep -v "grep" | awk '{print $2}')
if [ ! -z "$server_pid" ]; then
    echo "Processus serveur trouvé : $server_pid"
    kill -15 $server_pid 2>/dev/null || kill -9 $server_pid 2>/dev/null
    echo "Serveur (PID: $server_pid) arrêté"
else
    echo "Aucun processus serveur trouvé"
fi

echo "Nettoyage terminé"