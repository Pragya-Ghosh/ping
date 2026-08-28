#ifndef UTILS_H
#define UTILS_H

#include <netinet/in.h>

// --- ANSI COLOR MACROS ---
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"
// -------------------------

//used by server.c to track connection status
struct AcceptedSocket {
    int acceptedSocketFD;
    struct sockaddr_in address;
    int error;
    int acceptedSuccessfully;
};

void secureSend(int fd, char* data, int len, const char* key);
int createSocket();
struct sockaddr_in* createAddress(char* ip, int port);
void printHelpMenu(const char* filepath);
void printBanner(const char* filepath);

#endif