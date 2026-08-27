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
    //using maximum system backlog
    int listenFD = listen(serverSocketFD, SOMAXCONN);

    startConnections(serverSocketFD);


    close(serverSocketFD);
    

    return 0;
}