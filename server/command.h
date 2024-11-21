#ifndef COMMAND_H
#define COMMAND_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../client/client2.h"
#include "../awale/awale.h"
#include "../awale/awale_save.h"
#include "../message.h"
#include "data.h"
#include "util.h"

// Game commands
void processClientMessage(Client* client, const char* message, ServerContext* context);
void handleSendClientList(Client* client, ServerContext* context);
void handleSendGamesList(Client* client, ServerContext* context);
void handleChallenge(Client* client, const char* challengedName, ServerContext* context);
void handleWatchRequest(Client* client, const char* request, ServerContext* context);
void handleClientDisconnect(int clientIndex, ServerContext* context);
void handleGameOrChat(Client* client, const char* message, ServerContext* context);
void handleChatMessage(Client* client, const char* message, ServerContext* context);
void handlePrivateMessage(Client* client, const char* message, ServerContext* context);
void handlePrivateFriendsMessage(Client* client, const char* message, ServerContext* context);
void handleChallengeResponse(Client* client, const char* response, ServerContext* context);
void handleGameMove(int sessionId, Client* client, const char* buffer, ServerContext* context);
void handleSaveStateCommand(Client* client, const char* message, AwaleGame *game);
void handleLoadCommand(GameSession* session, Client* client, const char* message);
void handleBiography(Client* client, const char* message, ServerContext* context);
void handleAddFriend(Client* client, const char* friendName, ServerContext* context);
void handleFriendResponse(Client* client, const char* response, ServerContext* context);
void handleListFriends(Client* client, ServerContext* context);
void handleUnfriend(Client* client, const char* friendName, ServerContext* context);
void handlePrivacy(Client* client, const char* message, ServerContext* context);
void handleHelp(Client* client);
void handleExit(Client* client, ServerContext* context);
void handleNewConnection(ServerContext* context);
void startGame(int sessionId, Client* client1, Client* client2, ServerContext* context);
void printUpdatedElo(Client* player1, Client* player2);
void handleGameOver(GameSession* session);
void handleAnswer(Client* client, const char* response, ServerContext* context);

#endif
