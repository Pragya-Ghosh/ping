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


//disconnect and clear client data
void removeClient(int i, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    close(fds[i].fd);
    fds[i].fd = -1; 
    clientNames[i][0] = '\0';
    clientKeys[i][0] = '\0';
}

//check if a username is already taken
int isDuplicateUsername(char* username, struct pollfd fds[], char clientNames[][32]) {
    for (int j = 1; j < 100; j++) {
        if (fds[j].fd != -1 && strcmp(clientNames[j], username) == 0) {
            return 1; //found a match
        }
    }
    return 0; //no duplicate
}

//find socket index by username (returns -1 if offline)
int findClientIndex(char* targetName, struct pollfd fds[], char clientNames[][32]) {
    for (int j = 1; j < 100; j++) {
        if (fds[j].fd != -1 && strcmp(clientNames[j], targetName) == 0) {
            return j;
        }
    }
    return -1;
}

//process new connection on server socket
void handleNewConnection(int serverSocketFD, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    struct AcceptedSocket* clientSocket = acceptConnection(serverSocketFD);
    
    if (clientSocket->acceptedSuccessfully) {
        printf("[SERVER LOG] New socket connection established (FD: %d)\n", clientSocket->acceptedSocketFD);
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

//process registration handshake
void handleRegistration(int i, char* buffer, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    char parsedName[32];
    char parsedKey[32];

    if (sscanf(buffer, "REGISTER %31s KEY %31s", parsedName, parsedKey) == 2) {
        if (isDuplicateUsername(parsedName, fds, clientNames)) {
            char errMsg[100];
            snprintf(errMsg, sizeof(errMsg), "ERROR username %s already taken\n", parsedName);
            send(fds[i].fd, errMsg, strlen(errMsg), 0);
        } 
        else {
            strcpy(clientNames[i], parsedName);
            strcpy(clientKeys[i], parsedKey);
            
            char successMsg[100];
            snprintf(successMsg, sizeof(successMsg), "REGISTERED %s\n", parsedName);
            send(fds[i].fd, successMsg, strlen(successMsg), 0);
            
            printf("[SERVER LOG] %s registered successfully with key %s\n", clientNames[i], clientKeys[i]);
            fflush(stdout);
        }
    } 
    else {
        char *errMsg = "ERROR invalid command format\n";
        send(fds[i].fd, errMsg, strlen(errMsg), 0);
    }
}

//process QUIT command
void handleQuit(int i, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    char goodbyeMsg[100];
    snprintf(goodbyeMsg, sizeof(goodbyeMsg), "GOODBYE %s\n", clientNames[i]);
    send(fds[i].fd, goodbyeMsg, strlen(goodbyeMsg), 0);
    
    printf("[SERVER LOG] %s issued QUIT command. Disconnecting.\n", clientNames[i]);
    fflush(stdout);
    
    removeClient(i, fds, clientNames, clientKeys);
}

//process targeted SEND TO command
void handleSendTo(int i, char* targetName, char* messageContent, struct pollfd fds[], char clientNames[][32]) {
    int targetIndex = findClientIndex(targetName, fds, clientNames);
    
    if (targetIndex != -1) {
        char routedMsg[1100];
        snprintf(routedMsg, sizeof(routedMsg), "FROM %s: %s\n", clientNames[i], messageContent);
        send(fds[targetIndex].fd, routedMsg, strlen(routedMsg), 0);
        
        printf("[SERVER LOG] Routed message from %s to %s\n", clientNames[i], targetName);
        fflush(stdout);
    } 
    else {
        char errMsg[100];
        snprintf(errMsg, sizeof(errMsg), "ERROR %s is not online\n", targetName);
        send(fds[i].fd, errMsg, strlen(errMsg), 0);
        
        printf("[SERVER LOG] %s attempted to message offline user %s\n", clientNames[i], targetName);
        fflush(stdout);
    }
}

//process broadcast SEND ALL command
void handleSendAll(int i, char* messageContent, struct pollfd fds[], char clientNames[][32]) {
    char broadcastMsg[1100];
    snprintf(broadcastMsg, sizeof(broadcastMsg), "BROADCAST FROM %s: %s\n", clientNames[i], messageContent);
    
    //loop through everyone and send it out
    for (int j = 1; j < 100; j++) {
        //send if slot is active and it's not the sender
        if (fds[j].fd != -1 && j != i) {
            send(fds[j].fd, broadcastMsg, strlen(broadcastMsg), 0);
        }
    }
    
    printf("[SERVER LOG] %s broadcasted a message to everyone\n", clientNames[i]);
    fflush(stdout);
}

//process LIST command
void handleList(int i, struct pollfd fds[], char clientNames[][32]) {
    char listMsg[1024] = "ONLINE ";
    int first = 1;
    for (int j = 1; j < 100; j++) {
        if (fds[j].fd != -1 && clientNames[j][0] != '\0') {
            if (!first) strcat(listMsg, ", ");
            strcat(listMsg, clientNames[j]);
            first = 0;
        }
    }
    strcat(listMsg, "\n");
    send(fds[i].fd, listMsg, strlen(listMsg), 0);
    
    printf("[SERVER LOG] Sent online user list to %s\n", clientNames[i]);
    fflush(stdout);
}

//process SENDFILE command
void handleSendFile(int i, char* buffer, int n, struct pollfd fds[], char clientNames[][32]) {
    char targetName[32];
    char filename[128];
    int fileSize;

    //find the newline that separates the protocol header from the file payload
    char* payload = strchr(buffer, '\n');
    if (!payload) {
        char *errMsg = "ERROR invalid command format\n";
        send(fds[i].fd, errMsg, strlen(errMsg), 0);
        return;
    }
    
    *payload = '\0'; //split header and payload
    payload++; //point to the first byte of the file
    int headerLen = payload - buffer;
    int actualPayloadBytes = n - headerLen; 

    //parse framing format: SENDFILE TO <username> <filename> <size>
    if (sscanf(buffer, "SENDFILE TO %31s %127s %d", targetName, filename, &fileSize) == 3) {
        
        //check for valid .txt extension
        char* ext = strrchr(filename, '.');
        if (!ext || strcmp(ext, ".txt") != 0) {
            char *errMsg = "ERROR only .txt files are supported\n";
            send(fds[i].fd, errMsg, strlen(errMsg), 0);
            return;
        }

        //check size limit (1MB max)
        if (fileSize > 1048576) {
            char *errMsg = "ERROR file exceeds 1MB limit\n";
            send(fds[i].fd, errMsg, strlen(errMsg), 0);
            return;
        }

        int targetIndex = findClientIndex(targetName, fds, clientNames);
        if (targetIndex != -1) {
            //construct receiver frame: RECVFILE FROM <sender> <filename> <size>\n
            char header[256];
            int hLen = snprintf(header, sizeof(header), "RECVFILE FROM %s %s %d\n", clientNames[i], filename, fileSize);
            
            //forward header and raw payload
            send(fds[targetIndex].fd, header, hLen, 0);
            send(fds[targetIndex].fd, payload, actualPayloadBytes, 0);
            
            printf("[SERVER LOG] Routed file %s (%d bytes) from %s to %s\n", filename, fileSize, clientNames[i], targetName);
            fflush(stdout);
        } 
        else {
            char errMsg[100];
            snprintf(errMsg, sizeof(errMsg), "ERROR %s is not online\n", targetName);
            send(fds[i].fd, errMsg, strlen(errMsg), 0);
        }
    }
}

//route active chat commands
void handleChatCommand(int i, char* buffer, int n, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    
    //sendfile payloads have raw bytes, so do not strip newlines
    if (strncmp(buffer, "SENDFILE TO ", 12) == 0) {
        handleSendFile(i, buffer, n, fds, clientNames);
        return;
    }

    //for normal text commands, safely strip trailing newline
    buffer[strcspn(buffer, "\r\n")] = '\0';

    char targetName[32];
    char messageContent[1024];

    if (strcmp(buffer, "QUIT") == 0) 
        handleQuit(i, fds, clientNames, clientKeys);

    else if (strcmp(buffer, "LIST") == 0) 
        handleList(i, fds, clientNames);

    else if (sscanf(buffer, "SEND TO %31[^:]: %[^\n]", targetName, messageContent) == 2) 
        handleSendTo(i, targetName, messageContent, fds, clientNames);
    

    else if (sscanf(buffer, "SEND ALL: %[^\n]", messageContent) == 1) 
        handleSendAll(i, messageContent, fds, clientNames);
    

    else {
        char *errMsg = "ERROR invalid command format\n";
        send(fds[i].fd, errMsg, strlen(errMsg), 0);
        printf("[SERVER LOG] Rejected malformed input from %s\n", clientNames[i]);
        fflush(stdout);
    }
}

//start accepting incoming connections concurrently using poll()
void startConnections(int serverSocketFD) {
    struct pollfd fds[100]; 
    char clientNames[100][32];
    char clientKeys[100][32]; 
    
    for (int i = 0; i < 100; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN; 
        clientNames[i][0] = '\0';
        clientKeys[i][0] = '\0';
    }

    fds[0].fd = serverSocketFD;
    printf("Ping! Server polling for connections on port 2000...\n");
    fflush(stdout);

    //allocate 1MB buffer on heap to handle file transfers without stack overflow
    char* buffer = malloc(1048576 + 1024);

    while (1) {
        int poll_count = poll(fds, 100, -1); 

        if (poll_count < 0) {
            perror("Poll error");
            break;
        }

        if (fds[0].revents & POLLIN) {
            handleNewConnection(serverSocketFD, fds, clientNames, clientKeys);
        }

        for (int i = 1; i < 100; i++) {
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
                
                int n = recv(fds[i].fd, buffer, 1048576 + 1023, 0);

                if (n <= 0) {
                    printf("[SERVER LOG] %s disconnected.\n", clientNames[i][0] != '\0' ? clientNames[i] : "Unknown client");
                    fflush(stdout);
                    removeClient(i, fds, clientNames, clientKeys);
                } 
                else {
                    buffer[n] = '\0';

                    if (clientNames[i][0] == '\0') {
                        buffer[strcspn(buffer, "\r\n")] = '\0';
                        handleRegistration(i, buffer, fds, clientNames, clientKeys);
                    } 
                    else {
                        //pass exact byte count 'n' for file handling
                        handleChatCommand(i, buffer, n, fds, clientNames, clientKeys);
                    }
                }
            }
        }
    }
    free(buffer);
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


//intercept keyboard input and package files if necessary
void processClientInput(int clientSocketFD, char* line, ssize_t charCount) {
    if (strncmp(line, "SENDFILE TO ", 12) == 0) {
        char targetName[32];
        char filename[256];
        
        //parse UI format: SENDFILE TO <username>: <filename>.txt
        if (sscanf(line, "SENDFILE TO %31[^:]: %255s", targetName, filename) == 2) {
            FILE *f = fopen(filename, "rb");
            if (!f) {
                printf("server$ ERROR file not found: %s\n", filename);
            } 
            else {
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                if (fsize > 1048576) {
                    printf("server$ ERROR file exceeds 1MB limit\n");
                } 
                else {
                    char *fileBuf = malloc(fsize + 1024);
                    int headerLen = snprintf(fileBuf, fsize + 1024, "SENDFILE TO %s %s %ld\n", targetName, filename, fsize);
                    
                    fread(fileBuf + headerLen, 1, fsize, f);
                    send(clientSocketFD, fileBuf, headerLen + fsize, 0);
                    free(fileBuf);
                }
                fclose(f);
            }
        } 
        else {
            printf("client$ ERROR invalid format. Use: SENDFILE TO user: filename.txt\n");
        }
    } 
    else {
        //normal text message
        send(clientSocketFD, line, charCount, 0);
    }
}

//intercept server messages and save files to disk
void processServerMessage(char* buffer, int n) {
    buffer[n] = '\0';
    
    if (strncmp(buffer, "RECVFILE FROM ", 14) == 0) {
        char sender[32];
        char filename[128];
        int fsize;
        
        char* payload = strchr(buffer, '\n');
        if (payload) {
            *payload = '\0';
            payload++;
            
            if (sscanf(buffer, "RECVFILE FROM %31s %127s %d", sender, filename, &fsize) == 3) {
                char outName[256];
                snprintf(outName, sizeof(outName), "received_%s", filename);
                
                FILE *out = fopen(outName, "wb");
                if (out) {
                    fwrite(payload, 1, fsize, out);
                    fclose(out);
                    printf("RECVFILE FROM %s: %s (%d bytes)\n[content saved to ./%s]\n", sender, filename, fsize, outName);
                } 
                else {
                    printf("client$ ERROR could not write file %s\n", outName);
                }
            }
        }
    } 
    else {
        //normal server text message
        printf("%s", buffer);
    }
}

//poll for client, so it can handle keyboard input and server chat at the same time
void runClientLoop(int clientSocketFD) {
    struct pollfd fds[2];
    
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = clientSocketFD;
    fds[1].events = POLLIN;

    //allocate 1MB buffer on heap to handle incoming files
    char* serverBuffer = malloc(1048576 + 1024);

    while(1) {
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
                if(strcmp(line, "exit\n") == 0 || strcmp(line, "QUIT\n") == 0) {
                    send(clientSocketFD, "QUIT", 4, 0); 
                    free(line); 
                    break;
                }
                
                processClientInput(clientSocketFD, line, charCount);
            }
            free(line); 
        }

        //server sent a message or file
        if (fds[1].revents & POLLIN) {
            int n = recv(clientSocketFD, serverBuffer, 1048576 + 1023, 0);
            
            if (n <= 0) {
                printf("\nServer closed the connection.\n");
                break;
            } 
            else {
                processServerMessage(serverBuffer, n);
            }
        }
    }
    free(serverBuffer);
}



//read and print the available chat commands from a text file
void printHelpMenu(const char* filepath) {
    FILE *file = fopen(filepath, "r");
    
    if (file == NULL) {
        //fallback if the text file is missing or path is wrong
        printf("Type 'QUIT' to leave, or 'LIST' to see online users.\n");
        return;
    }

    char line[256];
    printf("\n");
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
    }
    printf("\n");
    
    fclose(file);
}

