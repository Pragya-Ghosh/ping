#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

int main() {
    
    /*create a TCP socket*/
    //IPv4, TCP, default protocol
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);

    /*connect to server*/
    //configure server address
    struct sockaddr_in saddr = {
        .sin_port = htons(80),
        .sin_family = AF_INET, 
        .sin_addr.s_addr = inet_addr("172.217.26.46")
    };

    //connect to server
    int connectFD = connect(socketFD, (struct sockaddr *)&saddr, sizeof(saddr));
    
    if (connectFD == 0)
        printf("Connection was successful\n");
    else
     printf("No\n");


    /*send some data to the server*/
    char* message;
    //http request from client
    message = "GET \\ HTTP/1.1\r\nHost:google.com\r\n\r\n";
    send(socketFD, message, strlen(message), 0);

    /*receive a response from server*/
    char buffer[1024];
    //returns the number of bytes received
    int n = recv(socketFD, buffer, 1023, 0);
    if(n > 0) {
        buffer[n] = '\0'; 
        printf("Response was: %s\n", buffer);
    }

    close(socketFD);

    return 0;
}