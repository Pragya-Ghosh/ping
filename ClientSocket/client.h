#ifndef CLIENT_H
#define CLIENT_H

void sendUsername(int clientSocketFD, char* activeKey);
void runClientLoop(int clientSocketFD, const char* activeKey);

#endif