#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h" 

//create TCP socket: IPv4, TCP, default protocol
int createSocket() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

//configure IPv4 socket address
struct sockaddr_in* createAddress(char* ip, int port) {
    struct sockaddr_in* address = malloc(sizeof(struct sockaddr_in));
    address->sin_port = htons(port);
    address->sin_family = AF_INET;

    if(ip == NULL || strlen(ip) == 0)
        address->sin_addr.s_addr = INADDR_ANY;
    else
        address->sin_addr.s_addr = inet_addr(ip);
    
    return address;
}

//accept incoming connections
struct AcceptedSocket* acceptConnections(int serverSocketFD) {
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    int clientSocketFD = accept(serverSocketFD, (struct sockaddr *)&clientAddress, &clientAddressLength);

    struct AcceptedSocket* acceptedSocket = malloc(sizeof(struct AcceptedSocket));
    acceptedSocket->address = clientAddress;
    acceptedSocket->acceptedSocketFD = clientSocketFD;
    acceptedSocket->acceptedSuccessfully = clientSocketFD > 0;

    if (!acceptedSocket->acceptedSuccessfully)
        acceptedSocket->error = clientSocketFD;

    return acceptedSocket;
}