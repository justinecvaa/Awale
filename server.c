#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "server.h"
#include "client2.h"
#include "awale.h"
#include <sys/select.h>

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if (err < 0)
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

static void app(void)
{
   SOCKET sock = init_connection();
   char buffer[BUF_SIZE];
   /* the index for the array */
   int actual = 0;
   int max = sock;
   /* an array for all clients */
   Client clients[MAX_CLIENTS];

   fd_set rdfs;

   while (1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for (i = 0; i < actual; i++)
      {
         FD_SET(clients[i].sock, &rdfs);
      }

      if (select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if (FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if (FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = {0};
         socklen_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if (csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         /* after connecting the client sends its name */
         if (read_client(csock, buffer) == -1)
         {
            /* disconnected */
            continue;
         }

         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         Client c = {csock};
         strncpy(c.name, buffer, BUF_SIZE - 1);
         clients[actual] = c;
         actual++;
      }
      else
      {
         int i = 0;
         for (i = 0; i < actual; i++)
         {
            /* a client is talking */
            if (FD_ISSET(clients[i].sock, &rdfs))
            {
               Client client = clients[i];
               int c = read_client(clients[i].sock, buffer);
               /* client disconnected */
               if (c == 0)
               {
                  closesocket(clients[i].sock);
                  remove_client(clients, i, &actual);
                  strncpy(buffer, client.name, BUF_SIZE - 1);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  send_message_to_all_clients(clients, client, actual, buffer, 1);
               }
               else
               {
                  // Check if the client requested the list of connected clients
                  if (strcmp(buffer, "list") == 0)
                  {
                     char client_list[BUF_SIZE] = "Connected clients:\n";
                     for (int j = 0; j < actual; j++)
                     {
                        strncat(client_list, clients[j].name, BUF_SIZE - strlen(client_list) - 1);
                        strncat(client_list, "\n", BUF_SIZE - strlen(client_list) - 1);
                     }
                     write_client(client.sock, client_list); // Send the list back to the requesting client
                  }
                  else if (strncmp(buffer, "challenge ", 10) == 0)
                  {
                     char *challenged_name = buffer + 10;                 // Get the name after "challenge "
                     challenged_name[strcspn(challenged_name, "\n")] = 0; // Remove newline if present

                     for (int j = 0; j < actual; j++)
                     {
                        if (strcmp(clients[j].name, challenged_name) == 0)
                        {
                           write_client(clients[j].sock, "You have been challenged by ");
                           write_client(clients[j].sock, client.name);
                           write_client(clients[j].sock, ". Do you accept? (yes/no)\n");

                           // Wait for a response from the challenged client
                           c = read_client(clients[j].sock, buffer);
                           buffer[c] = '\0'; // Ensure null termination
                           if (strcmp(buffer, "yes") == 0)
                           {
                              write_client(client.sock, "Challenge accepted. Starting game...\n");
                              write_client(clients[j].sock, "Challenge accepted. Starting game...\n");
                              // Start the game logic here
                              sleep(1);                // Wait for the clients to receive the message
                              int player = rand() % 2; // Randomly choose the starting player

                              AwaleGame game;
                              initializeGame(&game, client.name, clients[j].name);
                              char serializedGameBuffer[BUF_SIZE];
                              serializeGame(&game, serializedGameBuffer, sizeof(serializedGameBuffer));
                              // printf("%s", serializedGameBuffer);
                              if (player == 0) // Send the game state to the first player
                              {
                                 write_client(client.sock, serializedGameBuffer);
                                 write_client(clients[j].sock, serializedGameBuffer);
                                 sleep(1); // Wait for the clients to receive the message
                                 write_client(clients[j].sock, "waiting for opponent...\n");
                                 while (!game.gameOver)
                                 {
                                    printGameViewer(&game);
                                    // Receive the move from the first player
                                    c = read_client(client.sock, buffer);
                                    buffer[c] = '\0'; // Ensure null termination
                                    int house = atoi(buffer);

                                    while (!verifyMove(&game, house - 1))
                                    {
                                       write_client(client.sock, "Invalid input. Please enter a number between 1 and 6 or 'save'.\n");
                                       c = read_client(client.sock, buffer);
                                       buffer[c] = '\0'; // Ensure null termination
                                       house = atoi(buffer);
                                    }
                                    if (makeMove(&game, house - 1))
                                    {
                                       // Sérialisation normale pour le réseau
                                       serializeGame(&game, serializedGameBuffer, sizeof(serializedGameBuffer));
                                       write_client(client.sock, serializedGameBuffer);
                                       write_client(clients[j].sock, serializedGameBuffer);
                                       sleep(1); // Wait for the clients to receive the message
                                       write_client(client.sock, "waiting for opponent...\n");
                                    }

                                    // Receive the move from the second player
                                    c = read_client(clients[j].sock, buffer);
                                    buffer[c] = '\0'; // Ensure null termination
                                    house = atoi(buffer);

                                    while (!verifyMove(&game, house - 1))
                                    {
                                       write_client(clients[j].sock, "Invalid input. Please enter a number between 1 and 6 or 'save'.\n");
                                       c = read_client(clients[j].sock, buffer);
                                       buffer[c] = '\0'; // Ensure null termination
                                       house = atoi(buffer);
                                    }
                                    if (makeMove(&game, house - 1))
                                    {
                                       // Sérialisation normale pour le réseau
                                       serializeGame(&game, serializedGameBuffer, sizeof(serializedGameBuffer));
                                       write_client(clients[j].sock, serializedGameBuffer);
                                       write_client(client.sock, serializedGameBuffer);
                                       sleep(1); // Wait for the clients to receive the message
                                       write_client(clients[j].sock, "waiting for opponent...\n");
                                    }
                                 }
                              }
                              else // Send the game state to the second player
                              {
                                 write_client(clients[j].sock, serializedGameBuffer);
                                 write_client(client.sock, serializedGameBuffer);
                                 sleep(1); // Wait for the clients to receive the message
                                 write_client(client.sock, "waiting for opponent...\n");
                                 while (!game.gameOver)
                                 {
                                    printGameViewer(&game);
                                    // Receive the move from the first player
                                    c = read_client(clients[j].sock, buffer);
                                    buffer[c] = '\0'; // Ensure null termination
                                    int house = atoi(buffer);

                                    while (!verifyMove(&game, house - 1))
                                    {
                                       write_client(clients[j].sock, "Invalid input. Please enter a number between 1 and 6 or 'save'.\n");
                                       c = read_client(clients[j].sock, buffer);
                                       buffer[c] = '\0'; // Ensure null termination
                                       house = atoi(buffer);
                                    }
                                    if (makeMove(&game, house - 1))
                                    {
                                       // Sérialisation normale pour le réseau
                                       serializeGame(&game, serializedGameBuffer, sizeof(serializedGameBuffer));
                                       write_client(clients[j].sock, serializedGameBuffer);
                                       write_client(client.sock, serializedGameBuffer);
                                       sleep(1); // Wait for the clients to receive the message
                                       write_client(clients[j].sock, "waiting for opponent...\n");
                                    }

                                    // Receive the move from the second player
                                    c = read_client(client.sock, buffer);
                                    buffer[c] = '\0'; // Ensure null termination
                                    house = atoi(buffer);

                                    while (!verifyMove(&game, house - 1))
                                    {
                                       write_client(client.sock, "Invalid input. Please enter a number between 1 and 6 or 'save'.\n");
                                       c = read_client(client.sock, buffer);
                                       buffer[c] = '\0'; // Ensure null termination
                                       house = atoi(buffer);
                                    }
                                    if (makeMove(&game, house - 1))
                                    {
                                       // Sérialisation normale pour le réseau
                                       serializeGame(&game, serializedGameBuffer, sizeof(serializedGameBuffer));
                                       write_client(client.sock, serializedGameBuffer);
                                       write_client(clients[j].sock, serializedGameBuffer);
                                       sleep(1); // Wait for the clients to receive the message
                                       write_client(client.sock, "waiting for opponent...\n");
                                    }
                                 }
                              }
                           }
                           else
                           {
                              write_client(client.sock, "Challenge declined.\n");
                              write_client(clients[j].sock, "Challenge declined.\n");
                           }
                           break; // Exit the for loop after processing the challenge
                        }
                     }
                  }
                  else
                  {
                     send_message_to_all_clients(clients, client, actual, buffer, 0);
                  }
               }
               break;
            }
         }
      }
   }

   clear_clients(clients, actual);
   end_connection(sock);
}

static void clear_clients(Client *clients, int actual)
{
   int i = 0;
   for (i = 0; i < actual; i++)
   {
      closesocket(clients[i].sock);
   }
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*actual)--;
}

static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   for (i = 0; i < actual; i++)
   {
      /* we don't send message to the sender */
      if (sender.sock != clients[i].sock)
      {
         if (from_server == 0)
         {
            strncpy(message, sender.name, BUF_SIZE - 1);
            strncat(message, " : ", sizeof message - strlen(message) - 1);
         }
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         write_client(clients[i].sock, message);
      }
   }
}

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = {0};

   if (sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if (bind(sock, (SOCKADDR *)&sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if (listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int read_client(SOCKET sock, char *buffer)
{
   int n = 0;

   if ((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

static void write_client(SOCKET sock, const char *buffer)
{
   if (send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}

int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
