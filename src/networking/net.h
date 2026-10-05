#ifndef NET_H
#define NET_H

#include <stddef.h>

// Connects to host:port, sends an HTTP GET request, and allocates heap memory for response.
// Returns a pointer to the raw response string, or NULL on failure.
char* net_http_get(const char* host, int port, const char* path);

#endif // NET_H
