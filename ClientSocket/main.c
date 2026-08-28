#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>

#include "../shared/utils.h"
#include "client.h"

int main() {
    /*create the client socket*/
    int clientSocketFD = createSocket();

    /*connect to server*/
    struct sockaddr_in* saddr = createAddress("127.0.0.1", 2000);
    int connectFD = connect(clientSocketFD, (struct sockaddr*)saddr, sizeof(*saddr));
    
    if (connectFD == 0) {
        printf(COLOR_GREEN "Connection to server was successful.\n" COLOR_RESET);
    } else {
        printf(COLOR_RED "Connection failed. Is the server running?\n" COLOR_RESET);
        free(saddr);
        return 1; 
    }

    /*client chat*/
    //print ASCII banner
    printBanner("../ClientSocket/banner.txt");
    printf(COLOR_YELLOW "Ping! Join the conversation down below:\n\n" COLOR_RESET);

    char activeKey[32];
    sendUsername(clientSocketFD, activeKey);

    printf(COLOR_GREEN "You are now in the chat.\n" COLOR_RESET);
    printHelpMenu("../ClientSocket/protocols.txt");

    runClientLoop(clientSocketFD, activeKey);

    close(clientSocketFD);
    free(saddr); 

    return 0;
}