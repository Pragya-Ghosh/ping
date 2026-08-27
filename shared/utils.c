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
    //to store usernames and keys of the clients
    char clientNames[100][32];
    char clientKeys[100][32]; 
    
    //Initialize all slots to empty 
    for (int i = 0; i < 100; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN; 
        clientNames[i][0] = '\0';
        clientKeys[i][0] = '\0';
    }

    fds[0].fd = serverSocketFD;
    printf("Ping! Server polling for connections on port 2000...\n");
    fflush(stdout); //force terminal to print immediately

    while (1) {
        int poll_count = poll(fds, 100, -1); 

        if (poll_count < 0) {
            perror("Poll error");
            break;
        }

        //main server socket flashed (new connection)
        if (fds[0].revents & POLLIN) {
            struct AcceptedSocket* clientSocket = acceptConnection(serverSocketFD);
            
            if (clientSocket->acceptedSuccessfully) {
                printf("[SERVER LOG] New connection on socket %d\n", clientSocket->acceptedSocketFD);
                fflush(stdout);
                
                //find an empty slot in array to put the new client in
                for (int i = 1; i < 100; i++) {
                    if (fds[i].fd == -1) {
                        fds[i].fd = clientSocket->acceptedSocketFD;
                        clientNames[i][0] = '\0'; 
                        clientKeys[i][0] = '\0'; 
                        break;
                    }
                }
            }
            free(clientSocket); 
        }

        //existing client socket flashed (new data)
        for (int i = 1; i < 100; i++) {
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
                
                char buffer[1024];
                int n = recv(fds[i].fd, buffer, 1023, 0);

                if (n <= 0) {
                    //client disconnected
                    printf("[SERVER LOG] %s disconnected.\n", clientNames[i][0] != '\0' ? clientNames[i] : "Unknown client");
                    fflush(stdout);
                    
                    close(fds[i].fd);
                    fds[i].fd = -1; 
                    clientNames[i][0] = '\0';
                    clientKeys[i][0] = '\0';
                } 
                else {
                    buffer[n] = '\0';
                    buffer[strcspn(buffer, "\r\n")] = '\0';

                    if (clientNames[i][0] == '\0') {
                        //name empty means they need to register
                        char parsedName[32];
                        char parsedKey[32];

                        //parsing formatted string
                        if (sscanf(buffer, "REGISTER %31s KEY %31s", parsedName, parsedKey) == 2) {
                            
                            //check for duplicate usernames
                            int duplicate = 0;
                            for (int j = 1; j < 100; j++) {
                                if (fds[j].fd != -1 && strcmp(clientNames[j], parsedName) == 0) {
                                    duplicate = 1;
                                    break;
                                }
                            }

                            if (duplicate) {
                                char errMsg[100];
                                snprintf(errMsg, sizeof(errMsg), "ERROR username %s already taken\n", parsedName);
                                send(fds[i].fd, errMsg, strlen(errMsg), 0);
                                
                                printf("[SERVER LOG] Rejected duplicate username: %s\n", parsedName);
                                fflush(stdout);
                            } 
                            else {
                                strcpy(clientNames[i], parsedName);
                                strcpy(clientKeys[i], parsedKey);
                                
                                char successMsg[100];
                                snprintf(successMsg, sizeof(successMsg), "REGISTERED %s\n", parsedName);
                                send(fds[i].fd, successMsg, strlen(successMsg), 0);
                                
                                printf("[SERVER LOG] %s registered with key %s\n", clientNames[i], clientKeys[i]);
                                fflush(stdout);
                            }
                        } 
                        else {
                            char *errMsg = "ERROR invalid command format\n";
                            send(fds[i].fd, errMsg, strlen(errMsg), 0);
                            
                            printf("[SERVER LOG] Invalid registration format received.\n");
                            fflush(stdout);
                        }
                    } 
                    else {
                        char broadcastMsg[1100]; 
                        snprintf(broadcastMsg, sizeof(broadcastMsg), "%s: %s\n", clientNames[i], buffer);
                        
                        printf("[SERVER LOG] Broadcasting: %s", broadcastMsg);
                        fflush(stdout);
                        
                        for (int j = 1; j < 100; j++) {
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


//prompt client for username and key
void sendUsername(int clientSocketFD) {
    char username[32];
    char key[32];
    char payload[100];
    char response[1024];
    
    while (1) {
        printf("Enter your username: ");
        fgets(username, sizeof(username), stdin);
        username[strcspn(username, "\n")] = '\0'; 

        printf("Enter your encryption key: ");
        fgets(key, sizeof(key), stdin);
        key[strcspn(key, "\n")] = '\0';

        //formatted input as per registration protocol: REGISTER <username> KEY <key>
        snprintf(payload, sizeof(payload), "REGISTER %s KEY %s", username, key);
        send(clientSocketFD, payload, strlen(payload), 0);

        //wait for server reply
        int n = recv(clientSocketFD, response, sizeof(response) - 1, 0);
        if (n > 0) {
            response[n] = '\0';
            printf("server$ %s\n", response);
            
            //if successful, break out of loop and enter the chat
            if (strncmp(response, "REGISTERED", 10) == 0) {
                break; 
            }
        } 
        else {
            printf("Server disconnected.\n");
            exit(1);
        }
    }
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