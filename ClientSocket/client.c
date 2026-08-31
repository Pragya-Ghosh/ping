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
void sendUsername(int clientSocketFD, char* activeKey, char* savedUsername) {
    char payload[100];
    char response[1024];
    char parsedName[32];

    while (1) {
        printf("client$ "); 
        
        fgets(payload, sizeof(payload), stdin);
        
        //strip trailing newline to get a clean parsing
        payload[strcspn(payload, "\n")] = '\0';

        //validate protocol syntax locally before network transmission; this prevents 
        //malformed data from reaching the socket and breaking the server's parser
        if (sscanf(payload, "REGISTER %31s KEY %31s", parsedName, activeKey) != 2) {
            printf(COLOR_RED "client$ ERROR invalid format. Use: REGISTER <name> KEY <key>\n" COLOR_RESET);
            continue;
        }

        //restrict key length to 9 bytes; this is a cryptographic limitation 
        //required by the server's known-plaintext deduction logic (XORing against "REGISTER ")
        if (strlen(activeKey) > 9) {
            printf(COLOR_RED "client$ ERROR Key must be 9 characters or less.\n" COLOR_RESET);
            continue; 
        }

        //encrypt the message
        int payloadLen = strlen(payload);
        applyXOR(payload, payloadLen, activeKey);
        send(clientSocketFD, payload, payloadLen, 0);

        // block and wait for server validation
        int n = recv(clientSocketFD, response, sizeof(response) - 1, 0);
        if (n > 0) {
            //reverse the XOR operation using the negotiated symmetric key
            applyXOR(response, n, activeKey);
            response[n] = '\0';
            
            //format system notifications dynamically based on payload status
            if (strncmp(response, "ERROR", 5) == 0) 
                printf(COLOR_RED "server$ %s" COLOR_RESET "\n", response);
            else 
                printf(COLOR_GREEN "server$ %s" COLOR_RESET "\n", response);
            
            //terminate the registration loop and transition client to active chat phase
            if (strncmp(response, "REGISTERED", 10) == 0) {
                strcpy(savedUsername, parsedName); //save the name for the dynamic prompt
                break; 
            }
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
        if (strncmp(buffer, "ERROR", 5) == 0) {
            printf(COLOR_RED "server$ %s" COLOR_RESET "\n", buffer);
        }
        else if (strncmp(buffer, "ONLINE", 6) == 0 || strncmp(buffer, "GOODBYE", 7) == 0) {
            printf(COLOR_YELLOW "server$ %s" COLOR_RESET, buffer);
        }
        else {
            printf("%s", buffer);
        }
    }
}

//poll for client, so it can handle keyboard input and server chat at the same time
void runClientLoop(int clientSocketFD, const char* activeKey, const char* savedUsername) {
    struct pollfd fds[2];
    
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = clientSocketFD;
    fds[1].events = POLLIN;

    //allocate 1MB buffer on heap to handle incoming files
    char* serverBuffer = malloc(1048576 + 1024);
    
    int isQuitting = 0; //flag to mute the UI during the disconnect sequence

    while(1) {
        //print the dynamic username prompt required by the spec
        if (!isQuitting) {
            printf("%s$ ", savedUsername); 
            fflush(stdout);
        }

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
                    applyXOR(quitMsg, 4, activeKey); 
                    send(clientSocketFD, quitMsg, 4, 0); 
                    free(line); 
                    
                    isQuitting = 1; //mute the prompt while waiting for the server to close
                    continue; //keep polling for the goodbye message
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
                //only print the error if the server crashed or dropped us unexpectedly
                if (!isQuitting) {
                    printf(COLOR_RED "\nServer closed the connection.\n" COLOR_RESET);
                }
                break;
            } 
            else {
                //drop down a line so the incoming message doesn't collide with the prompt
                if (!isQuitting) {
                    printf("\n"); 
                }
                
                //decrypt incoming server message
                applyXOR(serverBuffer, n, activeKey);
                processServerMessage(serverBuffer, n);
            }
        }
    }
    free(serverBuffer);
}

