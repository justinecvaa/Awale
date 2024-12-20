#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <stdint.h> 
#include <signal.h>

#include "client1.h"
#include "../awale/awale.h"

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

static SOCKET global_sock = INVALID_SOCKET;
static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
    running = 0;
    if (global_sock != INVALID_SOCKET) {
        write_server(global_sock, "quit");
        usleep(100000); // 100ms pause
        closesocket(global_sock);
        global_sock = INVALID_SOCKET;
    }
}


static void app(const char *address, const char *name) {
    SOCKET sock = init_connection(address);
    global_sock = sock;

    signal(SIGINT, handle_sigint);

    write_server(sock, name);

    struct message msg;
    fd_set rdfs;
    AwaleGame game;

    while(running) {
        FD_ZERO(&rdfs);
        FD_SET(STDIN_FILENO, &rdfs);
        FD_SET(sock, &rdfs);

        // Utiliser un timeout pour vérifier régulièrement running
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if(select(sock + 1, &rdfs, NULL, NULL, &tv) == -1) {
            if (errno == EINTR) continue;  // Interruption par signal
            perror("select()");
            break;
        }

        /* something from standard input : i.e keyboard */
        if(FD_ISSET(STDIN_FILENO, &rdfs)) {
            fgets(msg.content, BUF_SIZE - 1, stdin);
            char *p = strstr(msg.content, "\n");
            if(p != NULL) {
                *p = 0;
            } else {
                msg.content[BUF_SIZE - 1] = 0;
            }
            //Vérifier si le message est vide
            if(strlen(msg.content) == 0) {
                continue;
            }
            write_server(sock, msg.content);
            //Effacer le message dans terminal
            printf("\033[1A\033[2K");
        } else if(FD_ISSET(sock, &rdfs)) {
            int n = read_server(sock, &msg);
            if(n == 0) {
                printf("Server disconnected!\n");
                break;
            }
            // Vérifier le contenu du message
            if (strncmp(msg.content, "game:", 5) == 0)
            {
                deserializeGame(&game, msg.content);
                printGame(&game, name);
            }
            else
            {
                puts(msg.content);
            }
        }
    }

    end_connection(sock);
    global_sock = INVALID_SOCKET;
}


static int init_connection(const char *address)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };
   struct hostent *hostinfo;

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   hostinfo = gethostbyname(address);
   if (hostinfo == NULL)
   {
      fprintf (stderr, "Unknown host %s.\n", address);
      exit(EXIT_FAILURE);
   }

   sin.sin_addr = *(IN_ADDR *) hostinfo->h_addr;
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(connect(sock,(SOCKADDR *) &sin, sizeof(SOCKADDR)) == SOCKET_ERROR)
   {
      perror("connect()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int read_server(SOCKET sock, struct message *msg)
{
    // Lire d'abord la taille
    int size_read = recv(sock, &msg->size, sizeof(uint32_t), 0);
    if (size_read != sizeof(uint32_t)) {
        perror("recv() size");
        return -1;
    }

    // Vérifier que la taille est valide
    if (msg->size >= BUF_SIZE) {
        fprintf(stderr, "Message trop grand: %u bytes\n", msg->size);
        return -1;
    }

    // Lire ensuite exactement le nombre d'octets indiqué
    int content_read = 0;
    int remaining = msg->size;

    while (remaining > 0) {
        int n = recv(sock, msg->content + content_read, remaining, 0);
        if (n <= 0) {
            perror("recv() content");
            return -1;
        }
        content_read += n;
        remaining -= n;
    }

    // Ajouter le caractère nul de fin
    msg->content[msg->size] = 0;

    // Retourner le nombre total d'octets lus (taille + contenu)
    return size_read + content_read;
}

static void write_server(SOCKET sock, const char *buffer)
{
    struct message msg;
    msg.size = strlen(buffer);
    if (msg.size >= BUF_SIZE) {
        fprintf(stderr, "Message trop grand\n");
         return;
    }
    
    strcpy(msg.content, buffer);
    
    // Envoyer la taille
    if (send(sock, &msg.size, sizeof(uint32_t), 0) != sizeof(uint32_t)) {
        perror("send() size");
        exit(errno);
    }
    
    // Envoyer le contenu
    if (send(sock, msg.content, msg.size, 0) != msg.size) {
        perror("send() content");
        exit(errno);
    }
    
}

int main(int argc, char **argv)
{
   if(argc < 2)
   {
      printf("Usage : %s [address] [pseudo]\n", argv[0]);
      return EXIT_FAILURE;
   }

   init();

   app(argv[1], argv[2]);

   end();

   return EXIT_SUCCESS;
}