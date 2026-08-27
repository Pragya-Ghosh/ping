#ifndef UTILS_H
#define UTILS_H

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

#include <netinet/in.h> 

struct AcceptedSocket {
    int acceptedSocketFD;
    struct sockaddr_in address;
    int error;
    int acceptedSuccessfully;
};


int createSocket();
struct sockaddr_in* createAddress(char* ip, int port);
struct AcceptedSocket* acceptConnection(int serverSocketFD);
void startConnections(int serverSocketFD);
void sendUsername(int clientSocketFD);
void runClientLoop(int clientSocketFD);
void printHelpMenu(const char* filepath);
void printBanner(const char* filepath);

#endif