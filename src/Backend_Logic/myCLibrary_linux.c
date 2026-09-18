#define PY_SSIZE_T_CLEAN


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
// #include <signal.h> // for custom error handling
#include <string.h> // why this?

#include <fcntl.h> // file control operations
#include <sys/socket.h> // socket library 
#include <netdb.h> // definitions for network-based operations. gai_strerror, NI_NUMERICHOST, etc
#include <sys/select.h> // for waiting for socket to finish, since we're non-blocking :)


#include <openssl/ssl.h> // create ctx and verify
#include <openssl/err.h>
#include <openssl/crypto.h>


#define DEFAULT_BUFLEN 512
#define EXPECTED_MSG_SIZE 12000 


struct SSLConnection {
    SSL* ssl;
    SSL_CTX* ctx;
};

void init_openssl(){
    SSL_library_init();           // loads encryption algs
    SSL_load_error_strings();     // loads error strings
    OpenSSL_add_all_algorithms(); // "Add all ciphers and digests"
}

void cleanup_openssl() {
    EVP_cleanup(); // clean up ssl/tls info
}

int connectToServer(const char* host, const char* port){
    /*
    Connect To Server
    WHAT: This function will use provided host and port from user
        to return a non-blocking socket connected to the destination host   
        using the port provided by the user.
    RETURN: Returns the socket number, or 0 upon failure
    */    
    int my_socket;

    struct addrinfo hints, *infoptr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // don't care if using IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // stream-based connection (TCP)

    int result = getaddrinfo(host, port, &hints, &infoptr); // struct list of potential IPs

    // have we succeeded
    if(result){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
        exit(1);
    }

    struct addrinfo *p; // iterator
    // char host_ip[256], port_ip[256]; // just to print, not needed

    // fprintf(stderr, "Looping through possible connections...\n");

    for (p = infoptr; p != NULL; p = p->ai_next){

        // getnameinfo(p->ai_addr, p->ai_addrlen, host_ip, sizeof(host_ip), NULL, 0, NI_NUMERICHOST);
        // puts(host_ip);   //<-- was used for testing

        // fprintf(stderr, "Making socket...\n");

        my_socket = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol);
        if(my_socket == -1){
            perror("socket error");
            continue;
        }

        // fprintf(stderr, "Created socket, attempting connection...\n");

        int connect_prog = connect(my_socket, p->ai_addr, p->ai_addrlen);

        if(connect_prog == -1 && errno == EINTR){
            // interrupted before the attempt even got going, one free retry
            connect_prog = connect(my_socket, p->ai_addr, p->ai_addrlen);
        }

        if(connect_prog == -1 && errno == EINPROGRESS){
            int attempts = 3;

            while(attempts > 0){
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(my_socket, &wfds);

                struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };

                int sres = select(my_socket + 1, NULL, &wfds, NULL, &tv);

                if(sres < 0 && errno == EINTR){
                    continue; // interrupted mid-wait, doesn't cost an attempt
                }

                if(sres < 0){
                    connect_prog = -1; // select() itself failed, errno already set
                    break;
                }

                if(sres == 0){
                    attempts--; // timed out this round, wait again if attempts remain
                    continue;
                }

                if(sres > 0){
                    int so_error;
                    socklen_t len = sizeof(so_error);
                    if(getsockopt(my_socket, SOL_SOCKET, SO_ERROR, &so_error, &len) == -1){
                        connect_prog = -1;
                        break;
                    }

                    if(so_error == 0){
                        connect_prog = 0; // success
                    } else {
                        errno = so_error;  // so the perror() below reports the real cause
                        connect_prog = -1;
                    }

                    break;
                }

                attempts--;
            }

            if(attempts == 0 && connect_prog == -1){
                fprintf(stderr, "connect timed out waiting for socket to become ready\n");
            }
        }

        if(connect_prog == -1){
            // checks if we messed up or not
            perror("connect error");
            close(my_socket);
            continue;
        }

        // fprintf(stderr, "Connected.\n");

        break; // reaching here means we connected successfully.
    }

    freeaddrinfo(infoptr);

    if(p == NULL){
        // we reached the end of the struct addrinfo list, no connections.
        fprintf(stderr, "Couldnt find way to connect.\n");
        exit(1);
    }

    return my_socket;
}

struct SSLConnection ssl_context_wrap(int mySocket, const char* hostname){
    // cleanup needs to happen before exit

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if(ctx == NULL){
        fprintf(stderr, "Couldn't create SSL CTX pointer.\n");
        exit(1);
    }

    SSL_CTX_set_options(ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);

    // fprintf(stderr, "Created new context\n");
 
    SSL* ssl = SSL_new(ctx);
    if (ssl == NULL) {
        fprintf(stderr, "Couldn't create SSL pointer.\n");
        exit(1);
    }

    // fprintf(stderr, "Created new SSL item\n");

    if (SSL_set_fd(ssl, mySocket) != 1) {
        fprintf(stderr, "Failed to bind socket to SSL.\n");
        exit(1);
    }

    // fprintf(stderr, "Set ssl file descriptor to socket\n");

    SSL_set_tlsext_host_name(ssl, hostname);

    // fprintf(stderr, "Set up external host name\n");    

    int ret;
    while ((ret = SSL_connect(ssl)) != 1) {
        int err = SSL_get_error(ssl, ret);

        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(mySocket, &fds);
            struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };

            int sres;
            if (err == SSL_ERROR_WANT_READ) {
                sres = select(mySocket + 1, &fds, NULL, NULL, &tv);   // wait for readable
            } else {
                sres = select(mySocket + 1, NULL, &fds, NULL, &tv);   // wait for writable
            }

            if (sres < 0 && errno == EINTR) {
                continue; // interrupted, doesn't count against us — just retry
            }
            if (sres <= 0) {
                fprintf(stderr, "Timed out waiting for TLS handshake to progress\n");
                exit(1);
            }
            continue; // socket's ready — try SSL_connect() again
        }

        // anything else here is a genuine TLS failure (bad cert, protocol mismatch, etc.)
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    // fprintf(stderr, "Connected SSL socket, finished TLS handshake\n");

    struct SSLConnection myConn;
    myConn.ssl = ssl;
    myConn.ctx = ctx;

    return myConn;
}

int sendDataToServer(void* ssl, int serverSocket, char* sendMe){
    /*
    Send Data To Server

    The following function will take in a serverSocket as input
        and a message to send and will send the message to the server.
    Upon success, the number of bytes sent will be returned.
    Upon failure, -1 will be returned.
    */
    // Send request -------------------------------------------------------------------------------
    
    int sendAmount = 0;;
    int totalSent = 0;

    SSL* mySSL = (SSL*)ssl;
    // Send an initial buffer
    // fprintf(stderr, "The message: %.*s", strlen(sendMe), sendMe);
    do{
        sendAmount = SSL_write(mySSL, sendMe + totalSent, (int) strlen(sendMe) - totalSent);
        // fprintf(stderr, "Just sent: %d bytes", sendAmount);
        if (sendAmount == -1) {
            perror("send error");
            close(serverSocket);
            return -1;
        }

        totalSent += sendAmount;
    }while(sendAmount < strlen(sendMe));
    
    // caused errors for reading, im using tls so dont shut down socket raw.
    // int shutdownResult = shutdown(serverSocket, SHUT_WR); // can no longer write to server, flushes buffer.
    // if(shutdownResult == -1){
    //     perror("shutdown failed after trying to close write-side pipe.");
    //     close(serverSocket);
    //     return -1;
    // }
    // fprintf(stdout, "\nFinished send. Total sent bytes: %d\n", totalSent);
    return sendAmount;
}

char* recvDataFromServer(void* ssl){
    // receive data --------------------------------------------------------------------------
    // create shared memory and set it to appropriate length

    // TO DO:
    // look for cybersec concerns when you finish this part.

    // user json.load in python to load data as list of lists. Use strstr() to skip past header from received data. return to python script
    // create a free function to free the malloc'd buffer from python script.

    int amountReceived = 0;
    int TOTALAmountReceived = 0;
    char* buffer = (char*)malloc(DEFAULT_BUFLEN);
    int buffLen = DEFAULT_BUFLEN;
    SSL* mySSL = (SSL*)ssl;

    while(1){
        // RECEIVE DATA --------------------------------------------------------
        // for loop used in case realloc had a transient issue (will pass)
        // fprintf(stderr, "Getting data....\n");
        if(TOTALAmountReceived + DEFAULT_BUFLEN > buffLen){
            char* newBuff = realloc(buffer, buffLen * 2);
            // check this code again, chance that you get too much data, realloc, and realloc to a different location and miss data
            if(newBuff != NULL){
                buffer = newBuff;
                buffLen *= 2;
            }
            else{
                fprintf(stderr, "Error: Not enough memory. Realloc failed.");
                free(buffer);
                return NULL;
            }
        }

        amountReceived = SSL_read(mySSL, buffer + TOTALAmountReceived, buffLen - TOTALAmountReceived);
        // fprintf(stderr, "Received: %d\n", amountReceived);
        int err =  SSL_get_error(mySSL, amountReceived);
        // fprintf(stderr, "Err: %d\n", err);

        if(amountReceived <= 0){
            // error occured, see if we can try again 
            if(err == SSL_ERROR_ZERO_RETURN){
                // server finished sending data
                // potentially... check this later
                // fprintf(stderr, "Received Everthing!\n");
                break;
            }
            if(err == SSL_ERROR_WANT_READ){
                // interrupt, try again
                // only works if socket is non-blocking
                continue;
            }
            // for any other error, likely hard to fix, ignore it for now
            ERR_print_errors_fp(stderr);
            // closesocket(serverSocket);
            free(buffer);
            return NULL;
        }

        TOTALAmountReceived += amountReceived;
    }

        if (TOTALAmountReceived >= buffLen) {
            fprintf(stdout, "Reallocating Buffer...\n");
            char* newBuff = realloc(buffer, buffLen + 1);
            if (!newBuff) {
                fprintf(stderr, "Error: Final realloc failed.\n");
                free(buffer);
                return NULL;
            }
            buffer = newBuff;
        }


    buffer[TOTALAmountReceived] = '\0';
    // fprintf(stdout, "Finished Receiving data, total amount received: %d\n", TOTALAmountReceived);
    return buffer;
}

void freeBuffer(char* buffer){
    free(buffer);
}

int cleanUp(struct SSLConnection myConn, int serverSocket){
    // DISCONNECT -------------------------------------------------------------
    int shutdownResult = shutdown(serverSocket, SHUT_RD);
    if(shutdownResult == -1){
        perror("shutdown SHUT_RD failed");
        close(serverSocket);
        return 1;
    }

    // CLEAN UP ----------------------------------------------------------------
    close(serverSocket); // Close the TCP socket

    SSL_shutdown(myConn.ssl);     // Gracefully close TLS session
    SSL_free(myConn.ssl);         // Free the SSL object
    SSL_CTX_free(myConn.ctx);     // Free the SSL context
    cleanup_openssl();     // Cleanup OpenSSL state

    return 0;
}

char* remove_header(char* buffer){
    // will not free buffer upon failure, will let user 
    // take care of that.

    // fprintf(stderr, "REMOVING HEADER FOR:\n\n%s", buffer);

    char* headerEnd = strstr(buffer, "\r\n\r\n");
    if(headerEnd){
        char* message_size_start = headerEnd + 4;
        // read sizeof(int) starting at message_size_start
        // fprintf(stderr, "read sizeof(int) starting at message_size_start\n");
        char* sizeEndPtr;
        long chunkSize = strtol(message_size_start, &sizeEndPtr, 16); // base 16: parses "2781" as hex

        if(sizeEndPtr == message_size_start){
            // no hex digits found at all — malformed chunk header
            free(buffer);
            return NULL;
        }

        // skip +2 (to skip \r\n)
        // fprintf(stderr, "skip +2\n");
        char* data_begin = sizeEndPtr + 2;

        // have a pointer end at start of \r\n0\r\n\r\n
        // fprintf(stderr, "have a pointer end at start of end");
        char* dataEnd = strstr(data_begin, "\r\n0\r\n\r\n");
        if(!dataEnd){
            // terminator not found — response was cut off or malformed
            free(buffer);
            return NULL;
        }

        //copy entire message from beginning to end pointers to a different buffer
        // fprintf(stderr, "copy entire message from beginning to end pointers to a different buffer\n");
        size_t msgLen = dataEnd - data_begin;
        char* newBuffer = malloc(msgLen + 1);
        if(!newBuffer){
            fprintf(stderr, "Error: malloc failed for dechunked buffer.\n");
            free(buffer);
            return NULL;
        }
        memcpy(newBuffer, data_begin, msgLen);
        newBuffer[msgLen] = '\0';

        // free original buffer
        // fprintf(stderr, "free original buffer and return\n");
        free(buffer);
        // return new buffer
        return newBuffer;
    }else{
        // data formatted weirdly, fail :(
        free(buffer);
        return NULL;
    }
}


#ifdef TEST_MAIN
int main(void) {
    const char *test_host = "api.jolpi.ca";
    const char *test_port = "443";
    char test_request[] = 
        "GET /ergast/f1/current/driverstandings/?format=json HTTP/1.1\r\n"
        "Host: api.jolpi.ca\r\n"
        "User-Agent: F1WdcTracker/0.1\r\n"
        "Connection: close\r\n"
        "\r\n";

    init_openssl();

    int sock = connectToServer(test_host, test_port);
    if (sock <= 0) {
        fprintf(stderr, "Failed to connect to server.\n");
        return 1;
    }

    struct SSLConnection conn = ssl_context_wrap(sock, test_host);

    int sent = sendDataToServer(conn.ssl, sock, test_request);
    if (sent <= 0) {
        fprintf(stderr, "Failed to send data.\n");
        cleanUp(conn, sock);
        return 1;
    }

    char *raw_response = recvDataFromServer(conn.ssl);
    if (!raw_response) {
        fprintf(stderr, "Failed to receive data.\n");
        cleanUp(conn, sock);
        return 1;
    }

    char *json_body = remove_header(raw_response);
    if (json_body) {
        printf("\nDe-chunked JSON Payload Received:\n%.200s...\n\n", json_body);
        freeBuffer(json_body);
    } else {
        fprintf(stderr, "Failed to parse header or de-chunk response.\n");
    }

    cleanUp(conn, sock);

    printf("Test completed successfully without crashing.\n");
    return 0;
}
#endif