#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <poll.h> 

#include "utils.h" 
#include "crypto.h"

// --- ANSI COLOR MACROS ---
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"
// -------------------------

//encrypt payload, send to socket, and instantly decrypt back to plaintext in-memory
void secureSend(int fd, char* data, int len, const char* key) {
    applyXOR(data, len, key);   
    send(fd, data, len, 0);     
    applyXOR(data, len, key);   
}

//create TCP socket: IPv4, TCP, default protocol
int createSocket() {
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
    
    //bypass the OS TIME_WAIT state to allow immediate port reuse
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

//read and print the available chat commands from a text file
void printHelpMenu(const char* filepath) {
    FILE *file = fopen(filepath, "r");
    
    if (file == NULL) {
        //fallback if the text file is missing or path is wrong
        printf(COLOR_CYAN "Type 'QUIT' to leave, or 'LIST' to see online users.\n" COLOR_RESET);
        return;
    }

    char line[256];
    printf(COLOR_CYAN "\n");
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
    }
    printf(COLOR_RESET "\n");
    
    fclose(file);
}

//read and print the ASCII banner from a text file
void printBanner(const char* filepath) {
    FILE *file = fopen(filepath, "r");
    
    if (file == NULL) 
        return;
    
    char line[256];
    
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
    }
    
    printf(COLOR_RESET "\n");
    fclose(file);
}