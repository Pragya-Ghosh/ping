#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main() {
    
    /*create a TCP socket*/
    //IPv4, TCP, default protocol
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);

    /*connect to server*/
    //configure server address
    struct sockaddr_in saddr = {
        .sin_port = htons(7000),
        .sin_family = AF_INET, 
        .sin_addr.s_addr = inet_addr("235.3.42.1")
    };

    //connect to server
    int connectFD = connect(socketFD, &saddr, sizeof(saddr));
    

    return 0;
}