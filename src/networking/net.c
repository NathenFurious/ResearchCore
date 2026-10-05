#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

char* net_http_get(const char* host, int port, const char* path) {
    // 1. Resolve host name to IP address
    struct hostent* server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "Error: Could not resolve hostname %s\n", host);
        return NULL;
    }

    // 2. Set up socket address structure
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    // 3. Create TCP socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Error creating socket");
        return NULL;
    }

    // 4. Connect to server
    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Error connecting to server");
        close(sock_fd);
        return NULL;
    }

    // 5. Construct HTTP GET request buffer
    char request[512];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n\r\n",
             path, host);

    // 6. Send request bytes across network
    if (send(sock_fd, request, strlen(request), 0) < 0) {
        perror("Error sending request");
        close(sock_fd);
        return NULL;
    }

    // 7. Receive response chunks into heap buffer using pointer arithmetic
    size_t capacity = 4096;
    size_t total_bytes = 0;
    char* response = (char*)malloc(capacity);
    if (response == NULL) {
        close(sock_fd);
        return NULL;
    }

    char chunk[1024];
    ssize_t bytes_read = 0;

    while ((bytes_read = recv(sock_fd, chunk, sizeof(chunk), 0)) > 0) {
        // Expand memory if buffer is full
        if (total_bytes + bytes_read >= capacity) {
            capacity *= 2;
            char* new_response = (char*)realloc(response, capacity);
            if (new_response == NULL) {
                free(response);
                close(sock_fd);
                return NULL;
            }
            response = new_response;
        }

        // Copy chunk bytes directly to offset pointer address
        memcpy(response + total_bytes, chunk, bytes_read);
        total_bytes += bytes_read;
    }

    response[total_bytes] = '\0'; // Null-terminate string response
    close(sock_fd);
    return response;
}
