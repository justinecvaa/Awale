# Version minimale requise de CMake
cmake_minimum_required(VERSION 3.10)
# Nom du projet
project(Awale C)
# Définir le standard C à utiliser
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Créer une bibliothèque pour le code commun d'Awale
add_library(awale_lib
    awale/awale.c
    awale/awale_save.c
    client/client_persistence.c
)

# Ajouter les headers pour la bibliothèque
target_include_directories(awale_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Créer l'exécutable serveur
add_executable(server
    server/server.c
)

# Lier la bibliothèque awale_lib et la bibliothèque mathématique
target_link_libraries(server PRIVATE 
    awale_lib 
    m
)

# Créer l'exécutable client
add_executable(client
    client/client1.c
)

target_link_libraries(client PRIVATE awale_lib)

# Créer un répertoire pour les fichiers de sauvegarde dans le dossier de build
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/saves)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/clients)