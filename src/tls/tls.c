#include "tls.h"
#include <stdio.h>
#include <stdlib.h>

int tls_init(void) { 
    return 1; 
}

void tls_cleanup(void) {}

TLSContext* tls_connect(SOCKET socket_fd, const char* hostname) {
    TLSContext* tls = (TLSContext*)calloc(1, sizeof(TLSContext));
    if (!tls) return NULL;
    tls->socket = socket_fd;

    // 1. Acquire Schannel credentials
    SCHANNEL_CRED cred_data = {0};
    cred_data.dwVersion = SCHANNEL_CRED_VERSION;
    cred_data.dwFlags = SCH_USE_STRONG_CRYPTO | SCH_CRED_AUTO_CRED_VALIDATION;

    if (AcquireCredentialsHandleA(NULL, UNISP_NAME_A, SECPKG_CRED_OUTBOUND, 
                                 NULL, &cred_data, NULL, NULL, 
                                 &tls->creds, NULL) != SEC_E_OK) {
        free(tls);
        return NULL;
    }

    // 2. Perform Schannel Handshake Loop
    DWORD flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | 
                  ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY | 
                  ISC_REQ_STREAM;

    SecBuffer out_buf = {0, SECBUFFER_TOKEN, NULL};
    SecBufferDesc out_desc = {SECBUFFER_VERSION, 1, &out_buf};
    DWORD context_flags;

    SECURITY_STATUS status = InitializeSecurityContextA(
        &tls->creds, NULL, (SEC_CHAR*)hostname, flags, 0, 0, 
        NULL, 0, &tls->ctx, &out_desc, &context_flags, NULL);

    if (status == SEC_I_CONTINUE_NEEDED) {
        // Send handshake token to server
        send(tls->socket, out_buf.pvBuffer, out_buf.cbBuffer, 0);
        FreeContextBuffer(out_buf.pvBuffer);
    } else {
        FreeCredentialsHandle(&tls->creds);
        free(tls);
        return NULL;
    }

    // Query stream sizes for header/trailer sizes needed during encrypt/decrypt
    QueryContextAttributesA(&tls->ctx, SECPKG_ATTR_STREAM_SIZES, &tls->sizes);
    tls->connected = 1;
    return tls;
}

int tls_write(TLSContext* tls, const char* data, size_t len) {
    if (!tls || !tls->connected) return -1;

    // Allocate memory for Header + Payload + Trailer
    size_t total_size = tls->sizes.cbHeader + len + tls->sizes.cbMaximumSignature;
    char* buffer = (char*)malloc(total_size);
    if (!buffer) return -1;

    memcpy(buffer + tls->sizes.cbHeader, data, len);

    SecBuffer buffers[4] = {
        { tls->sizes.cbHeader, SECBUFFER_STREAM_HEADER, buffer },
        { (DWORD)len, SECBUFFER_DATA, buffer + tls->sizes.cbHeader },
        { tls->sizes.cbMaximumSignature, SECBUFFER_STREAM_TRAILER, buffer + tls->sizes.cbHeader + len },
        { 0, SECBUFFER_EMPTY, NULL }
    };
    SecBufferDesc desc = { SECBUFFER_VERSION, 4, buffers };

    if (EncryptMessage(&tls->ctx, 0, &desc, 0) != SEC_E_OK) {
        free(buffer);
        return -1;
    }

    int sent = send(tls->socket, buffer, buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer, 0);
    free(buffer);
    return sent;
}

int tls_read(TLSContext* tls, char* buffer, size_t len) {
    if (!tls || !tls->connected) return -1;

    char raw_buf[4096];
    int bytes_received = recv(tls->socket, raw_buf, sizeof(raw_buf), 0);
    if (bytes_received <= 0) return bytes_received;

    SecBuffer buffers[4] = {
        { (DWORD)bytes_received, SECBUFFER_DATA, raw_buf },
        { 0, SECBUFFER_EMPTY, NULL },
        { 0, SECBUFFER_EMPTY, NULL },
        { 0, SECBUFFER_EMPTY, NULL }
    };
    SecBufferDesc desc = { SECBUFFER_VERSION, 4, buffers };

    if (DecryptMessage(&tls->ctx, &desc, 0, NULL) != SEC_E_OK) {
        return -1;
    }

    // Find decrypted data buffer
    for (int i = 0; i < 4; i++) {
        if (buffers[i].BufferType == SECBUFFER_DATA) {
            size_t copy_size = buffers[i].cbBuffer < len ? buffers[i].cbBuffer : len;
            memcpy(buffer, buffers[i].pvBuffer, copy_size);
            return (int)copy_size;
        }
    }
    return 0;
}

void tls_close(TLSContext* tls) {
    if (!tls) return;
    DeleteSecurityContext(&tls->ctx);
    FreeCredentialsHandle(&tls->creds);
    free(tls);
}
