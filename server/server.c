#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/select.h>
#include "server.h"
#include "command.h"
#include "util.h"
#include "../client/client2.h"
#include "../awale/awale.h"
#include "../awale/awale_save.h"
#include "../message.h"

static ServerContext* context;

// Main application loop -- server
static void app(void) {
    SOCKET sock = init_connection();
    context = malloc(sizeof(ServerContext));
    initServerContext(context, sock);
    struct message msg;
    fd_set rdfs;

    while(context->isRunning) {
        FD_ZERO(&rdfs);
        FD_SET(STDIN_FILENO, &rdfs);
        FD_SET(sock, &rdfs);

        // Mettre à jour maxSocket et ajouter uniquement les sockets valides
        context->maxSocket = sock;
        for(int i = 0; i < context->actualClients; i++) {
            if(context->clients[i].sock != INVALID_SOCKET) {
                FD_SET(context->clients[i].sock, &rdfs);
                context->maxSocket = context->clients[i].sock > context->maxSocket ? 
                                   context->clients[i].sock : context->maxSocket;
            }
        }

        if(select(context->maxSocket + 1, &rdfs, NULL, NULL, NULL) == -1) {
            perror("select()");
            exit(errno);
        }

        if(FD_ISSET(STDIN_FILENO, &rdfs)) {
            break;
        }
        else if(FD_ISSET(sock, &rdfs)) {
            handleNewConnection(context);
        }
        else {
            for(int i = 0; i < context->actualClients; i++) {
                if(context->clients[i].sock != INVALID_SOCKET && FD_ISSET(context->clients[i].sock, &rdfs)) {
                    Client* client = &context->clients[i];
                    int c = read_client(client->sock, &msg);
                    
                    if(c == 0) {
                        handleClientDisconnect(i, context);
                        i--; // Ajuster l'index car le tableau a été modifié
                        continue;
                    }
                    
                    processClientMessage(client, msg.content, context);
                }
            }
        }

        checkGameTimeouts(context);
        
        // Nettoyer les clients invalides périodiquement
        static time_t lastCleanup = 0;
        time_t now = time(NULL);
        if (now - lastCleanup > 60) { // Toutes les 60 secondes
            for(int i = 0; i < context->actualClients; i++) {
                if(context->clients[i].sock == INVALID_SOCKET) {
                    handleClientDisconnect(i, context);
                    i--; // Ajuster l'index car le tableau a été modifié
                }
            }
            lastCleanup = now;
        }
    }

    clear_clients(context->clients, context->actualClients);
    end_connection(sock);
    free(context);
}



// ************************************************************************************************
// Server related functions
// ************************************************************************************************

static int init_connection(void) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN sin = {0};

    if(sock == INVALID_SOCKET) {
        perror("socket()");
        exit(errno);
    }

    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(PORT);
    sin.sin_family = AF_INET;

    if(bind(sock, (SOCKADDR *)&sin, sizeof sin) == SOCKET_ERROR) {
        perror("bind()");
        exit(errno);
    }

    if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR) {
        perror("listen()");
        exit(errno);
    }

    return sock;
}


static void end_connection(int sock) {
    closesocket(sock);
}

static void clear_clients(Client *clients, int actual) {
    for(int i = 0; i < actual; i++) {
        closesocket(clients[i].sock);
    }
}


static void init(void) {
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0) {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void) {
#ifdef WIN32
   WSACleanup();
#endif
}


int main(int argc, char **argv) {
    srand(time(NULL));
    init();
    app();
    end();
    return EXIT_SUCCESS;
}