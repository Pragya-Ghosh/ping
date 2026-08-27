#ifndef UTILS_H
#define UTILS_H

#include <netinet/in.h> 

struct AcceptedSocket {
    int acceptedSocketFD;
    struct sockaddr_in address;
    int error;
    int acceptedSuccessfully;
};

// Function declarations
int createSocket();
struct sockaddr_in* createAddress(char* ip, int port);
struct AcceptedSocket* acceptConnections(int serverSocketFD);

#endif