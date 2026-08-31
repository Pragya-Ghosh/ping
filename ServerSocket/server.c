#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>

#include "../shared/utils.h"
#include "../shared/crypto.h"
#include "server.h"

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
            return 1; 
        }
    }
    return 0; 
}

//find socket index by username 
int findClientIndex(char* targetName, struct pollfd fds[], char clientNames[][32]) {
    for (int j = 1; j < 100; j++) {
        if (fds[j].fd != -1 && strcmp(clientNames[j], targetName) == 0) {
            return j;
        }
    }
    return -1; //user is offline
}

//process new connection on server socket
void handleNewConnection(int serverSocketFD, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    struct AcceptedSocket* clientSocket = acceptConnection(serverSocketFD);
    
    if (clientSocket->acceptedSuccessfully) {
        printf(COLOR_CYAN "[SERVER LOG] New socket connection established (FD: %d)\n" COLOR_RESET, clientSocket->acceptedSocketFD);
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
            secureSend(fds[i].fd, errMsg, strlen(errMsg), parsedKey);
        } 
        else {
            strcpy(clientNames[i], parsedName);
            strcpy(clientKeys[i], parsedKey);
            
            char successMsg[100];
            snprintf(successMsg, sizeof(successMsg), "REGISTERED %s\n", parsedName);
            secureSend(fds[i].fd, successMsg, strlen(successMsg), clientKeys[i]);
            
            printf(COLOR_GREEN "[SERVER LOG] %s registered successfully\n" COLOR_RESET, clientNames[i]);
            fflush(stdout);
        }
    } 
    else {
        char errMsg[] = "ERROR invalid command format\n";
        send(fds[i].fd, errMsg, strlen(errMsg), 0); 
    }
}

//process QUIT command
void handleQuit(int i, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    char goodbyeMsg[100];
    snprintf(goodbyeMsg, sizeof(goodbyeMsg), "GOODBYE %s\n", clientNames[i]);
    secureSend(fds[i].fd, goodbyeMsg, strlen(goodbyeMsg), clientKeys[i]);
    
    printf(COLOR_YELLOW "[SERVER LOG] %s disconnected.\n" COLOR_RESET, clientNames[i]);
    fflush(stdout);
    
    removeClient(i, fds, clientNames, clientKeys);
}

//process targeted SEND TO command
void handleSendTo(int i, char* targetName, char* messageContent, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    int targetIndex = findClientIndex(targetName, fds, clientNames);
    
    if (targetIndex != -1) {
        char routedMsg[1100];
        snprintf(routedMsg, sizeof(routedMsg), "FROM %s: %s\n", clientNames[i], messageContent);
        secureSend(fds[targetIndex].fd, routedMsg, strlen(routedMsg), clientKeys[targetIndex]);
        
        printf(COLOR_CYAN "[SERVER LOG] Routed message from %s to %s\n" COLOR_RESET, clientNames[i], targetName);
        fflush(stdout);
    } 
    else {
        char errMsg[100];
        snprintf(errMsg, sizeof(errMsg), "ERROR %s is not online\n", targetName);
        secureSend(fds[i].fd, errMsg, strlen(errMsg), clientKeys[i]);
        
        printf(COLOR_RED "[SERVER LOG] Failed route: %s is offline\n" COLOR_RESET, targetName);
        fflush(stdout);
    }
}

//process broadcast SEND ALL command
void handleSendAll(int i, char* messageContent, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    char broadcastMsg[1100];
    snprintf(broadcastMsg, sizeof(broadcastMsg), "BROADCAST FROM %s: %s\n", clientNames[i], messageContent);
    
    //loop through everyone and send it out
    for (int j = 1; j < 100; j++) {
        //send if slot is active and it's not the sender
        if (fds[j].fd != -1 && j != i) {
            secureSend(fds[j].fd, broadcastMsg, strlen(broadcastMsg), clientKeys[j]);
        }
    }
    
    printf(COLOR_CYAN "[SERVER LOG] Broadcast dispatched from %s\n" COLOR_RESET, clientNames[i]);
    fflush(stdout);
}

//process LIST command
void handleList(int i, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
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
    secureSend(fds[i].fd, listMsg, strlen(listMsg), clientKeys[i]);
}

//process SENDFILE command
void handleSendFile(int i, char* buffer, int n, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    char targetName[32];
    
    //allocates 256 bytes to support both standard short paths and long absolute paths 
    char filename[256]; 
    int fileSize;

    //find the newline that separates the protocol header from the file payload
    char* payload = strchr(buffer, '\n');
    if (!payload) {
        char errMsg[] = "ERROR invalid command format\n";
        secureSend(fds[i].fd, errMsg, strlen(errMsg), clientKeys[i]);
        return;
    }
    
    *payload = '\0'; //split header and payload
    payload++; //point to the first byte of the file payload
    
    //determine exactly how many raw bytes were received in this frame to avoid 
    //relying on null-terminators, which would break on binary data
    int headerLen = payload - buffer;
    int actualPayloadBytes = n - headerLen; 

    //parse framing format: strictly enforce the length-prefixed format[cite: 4]
    if (sscanf(buffer, "SENDFILE TO %31s %255s %d", targetName, filename, &fileSize) == 3) {
        
        //check for valid .txt extension
        char* ext = strrchr(filename, '.');
        if (!ext || strcmp(ext, ".txt") != 0) {
            char errMsg[] = "ERROR only .txt files are supported\n";
            secureSend(fds[i].fd, errMsg, strlen(errMsg), clientKeys[i]);
            return;
        }

        //check size limit (1MB max) to safely avoid chunked transfers[cite: 4]
        if (fileSize > 1048576) {
            char errMsg[] = "ERROR file exceeds 1MB limit\n";
            secureSend(fds[i].fd, errMsg, strlen(errMsg), clientKeys[i]);
            return;
        }

        int targetIndex = findClientIndex(targetName, fds, clientNames);
        if (targetIndex != -1) {
            
            //construct receiver frame using length-prefixed format
            //buffer increased to 512 to ensure large absolute paths do not truncate the frame
            char header[512];
            int hLen = snprintf(header, sizeof(header), "RECVFILE FROM %s %s %d\n", clientNames[i], filename, fileSize);
            
            //forward header and raw payload securely
            //stitch header and payload together in memory using memcpy instead of strcat
            //this is critical because the payload may contain null bytes that would stop a string copy
            char* combinedBuffer = malloc(hLen + actualPayloadBytes);
            memcpy(combinedBuffer, header, hLen);
            memcpy(combinedBuffer + hLen, payload, actualPayloadBytes);
            
            secureSend(fds[targetIndex].fd, combinedBuffer, hLen + actualPayloadBytes, clientKeys[targetIndex]);
            free(combinedBuffer);
            
            printf(COLOR_CYAN "[SERVER LOG] Routed file %s (%d bytes) from %s to %s\n" COLOR_RESET, filename, fileSize, clientNames[i], targetName);
            fflush(stdout);
        } 
        else {
            char errMsg[100];
            snprintf(errMsg, sizeof(errMsg), "ERROR %s is not online\n", targetName);
            secureSend(fds[i].fd, errMsg, strlen(errMsg), clientKeys[i]);
        }
    } 
    else {
        //catches badly formatted file transfers that are missing the size prefix
        char errMsg[] = "ERROR invalid command format\n";
        secureSend(fds[i].fd, errMsg, strlen(errMsg), clientKeys[i]);
    }
}

//route active chat commands
void handleChatCommand(int i, char* buffer, int n, struct pollfd fds[], char clientNames[][32], char clientKeys[][32]) {
    
    //intercept length-prefixed frames here before modifying the buffer
    //sendfile payloads have raw bytes, so do not strip newlines or treat them as strings
    if (strncmp(buffer, "SENDFILE TO ", 12) == 0) {
        handleSendFile(i, buffer, n, fds, clientNames, clientKeys);
        return;
    }

    //for normal text commands, strip trailing newline
    buffer[strcspn(buffer, "\r\n")] = '\0';

    char targetName[32];
    char messageContent[1024];

    if (strcmp(buffer, "QUIT") == 0) 
        handleQuit(i, fds, clientNames, clientKeys);

    else if (strcmp(buffer, "LIST") == 0) 
        handleList(i, fds, clientNames, clientKeys);

    else if (sscanf(buffer, "SEND TO %31[^:]: %[^\n]", targetName, messageContent) == 2) 
        handleSendTo(i, targetName, messageContent, fds, clientNames, clientKeys);
    
    else if (sscanf(buffer, "SEND ALL: %[^\n]", messageContent) == 1) 
        handleSendAll(i, messageContent, fds, clientNames, clientKeys);
    
    else if (strncmp(buffer, "SEND ", 5) == 0 || strncmp(buffer, "SENDFILE ", 9) == 0) {
        //catches badly formatted known commands
        char errMsg[] = "ERROR invalid command format\n";
        secureSend(fds[i].fd, errMsg, strlen(errMsg), clientKeys[i]);
        printf(COLOR_RED "[SERVER LOG] Malformed command rejected from %s\n" COLOR_RESET, clientNames[i]);
        fflush(stdout);
    }
    else {
        //catches completely unrecognizable commands so the server doesn't crash
        char errMsg[] = "ERROR unknown command\n";
        secureSend(fds[i].fd, errMsg, strlen(errMsg), clientKeys[i]);
        printf(COLOR_RED "[SERVER LOG] Unknown command rejected from %s\n" COLOR_RESET, clientNames[i]);
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
    printf(COLOR_YELLOW "Ping! Server polling for connections...\n" COLOR_RESET);
    fflush(stdout);

    //Allocate 1MB buffers safely on the heap to prevent stack overflows
    char* buffer = malloc(1048576 + 1024);
    char* tempDecrypted = malloc(1048576 + 1024);

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
                    if (clientNames[i][0] != '\0') {
                        printf(COLOR_YELLOW "[SERVER LOG] %s disconnected.\n" COLOR_RESET, clientNames[i]);
                        fflush(stdout);
                    }
                    removeClient(i, fds, clientNames, clientKeys);
                } 
                else {
                    if (clientNames[i][0] == '\0') {
                        
                        // Reject packets too small to possibly be a registration
                        if (n < 9) {
                            char errMsg[] = "ERROR invalid command format\n";
                            send(fds[i].fd, errMsg, strlen(errMsg), 0);
                            continue;
                        }

                        // deduce key if registration is encrypted 
                        if (strncmp(buffer, "REGISTER ", 9) != 0) {
                            
                            // try all possible key lengths up to 9
                            for (int kLen = 1; kLen <= 9; kLen++) {
                                char testKey[32] = {0};
                                
                                for (int k = 0; k < kLen; k++) {
                                    testKey[k] = buffer[k] ^ "REGISTER "[k];
                                }
                                testKey[kLen] = '\0';
                                
                                // Safely copy n bytes into our 1MB heap buffer
                                memcpy(tempDecrypted, buffer, n);
                                applyXOR(tempDecrypted, n, testKey);
                                tempDecrypted[n] = '\0';
                                
                                char pName[32], pKey[32];
                                if (sscanf(tempDecrypted, "REGISTER %31s KEY %31s", pName, pKey) == 2) {
                                    if (strcmp(pKey, testKey) == 0) {
                                        memcpy(buffer, tempDecrypted, n); // restore plaintext
                                        break;
                                    }
                                }
                            }
                        }
                        
                        buffer[n] = '\0';
                        buffer[strcspn(buffer, "\r\n")] = '\0';
                        handleRegistration(i, buffer, fds, clientNames, clientKeys);
                    } 
                    else {
                        // decrypt incoming data before parsing
                        applyXOR(buffer, n, clientKeys[i]);
                        
                        //pass exact byte count 'n' for file handling instead of relying on null bytes
                        handleChatCommand(i, buffer, n, fds, clientNames, clientKeys);
                    }
                }
            }
        }
    }
    free(buffer);
    free(tempDecrypted);
}