#define PY_SSIZE_T_CLEAN
//#include <Python.h>


#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> // file control operations
#include <winsock2.h> // for windows, not linux
#include <ws2tcpip.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "443"
#define HOST 'api.jolpi.ca'
// #define HOST 'localhost'
#define DEFAULT_BUFLEN 512
#define EXPECTED_MSG_SIZE 31000 // 31kB

// errors:
#define CONNECTION_ERROR 1
#define WSASTARTUP_ERROR 2
#define GETADDRINFO_ERROR 3
#define SEND_FAIL 4
#define SHUTDOWN_ERROR 5
#define RECV_ERROR 6

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

// /*
// Connect To Server
// This function will use provided host and port from user
//     to return a socket connected to the destination host   
//     using the port provided by the user.
// Upon failure the function will return -1.
// */

void init_openssl(){
    SSL_library_init();           // loads encryption algs
    SSL_load_error_strings();     // loads error strings
    OpenSSL_add_all_algorithms(); // "Add all ciphers and digests"
}

void cleanup_openssl() {
    EVP_cleanup();
}

SOCKET connectToServer(const char* host, const char* port){
    WSADATA wsaData; // init WSAData obj

    int iResult; // init winsock and check for errors

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", iResult);
        return ~0;
    }

    struct addrinfo hints, *result, *ptr;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_UNSPEC; // unspecified, so IPv4 and IPv6 are fine
    hints.ai_socktype = SOCK_STREAM; //TCP
    hints.ai_protocol = IPPROTO_TCP; // TCP Protocol

    iResult = getaddrinfo(host, port, &hints, &result);
    if(iResult != 0){
        fprintf(stderr, "getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return ~0;
    }

    SOCKET serverSocket = INVALID_SOCKET;
    
    ptr = result;

    for(; ptr != NULL; ptr = ptr->ai_next){ //trying as many addresses as possible
        serverSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if(serverSocket == INVALID_SOCKET){
            continue;
        }

        iResult = connect( serverSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if(iResult != SOCKET_ERROR){
            break; // found a valid address!
        }

        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if(iResult == INVALID_SOCKET){
        fprintf(stderr, "Unable to connect to server!\n");
        WSACleanup();
        return ~0;
    }

    return serverSocket;
}

struct SSLConnection ssl_context_wrap(SOCKET mySocket){
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

int sendDataToServer(void* ssl, SOCKET serverSocket, char* sendMe){
    /*************************************************************
    Send Data To Server

    The following function will take in a serverSocket as input
    and a message to send and will send the message to the server.
    Upon success, the number of bytes sent will be returned.
    Upon failure, -1 will be returned.
    *************************************************************/
    // Send request -------------------------------------------------------------------------------
    
    int sendAmount = 0;;
    int totalSent = 0;

    SSL* mySSL = (SSL*)ssl;
    // Send an initial buffer
    do{
        sendAmount = SSL_write(mySSL, sendMe + totalSent, (int) strlen(sendMe) - totalSent);
        if (sendAmount == SOCKET_ERROR) {
            printf("send failed: %d\n", WSAGetLastError());
            // closesocket(serverSocket);
            WSACleanup();
            return -1;
        }

        totalSent += sendAmount;
    }while(sendAmount < strlen(sendMe));

    int shutdownResult = shutdown(serverSocket, SD_SEND);
    if(shutdownResult == SOCKET_ERROR){
        fprintf(stderr, "shutdown SEND failed: %d\n", WSAGetLastError());
        // closesocket(serverSocket);
        WSACleanup();
        return -1;
    }

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
        if(TOTALAmountReceived + DEFAULT_BUFLEN > buffLen){
            char* newBuff = realloc(buffer, buffLen * 2);
            if(newBuff != NULL){
                buffer = newBuff;
                buffLen *= 2;
            }
            else{
                fprintf(stderr, "Error: Not enough memory. Realloc failed.");
                free(buffer);
                WSACleanup();
                return NULL;
            }
        }
        //-----------------------------------------------------------------------
        amountReceived = SSL_read(mySSL, buffer + TOTALAmountReceived, buffLen - TOTALAmountReceived);
        int err =  SSL_get_error(mySSL, amountReceived);
        if(amountReceived == 0 && err == 0){
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
    }

    if (TOTALAmountReceived >= buffLen) {
        char* newBuff = realloc(buffer, buffLen + 1);
        if (!newBuff) {
            fprintf(stderr, "Error: Final realloc failed.\n");
            free(buffer);
            WSACleanup();
            return NULL;
        }
        buffer = newBuff;
    }
    buffer[TOTALAmountReceived] = '\0';

    return buffer;
}

void freeBuffer(char* buffer){
    free(buffer);
}

int cleanUp(struct SSLConnection myConn, SOCKET serverSocket){
    // DISCONNECT -------------------------------------------------------------
    int shutdownResult = shutdown(serverSocket, SD_RECEIVE);
    if(shutdownResult == SOCKET_ERROR){
        fprintf(stderr, "shutdown RECV failed: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // CLEAN UP ----------------------------------------------------------------
    closesocket(serverSocket); // Close the TCP socket
    WSACleanup();

    SSL_shutdown(myConn.ssl);     // Gracefully close TLS session
    SSL_free(myConn.ssl);         // Free the SSL object
    SSL_CTX_free(myConn.ctx);     // Free the SSL context
    cleanup_openssl();     // Cleanup OpenSSL state


    return 0;
}

char* testData(){
    // data = [
    // {"id": 1, "name": "Alice", "score": 92},
    // {"id": 2, "name": "Bob", "score": 85},
    // ]

    return NULL;
}