#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>

#include "../shared/utils.h"

int main() {
    
    /*create the client socket*/
    int clientSocketFD = createSocket();

    /*connect to server*/
    struct sockaddr_in* saddr = createAddress("127.0.0.1", 2000);

    int connectFD = connect(clientSocketFD, (struct sockaddr*)saddr, sizeof(*saddr));
    
    if (connectFD == 0)
        printf("Connection was successful\n");
    else
     printf("No\n");


    /*client chat*/
    char* line = NULL;
    size_t lineSize = 0;
    printf("Ping! Join the conversation down below:\n");

    sendUsername(clientSocketFD);

    printf("You are now in the chat.\n");
    printHelpMenu("./protocols.txt");

    runClientLoop(clientSocketFD);

    close(clientSocketFD);

    return 0;
}