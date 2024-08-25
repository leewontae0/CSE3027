#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define BACKLOG 10
#define SIZE 1024

// Function to parse the HTTP request and extract the requested file path
void parse_request(char *request, char *path) {

    char *line = strtok(request, "\n");
    
    sscanf(line, "GET %s ", path);

    // If the requested path is "/", replace it with "./index.html"
    if (strcmp(path, "/") == 0) {
        strcpy(path, "./index.html");
    } 
    else { // Otherwise, prepend "./" to the path
        memmove(path+1, path, strlen(path) + 1);
        path[0] = '.';
    }

    return;
}

// Function to determine the content type of the requested file based on its extension
char* get_type(char *path) {

    char* exts[] = {".html", ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".mp3", ".pdf", ".ico"};
    
    char* content_types[] = {"text/html",
                             "image/jpeg",
                             "image/jpeg",
                             "image/png",
                             "image/bmp",
                             "image/gif",
                             "audio/mp3",
                             "application/pdf",
                             "image/x-icon"};

    // Get the file extension of the requested path
    char* ext = strrchr(path, '.');
    if (ext == NULL) {
        return "Unknown";
    }

    // Match the file extension to determine its content type
    for (int i = 0; i < sizeof(exts) / sizeof(exts[0]); ++i) {
        if (strcmp(exts[i], ext) == 0) {
            return content_types[i];
        }
    }

    return "Unknown";

}

int main(int argc, char **argv) {

    // Check if the port number is provided as a command-line argument
    if (argc < 2) {
        perror("port");
    }

    int server_socket, client_socket; /* listen on sockfd, new connection on new_fd */

    int port;

    char request[SIZE]; // Buffer to store the HTTP request message
    char response[SIZE]; // Buffer to store the HTTP response message

    struct sockaddr_in server_addr; /* my address */
    struct sockaddr_in client_addr; /* connector address*/

    // Create a socket for the server
    if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // Parse the port number from the command-line argument
    port = atoi(argv[1]);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the server address
    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(server_socket, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    while (1) {

        pid_t pid;
        // Fork a child process to handle each incoming connection
        if ((pid = fork()) < 0) {
            perror("fork");
            continue;
        }

        else if (pid > 0) {wait(NULL);}

        else {

            socklen_t sin_size = sizeof(struct sockaddr_in);

            // Accept an incoming connection request
            if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
                perror("accept");
                exit(1);
            }

            // Initialize request and response buffers
            memset(request, '\0', SIZE);
            memset(response, '\0', SIZE);

            // Read the HTTP request message from the client
            if (read(client_socket, request, SIZE) <= 0) {
                perror("read");
                exit(1);
            }

            // Print the received request message
            printf("%s", request);

            char path[SIZE];

            // Parse the request to extract the requested file path
            parse_request(request, path);

            FILE *file = fopen(path, "rb");

            int file_size = 0;

            // If the requested file does not exist, prepare a 404 Not Found response
            if (file == NULL) {
                sprintf(response, "HTTP/1.1 404 Not Found\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", file_size);
            }
            else {
                // If the file exists, determine its size and content type
                fseek(file, 0, SEEK_END);
                file_size = ftell(file);
                rewind(file);

                char *file_type = get_type(path);

                // Prepare a 200 OK response with content type and length headers
                sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", file_type, file_size);
            }

            int written_num;

            // Send the response message to the client
            written_num = write(client_socket, response, strlen(response));
            if (written_num < 0) {
                perror("write");
                exit(1);
            }

            char write_buffer[SIZE];
            memset(write_buffer, '\0', SIZE);

            // Send the content of the requested file to the client
            while ((file_size = fread(write_buffer, sizeof(char), SIZE, file)) > 0) {
                written_num = write(client_socket, write_buffer, file_size);
                if (written_num < 0) {
                    perror("write");
                    exit(1);
                }
            }

            // Close the file and the client socket
            fclose(file);

        }
    }

    // Close the socket
    close(server_socket);
    close(client_socket);

    return 0;
}