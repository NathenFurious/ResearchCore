#ifndef TLS_H
#define TLS_H

#define SECURITY_WIN32
#include <windows.h>
#include <schnlsp.h>
#include <schannel.h>
#include <security.h>
#include <stddef.h>

typedef struct TLSContext {
    SOCKET socket;
    CredHandle creds;
    CtxtHandle ctx;
    SecPkgContext_StreamSizes sizes;
    int connected;
} TLSContext;

// Global setup (Schannel requires no global init, kept for API parity)
int tls_init(void);
void tls_cleanup(void);

// Connection management
TLSContext* tls_connect(SOCKET socket_fd, const char* hostname);
void tls_close(TLSContext* tls);

// Encrypted I/O operations
int tls_write(TLSContext* tls, const char* data, size_t len);
int tls_read(TLSContext* tls, char* buffer, size_t len);

#endif // TLS_H
