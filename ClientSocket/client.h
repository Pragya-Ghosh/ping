#ifndef CLIENT_H
#define CLIENT_H

void sendUsername(int clientSocketFD, char* activeKey, char* savedUsername);
void runClientLoop(int clientSocketFD, const char* activeKey, const char* savedUsername);

#endif