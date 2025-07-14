import ctypes # allows me to import a c file 
import asyncio
import os
import subprocess
import json

DEFAULT_PORT = '443'
# host = 'api.jolpi.ca'
host = 'localhost'
DEFAULT_BUFLEN = 512
EXPECTED_MSG_SIZE = 31000 # 31kB
# sharedMemName = "SharedMemory"
myMessage = "GET /ergast/f1/2025/driverstandings/?format=api HTTP/1.1\r\n" \
            "Host: api.jolpi.ca\r\n" \
            "User-Agent: my-openssl-client/1.0\r\n" \
            "Connection: keep-alive\r\n" \
            "\r\n"

#test server command: nc -l -p 1234 -e /bin/cat -k

# class Result(ctypes.Structure):
#     _fields_ = [
#         ("succeed"),
#         ("errorCode"),
#         ("")
#     ]

class SSLConnection(ctypes.Structure):
    _fields_ = [
        ("ssl", ctypes.c_void_p),
        ("ctx", ctypes.c_void_p)
    ]

class APIError(Exception):
    """Base class for API-related errors."""
    pass

class ConnectionError(APIError):
    pass

class TimeoutError(APIError):
    pass

class InvalidDataError(APIError):
    pass

class SendError(APIError):
    pass

class RecvError(APIError):
    pass

class CleanupError():
    pass

class GeneralError(APIError):
    pass


def check_error(code):
    if code == 0:
        return
    elif code == 1:
        raise ConnectionError()
    elif code == 2:
        raise TimeoutError()
    elif code == 3:
        raise InvalidDataError()
    elif code == 4:
        raise SendError()
    elif code == 5:
        raise RecvError()    
    elif code == 6:
        raise CleanupError()
    elif code == 2:
        raise GeneralError()
    

def getAPIData(host, port, clib):
    # ----- Declare all foreign functions (FFIs) that will be used -----

    initSSL =      clib.init_openssl
    connectToAPI = clib.connectToServer
    contextWrap =  clib.ssl_context_wrap
    sendRequest =  clib.sendDataToServer
    recvData =     clib.recvDataFromServer
    clean =        clib.cleanUp
    
    # ------------------------------------------------------------
    # ----- Declare all argument and return types for FFIs -----

    initSSL.argtypes = None
    initSSL.restype = None

    connectToAPI.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    connectToAPI.restype = ctypes.c_size_t

    contextWrap.argtypes = [ctypes.c_size_t]
    contextWrap.restype = SSLConnection

    sendRequest.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_char_p]
    sendRequest.restype = ctypes.c_int

    recvData.argtypes = [ctypes.c_void_p]
    recvData.restype = ctypes.c_char_p

    clean.argtypes = [SSLConnection, ctypes.c_size_t]
    clean.restype = ctypes.c_int

    # -----------------------------------------------------------------
    initSSL() # init all needed libraries for secure socket connection
    # -----------------------------------------------------------------

    # Get socket to connect to API:
    connectionSocket = connectToAPI( host.encode('utf-8') , port.encode('utf-8') )
    if(connectionSocket == ~0):
        check_error(1)
    # -----------------------------------------------------------------

    # Wrap connected socket with TCP and a context wrap:
    connection = contextWrap(connectionSocket)
    # -----------------------------------------------------------------

    # Send data to server:
    amountSent = sendRequest(connection.ssl, connectionSocket, myMessage.encode('utf-8') )
    if(amountSent <= 0):
        check_error(4)
        clean(connectionSocket)
        return None

    # -----------------------------------------------------------------

    # Receive Data:
    dataString = recvData(connection.ssl)
    if(dataString == None):
        check_error(5)
        clean(connectionSocket)
        return None

    # -----------------------------------------------------------------
    
    # Convert data from JSON to Python tables
    print("recv'd: ", dataString)
    try:
        convertedData = json.loads(dataString.decode('utf-8'))
    except Exception as e:
        clean(connection, connectionSocket)
        return dataString


    # -----------------------------------------------------------------

    # Clean up sockets and close connections.
    cleanStatus = clean(connection, connectionSocket)
    if(cleanStatus != 0):
        check_error(6)
        return None
    
    return convertedData

def main():
    lib = ctypes.CDLL('./myCLibrary.dll')
    # print("Opened Library\n")
    # Part 1: Get data from C function, do this asynchronously so that you load electronJS while this is communicating with API
    data = getAPIData(host, DEFAULT_PORT, lib)
    # Part 1 Async: Load ElectronJS output.
    # electronPath = "C:/Users/jolpi/Documents/Projects/F1WDCTrackerFFI"
    # main_js_path = os.path.join(electronPath, "main.js")
    # subprocess.run(["npx", "electron", main_js_path], cwd=electronPath)
    # Part 2: If data loaded, give electronJS data, else put down a loading screen/loop in electronJS output

    # Part 3: conditional if we're waiting for data, once data arrives, load it to screen


    freeBuffer =   lib.freeBuffer
    freeBuffer.argtypes = [ctypes.c_char_p]

    freeBuffer(data)

    return 0

if __name__ == "__main__":
    main()


'''
To do list:
have index.html pull list data from python received data
    figure out how ton give the electron window the python data

list the errors here bro

make a simple UI, dont need to spend time making it pretty for now, just get soemthing working.

'''