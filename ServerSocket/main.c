#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#include "../shared/utils.h"

int main() {

    /*create the socket for server*/
    int serverSocketFD = createSocket();
    struct sockaddr_in* serverAddress = createAddress(NULL, 2000);

    /*bind to a particular address*/
    int bindFD = bind(serverSocketFD, (struct sockaddr *)serverAddress, sizeof(*serverAddress));

    if(bindFD == 0)
        printf("Server socket was bound successfully\n");
    else
     printf("no\n");

    
    /*listen for incoming connections*/
    int listenFD = listen(serverSocketFD, 10);

    /*accept a connection*/
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    int clientSocketFD = accept(serverSocketFD, (struct sockaddr *)&clientAddress, &clientAddressLength);

    char buffer[1024];
    while (1) {
        int n = recv(clientSocketFD, buffer, 1023, 0);
        if(n > 0) {
            buffer[n] = '\0';
            printf("Request was: %s\n", buffer);
        }
        else break;
    }


    close(serverSocketFD);
    close(clientSocketFD);

    return 0;
}