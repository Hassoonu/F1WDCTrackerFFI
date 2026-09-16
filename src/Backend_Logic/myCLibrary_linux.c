#define PY_SSIZE_T_CLEAN
//#include <Python.h>


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h> // why this?
#include <string.h> // why this?

#include <fcntl.h> // file control operations
#include <sys/socket.h> // socket library 
#include <netdb.h> // definitions for network-based operations. gai_strerror, NI_NUMERICHOST, etc


#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>


#define DEFAULT_BUFLEN 512
#define EXPECTED_MSG_SIZE 31000 // 31kB

// errors:
// #define SOCKET_CREATION_ERROR 1
// #define CONNECTION_ERROR 2
// #define WSASTARTUP_ERROR 3
// #define GETADDRINFO_ERROR 4
// #define SEND_FAIL 5
// #define SHUTDOWN_ERROR 6
// #define RECV_ERROR 7


/*
To compile:
    gcc -shared -o myCLibary.so myCLibary.c
*/


// struct Result {
//     int succeed;
//     int errorCode;
//     SOCKET socket;

// };


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
    EVP_cleanup();
}


struct addrinfo hints, *infoptr;


int connectToServer(const char* host, const char* port){
    /*
    Connect To Server
    WHAT: This function will use provided host and port from user
        to return a socket connected to the destination host   
        using the port provided by the user.
    RETURN: Returns the socket number, or 0 upon failure
*/    
    int my_socket;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // don't care if using IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // stream-based connection (TCP)

    int result = getaddrinfo(host, NULL, &hints, &infoptr); // struct list of potential IPs

    // have we succeeded
    if(result){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
        exit(1);
    }

    struct addrinfo *p; // iterator
    // char host_ip[256], port_ip[256]; // just to print, not needed

    for (p = infoptr; p != NULL; p = p->ai_next){
        // getnameinfo(p->ai_addr, p->ai_addrlen, host_ip, sizeof(host_ip), NULL, 0, NI_NUMERICHOST);
        // puts(host_ip);   <-- was used for testing

        my_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol); //SOCK_NONBLOCK
        if(my_socket == -1){
            perror("socket error");
            continue;
        }

        if(connect(my_socket, p->ai_addr, p->ai_addrlen) == -1){
            perror("connect error");
            close(my_socket);
            continue;
        }

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

struct SSLConnection ssl_context_wrap(int mySocket){
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, mySocket);

    if (SSL_connect(ssl) <= 0) { // TCP handshake
        ERR_print_errors_fp(stderr); // Print any handshake errors
        exit(1);
    }

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
    fprintf(stderr, "The message: %.*s", strlen(sendMe), sendMe);
    do{
        sendAmount = SSL_write(mySSL, sendMe + totalSent, (int) strlen(sendMe) - totalSent);
        fprintf(stderr, "Just sent: %d bytes", sendAmount);
        if (sendAmount == SOCKET_ERROR) {
            printf("send failed: %d\n", WSAGetLastError());
            close(serverSocket);
            return -1;
        }

        totalSent += sendAmount;
    }while(sendAmount < strlen(sendMe));
    
    int shutdownResult = shutdown(serverSocket, SD_SEND);
    if(shutdownResult == SOCKET_ERROR){
        fprintf(stderr, "shutdown SEND failed: %d\n", WSAGetLastError());
        close(serverSocket);
        return -1;
    }
    fprintf(stderr, "Finished send. Total sent bytes: %d\n", totalSent);
    return sendAmount;
}

char* recvDataFromServer(void* ssl){
    // receive data --------------------------------------------------------------------------
    // create shared memory and set it to appropriate length

    // TO DO:
    // create buffer, realloc when it's full. Have a max buffer size so you don't overload.
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
        fprintf(stderr, "Getting data....\n");
        if(TOTALAmountReceived + DEFAULT_BUFLEN > buffLen){
            char* newBuff = realloc(buffer, buffLen * 2);
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
        //-----------------------------------------------------------------------
        amountReceived = SSL_read(mySSL, buffer + TOTALAmountReceived, buffLen - TOTALAmountReceived);
        fprintf(stderr, "Received: %d\n", amountReceived);
        int err =  SSL_get_error(mySSL, amountReceived);
        fprintf(stderr, "Err: %d\n", err);
        fprintf(stderr, "%.*s\n", amountReceived, buffer);
        if(amountReceived == 0 && err >= 0){
            // server finished sending data
            fprintf(stderr, "Received Everthing!\n");
            break;
        }
        if(amountReceived == SOCKET_ERROR && (err == WSAEWOULDBLOCK || err == WSAEINTR)){
            // interrupt, try again
            continue;
        }
        if(err == WSAECONNRESET){
            fprintf(stderr, "Error: Server closed connection abruptly. WSAECONNRESET");
            free(buffer);
            WSACleanup();
            return NULL;
        }
        if(amountReceived < 0){
            // error
            fprintf(stderr, "recv failed: %d\n", WSAGetLastError());
            // closesocket(serverSocket);
            free(buffer);
            WSACleanup(); // make function to free buffer and run WSACleanup().
            return NULL;
        }
        TOTALAmountReceived += amountReceived;

        if (TOTALAmountReceived >= buffLen) {
            fprintf(stderr, "Reallocating Buffer...\n");
            char* newBuff = realloc(buffer, buffLen + 1);
            if (!newBuff) {
                fprintf(stderr, "Error: Final realloc failed.\n");
                free(buffer);
                WSACleanup();
                return NULL;
        }
        buffer = newBuff;
    }
    
    }

    buffer[TOTALAmountReceived] = '\0';
    fprintf(stderr, "Finished Receiving data, total amount received: %d\n", TOTALAmountReceived);
    return buffer;
}

void freeBuffer(char* buffer){
    free(buffer);
}

int cleanUp(struct SSLConnection myConn, int serverSocket){
    // DISCONNECT -------------------------------------------------------------
    int shutdownResult = shutdown(serverSocket, SD_RECEIVE);
    if(shutdownResult == SOCKET_ERROR){
        fprintf(stderr, "shutdown RECV failed: %d\n", WSAGetLastError());
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

// char* testData(){
//     // data = [
//     // {"id": 1, "name": "Alice", "score": 92},
//     // {"id": 2, "name": "Bob", "score": 85},
//     // ]

//     return NULL;
// }