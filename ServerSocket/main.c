#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../shared/utils.h"
#include "server.h"

int main(int argc, char *argv[]) {
    
    /*check command-line arguments*/
    if (argc != 2) {
        printf(COLOR_RED "Usage: %s <port>\n" COLOR_RESET, argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    /*create the socket for server*/
    int serverSocketFD = createSocket();
    struct sockaddr_in* serverAddress = createAddress(NULL, port);

    /*bind to a particular address*/
    int bindFD = bind(serverSocketFD, (struct sockaddr *)serverAddress, sizeof(*serverAddress));

    if (bindFD == 0) {
        printf(COLOR_GREEN "Server socket was bound successfully to port %d\n" COLOR_RESET, port);
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