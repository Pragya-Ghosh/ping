#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <poll.h> 

#include "utils.h" 

//create TCP socket: IPv4, TCP, default protocol
int createSocket() {
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
    
    //Bypass the OS TIME_WAIT state to allow immediate port reuse
    //ensures rapid server restarts during testing
    int opt = 1;
    setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    return socketFD;
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

//accept incoming connection
struct AcceptedSocket* acceptConnection(int serverSocketFD) {
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


//start accepting incoming connections concurrently using poll()
void startConnections(int serverSocketFD) {
    //create a radar board that can hold up to 100 clients
    struct pollfd fds[100]; 
    //to store usernames of the clients
    char clientNames[100][32];
    
    //Initialize all slots to empty 
    for (int i = 0; i < 100; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN; //because we want to know when data comes 'in'
        clientNames[i][0] = '\0';
    }

    //main server socket at first slot
    fds[0].fd = serverSocketFD;

    printf("Ping! Server polling for connections on port 2000...\n");

    while (1) {
        //wait for activity on any socket
        int poll_count = poll(fds, 100, -1); //-1 means block until something happens

        if (poll_count < 0) {
            perror("Poll error");
            break;
        }

        //main server socket flashed (new connection)
        if (fds[0].revents & POLLIN) {
            struct AcceptedSocket* clientSocket = acceptConnection(serverSocketFD);
            
            if (clientSocket->acceptedSuccessfully) {
                printf("New client connected on socket %d\n", clientSocket->acceptedSocketFD);
                
                //find an empty slot in array to put the new client in
                for (int i = 1; i < 100; i++) {
                    if (fds[i].fd == -1) {
                        fds[i].fd = clientSocket->acceptedSocketFD;
                        clientNames[i][0] = '\0'; 
                        break;
                    }
                }
            }
            free(clientSocket); 
        }

        //existing client socket flashed (new message)
        for (int i = 1; i < 100; i++) {
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
                
                char buffer[1024];
                int n = recv(fds[i].fd, buffer, 1023, 0);

                if (n <= 0) {
                    //client disconnected
                    printf("%s disconnected.\n", clientNames[i][0] != '\0' ? clientNames[i] : "Unknown client");
                    //remove client from poll 
                    close(fds[i].fd);
                    fds[i].fd = -1; 
                    clientNames[i][0] = '\0';
                } 
                else {
                    //successfully received message
                    buffer[n] = '\0';

                    if (clientNames[i][0] == '\0') {
                        //name empty means it's their first message
                        strncpy(clientNames[i], buffer, 31);
                        printf("[SERVER] %s joined the chat!\n", clientNames[i]);
                    } 
                    else {
                        //not a new client, was already in conversation
                        char broadcastMsg[1100]; 
                        snprintf(broadcastMsg, sizeof(broadcastMsg), "%s: %s", clientNames[i], buffer);
                        
                        //print to server terminal just to see
                        printf("%s", broadcastMsg);
                        
                        //loop through everyone and send it out
                        for (int j = 1; j < 100; j++) {
                            //send if slot is active and it's not the sender
                            if (fds[j].fd != -1 && j != i) {
                                send(fds[j].fd, broadcastMsg, strlen(broadcastMsg), 0);
                            }
                        }
                    }
                }
            }
        }
    }
}

//prompt client for username and send it to server
void sendUsername(int clientSocketFD) {
    char username[32];
    
    printf("Enter your username: ");
    fgets(username, sizeof(username), stdin);
    
    //replacing \n with null terminator
    username[strcspn(username, "\n")] = '\0';

    //send the username to the server
    send(clientSocketFD, username, strlen(username), 0);
}


//poll for client, so it can handle keyboard input and server chat at the same time
void runClientLoop(int clientSocketFD) {
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
                if(strcmp(line, "exit\n") == 0) {
                    free(line); 
                    break;
                }
                
                send(clientSocketFD, line, charCount, 0);
            }
            free(line); 
        }

        //server sent a broadcast message
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
}