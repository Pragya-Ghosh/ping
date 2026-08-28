#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../shared/utils.h"
#include "server.h"

int main() {
    
    /*create the socket for server*/
    int serverSocketFD = createSocket();
    struct sockaddr_in* serverAddress = createAddress(NULL, 2000);

    /*bind to a particular address*/
    int bindFD = bind(serverSocketFD, (struct sockaddr *)serverAddress, sizeof(*serverAddress));

    if (bindFD == 0) {
        printf(COLOR_GREEN "Server socket was bound successfully\n" COLOR_RESET);
    } 
    else {
        printf(COLOR_RED "Failed to bind socket\n" COLOR_RESET);
        free(serverAddress);
        return 1; 
    }
    
    /*listen for incoming connections*/
    //using maximum system backlog
    int listenFD = listen(serverSocketFD, SOMAXCONN);
    if (listenFD < 0) {
        perror(COLOR_RED "Listen failed" COLOR_RESET);
        free(serverAddress);
        return 1;
    }

    startConnections(serverSocketFD);

    close(serverSocketFD);
    free(serverAddress); 
    
    return 0;
}