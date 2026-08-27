CC = gcc
CFLAGS = -Wall -Wextra

# Build both by default when you just type 'make'
all: client server

# How to build the client
client: ClientSocket/main.c shared/utils.c
	$(CC) $(CFLAGS) ClientSocket/main.c shared/utils.c -o client

# How to build the server
server: ServerSocket/main.c shared/utils.c
	$(CC) $(CFLAGS) ServerSocket/main.c shared/utils.c -o server

# Clean up compiled files
clean:
	rm -f ClientSocket/client ServerSocket/server