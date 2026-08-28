#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>

#include "../shared/utils.h"
#include "../shared/crypto.h"
#include "client.h"

//prompt client for username and key
void sendUsername(int clientSocketFD, char* activeKey) {
    char username[32];
    char payload[100];
    char response[1024];
    
    while (1) {
        printf("Enter your username: ");
        fgets(username, sizeof(username), stdin);
        username[strcspn(username, "\n")] = '\0'; 

        printf("Enter your encryption key: ");
        fgets(activeKey, 32, stdin);
        activeKey[strcspn(activeKey, "\n")] = '\0';

        //formatted input as per registration protocol
        snprintf(payload, sizeof(payload), "REGISTER %s KEY %s", username, activeKey);
        
        //encrypt the registration payload before sending
        int payloadLen = strlen(payload);
        applyXOR(payload, payloadLen, activeKey);
        send(clientSocketFD, payload, payloadLen, 0);

        //wait for server reply
        int n = recv(clientSocketFD, response, sizeof(response) - 1, 0);
        if (n > 0) {
            //decrypt server response
            applyXOR(response, n, activeKey);
            response[n] = '\0';
            
            //if it starts with ERROR, print red; otherwise, default color with reset
            if (strncmp(response, "ERROR", 5) == 0) 
                printf(COLOR_RED "server$ %s" COLOR_RESET "\n", response);
            else 
                printf(COLOR_GREEN "server$ %s" COLOR_RESET "\n", response);
            
            //if successful, break out of loop and enter the chat
            if (strncmp(response, "REGISTERED", 10) == 0) 
                break; 
        } 
        else {
            printf(COLOR_RED "Server disconnected\n" COLOR_RESET);
            exit(1);
        }
    }
}

//intercept keyboard input and package files if necessary
void processClientInput(int clientSocketFD, char* line, ssize_t charCount, const char* activeKey) {
    if (strncmp(line, "SENDFILE TO ", 12) == 0) {
        char targetName[32];
        char filename[256];
        
        //parse UI format: SENDFILE TO <username>: <filename>.txt
        if (sscanf(line, "SENDFILE TO %31[^:]: %255s", targetName, filename) == 2) {
            FILE *f = fopen(filename, "rb");
            if (!f) {
                printf(COLOR_RED "server$ ERROR file not found: %s\n" COLOR_RESET, filename);
            } 
            else {
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                if (fsize > 1048576) {
                    printf(COLOR_RED "server$ ERROR file exceeds 1MB limit\n" COLOR_RESET);
                } 
                else {
                    char *fileBuf = malloc(fsize + 1024);
                    int headerLen = snprintf(fileBuf, fsize + 1024, "SENDFILE TO %s %s %ld\n", targetName, filename, fsize);
                    
                    fread(fileBuf + headerLen, 1, fsize, f);
                    
                    //encrypt header and raw payload
                    applyXOR(fileBuf, headerLen + fsize, activeKey);
                    send(clientSocketFD, fileBuf, headerLen + fsize, 0);
                    free(fileBuf);
                }
                fclose(f);
            }
        } 
        else {
            printf(COLOR_RED "client$ ERROR invalid format. Use: SENDFILE TO user: filename.txt\n" COLOR_RESET);
        }
    } 
    else {
        //encrypt normal text message
        applyXOR(line, charCount, activeKey);
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
                
                //strip any absolute paths sent 
                char *base_name = strrchr(filename, '/');
                base_name = base_name ? base_name + 1 : filename;

                char outName[256];
                snprintf(outName, sizeof(outName), "received_%s", base_name);

                
                FILE *out = fopen(outName, "wb");
                if (out) {
                    fwrite(payload, 1, fsize, out);
                    fclose(out);
                    printf(COLOR_GREEN "RECVFILE FROM %s: %s (%d bytes)\n[content saved to ./%s]\n" COLOR_RESET, sender, base_name, fsize, outName);
                } 
                else {
                    printf(COLOR_RED "client$ ERROR could not write file %s\n" COLOR_RESET, outName);
                }
            }
        }
    } 
    else {
        //normal server text message 
        if (strncmp(buffer, "ERROR", 5) == 0) 
            printf(COLOR_RED "%s" COLOR_RESET "\n", buffer);
        else 
            printf("%s", buffer);
    }
}

//poll for client, so it can handle keyboard input and server chat at the same time
void runClientLoop(int clientSocketFD, const char* activeKey) {
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
                //check for disconnect commands
                if(strcmp(line, "exit\n") == 0 || strcmp(line, "QUIT\n") == 0) {
                    char quitMsg[] = "QUIT";
                    applyXOR(quitMsg, 4, activeKey); //encrypt QUIT command
                    send(clientSocketFD, quitMsg, 4, 0); 
                    free(line); 
                    break;
                }
                //check for local HELP command
                else if (strcmp(line, "HELP\n") == 0) {
                    printHelpMenu("../ClientSocket/protocols.txt");
                }
                //process normal chat or file transfers
                else {
                    processClientInput(clientSocketFD, line, charCount, activeKey);
                }
            }
            free(line); 
        }

        //server sent a message or file
        if (fds[1].revents & POLLIN) {
            int n = recv(clientSocketFD, serverBuffer, 1048576 + 1023, 0);
            
            if (n <= 0) {
                printf(COLOR_RED "\nServer closed the connection.\n" COLOR_RESET);
                break;
            } 
            else {
                //decrypt incoming server message
                applyXOR(serverBuffer, n, activeKey);
                processServerMessage(serverBuffer, n);
            }
        }
    }
    free(serverBuffer);
}