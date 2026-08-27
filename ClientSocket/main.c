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

    printf("You are now in the chat. (type 'exit' to leave)\n");

    //Poll for client, so it can handle keyboard input and server chat at the same time
    struct pollfd fds[2];
    
    //slot 0: watch the keyboard (client input)
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    //slot 1: watch server socket (chat conversation)
    fds[1].fd = clientSocketFD;
    fds[1].events = POLLIN;

    while(1) {
        //wait for either keyboard or server chat to do something
        int poll_count = poll(fds, 2, -1);
        if (poll_count < 0) {
            perror("Poll error");
            break;
        }

        //typed on keyboard
        if (fds[0].revents & POLLIN) {
            char* line = NULL;
            size_t lineSize = 0;
            
            ssize_t charCount = getline(&line, &lineSize, stdin);
            if(charCount > 0) {
                if(strcmp(line, "exit\n") == 0) 
                    break;
                
                send(clientSocketFD, line, charCount, 0);
            }
        }

        //server sent a vbroadcast message
        if (fds[1].revents & POLLIN) {
            char buffer[1024];
            int n = recv(clientSocketFD, buffer, 1023, 0);
            
            if (n <= 0) {
                printf("\nServer closed the connection.\n");
                break;
            } 
            else {
                buffer[n] = '\0';
                printf("%s", buffer);
            }
        }
    }

    close(clientSocketFD);

    return 0;
}