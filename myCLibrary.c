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

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "8000"
#define HOST = 'http://api.jolpi.ca/ergast/f1/current/driverStandings'
#define DEFAULT_BUFLEN 512
#define EXPECTED_MSG_SIZE 31000 // 31kB
#define sharedMemName "/my_shared_mem"

/*
To compile:
    gcc -shared -o myCLibary.so myCLibary.c
*/



/*
Connect To Server
This function will use provided host and port from user
    to return a socket connected to the destination host   
    using the port provided by the user.
Upon failure the function will return -1.
*/

int connectToServer(const char* host, const char* port){
    WSADATA wsaData; // init WSAData obj

    int iResult; // init winsock and check for errors

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", iResult);
        return -1;
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
        return -1;
    }

    SOCKET ConnectSocket = INVALID_SOCKET;
    
    ptr = result;
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

    for(ptr = result; ptr != NULL; ptr = ptr->ai_next){ //trying as many addresses as possible
        iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if(iResult != INVALID_SOCKET){
            break; // found a valid address!
        }
    }

    freeaddrinfo(result);

    if(iResult == INVALID_SOCKET){
        fprintf(stderr, "Unable to connect to server!\n");
        WSACleanup();
        return -1;
    }

    return ConnectSocket;
}

/*
Send Data To Server

The following function will take in a serverSocket as input
and a message to send and will send the message to the server.
Upon success, the number of bytes sent will be returned.
Upon failure, -1 will be returned.
*/

int sendDataToServer(int serverSocket, char* sendMe){
     // Send request -------------------------------------------------------------------------------
    
    char recvbuff[DEFAULT_BUFLEN];

    int sendAmount;

    // Send an initial buffer
    do{
        sendAmount = send(serverSocket, sendMe, (int) strlen(sendMe), 0);
        if (sendAmount == SOCKET_ERROR) {
            printf("send failed: %d\n", WSAGetLastError());
            closesocket(serverSocket);
            WSACleanup();
            return -1;
        }
    }while(sendAmount < strlen(sendMe));

    int shutdownResult = shutdown(serverSocket, SD_SEND);
    if(shutdownResult == SOCKET_ERROR){
        fprintf(stderr, "shutdown SEND failed: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }

    return sendAmount;
}

int recvDataFromServer(int serverSocket){
    // receive data --------------------------------------------------------------------------
    // create shared memory and set it to appropriate length

    // TO DO:
    // create buffer, realloc when it's full. Have a max buffer size so you don't overload.
    // look for cybersec concerns when you finish this part.

    // user json.load in python to load data as list of lists. Use strstr() to skip past header from received data. return to python script
    // create a free function to free the malloc'd buffer from python script.

    int amountReceived = 0;
    int TOTALAmountReceived = 0;

    while(true){
        // RECEIVE DATA ---------------------------------------------
        amountReceived = recv(serverSocket, recvbuff, recvbuflen, 0);
        if(amountReceived == 0){
            // server finished sending data
            break;
        }
        if(amountReceived < 0 && errno == WSAEWOULDBLOCK){
            // interrupt, try again
            continue;
        }
        if(amountReceived < 0){
            // error
            fprintf(stderr, "recv failed: %d\n", WSAGetLastError());
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }
        
        // APPEND DATA TO SHARED MEMORY --------------------------------
        memcpy((char*)ptr + TOTALAmountReceived, recvbuff, amountReceived);
        TOTALAmountReceived += amountReceived;
    }

    return 0;
}

int cleanUp(){
    // DISCONNECT -------------------------------------------------------------
    shutdownResult = shutdown(ConnectSocket, SD_SEND);
    if(shutdownResult == SOCKET_ERROR){
        fprintf(stderr, "shutdown RECV failed: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // CLEAN UP ----------------------------------------------------------------
    closeoscket(ConnectSocket);
    WSACleanup();

    if (munmap(ptr, EXPECTED_MSG_SIZE) == -1) {
        fprintf(stderr, "munmap Shared Memory Failed: %d\n", WSAGetLastError());
        return 1;
    }

    if (close(sharedMemFD) == -1) {
        fprintf(stderr, "close sharedMemory File Descriptor Failed: %d\n", WSAGetLastError());
        return 1;
    }

    return 0;
}

// int execute(){

//     connectToServer();

//     sendDataToServer();

//     recvDataFromServer();

//     cleanUp();
// }

// static PyObject * myModule_getData(PyObject *self, PyObject *args){
//     // arguments should be null, server and port are static.
//     execute();
// }