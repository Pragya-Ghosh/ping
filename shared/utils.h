#ifndef UTILS_H
#define UTILS_H

// Declare your shared functions here
int createSocket();
struct sockaddr_in* createAddress(char* ip, int port);

#endif