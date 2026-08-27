#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

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


    char* line = NULL;
    size_t lineSize = 0;
    printf("Ping! Join the conversation down below: (type 'exit' to leave)\n");
    while(1) {
        ssize_t charCount = getline(&line, &lineSize, stdin);
        if(charCount > 0) {
            if(strcmp(line, "exit\n") == 0)
                break;

            ssize_t n = send(clientSocketFD, line, charCount, 0);
        }

    }

    close(clientSocketFD);

    return 0;
}