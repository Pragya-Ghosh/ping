#ifndef UTILS_H
#define UTILS_H

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

#endif