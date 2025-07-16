import ctypes # allows me to import a c file 
import asyncio
import os
import subprocess
import json
from memory_profiler import profile
from flask import Flask, jsonify

DEFAULT_PORT = '443'
host = 'api.jolpi.ca'
# host = 'localhost'
DEFAULT_BUFLEN = 512
EXPECTED_MSG_SIZE = 31000 # 31kB
# sharedMemName = "SharedMemory"
myMessage = "GET /ergast/f1/current/driverstandings/?format=json HTTP/1.1\r\n" \
            "Host: api.jolpi.ca\r\n" \
            "User-Agent: my-openssl-client/1.0\r\n" \
            "Connection: close\r\n" \
            "\r\n"

app = Flask(__name__)

data = []
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
    global data
    # ----- Declare all foreign functions (FFIs) that will be used -----

    initSSL =      clib.init_openssl
    connectToAPI = clib.connectToServer
    contextWrap =  clib.ssl_context_wrap
    sendRequest =  clib.sendDataToServer
    recvData =     clib.recvDataFromServer
    clean =        clib.cleanUp
    freeBuffer =   clib.freeBuffer
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

    
    freeBuffer.argtypes = [ctypes.c_char_p]

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

    # -----------------------------------------------------------------

    # Receive Data:
    dataString = recvData(connection.ssl)
    if(dataString == None):
        check_error(5)
        clean(connectionSocket)

    # -----------------------------------------------------------------
    
    # Convert data from JSON to Python tables
    # print("recv'd: ", dataString)
    try:
        convertedData = json.loads(dataString.decode('utf-8'))
    except Exception as e:
        clean(connection, connectionSocket)
        freeBuffer(dataString)

    # -----------------------------------------------------------------

    # Clean up sockets and close connections.
    cleanStatus = clean(connection, connectionSocket)
    if(cleanStatus != 0):
        freeBuffer(dataString)
        # print("error in clean func", flush=True)
        check_error(6)
    
    # print("CONVERTED STRING IS:", convertedData['MRData']['StandingsTable']['StandingsLists'][0]["DriverStandings"])
    # freeBuffer(dataString)
    data = convertedData

def parse_data(data):
    standings_list = data['MRData']['StandingsTable']['StandingsLists'][0]["DriverStandings"]
    driverId = []
    points = []
    # print(standings_list, flush=True)
    for i in range(len(standings_list)):
        points.append(standings_list[i]['points'])
        driverId.append(standings_list[i]['Driver']['driverId'])
    # print("---------------------------------------------------------", flush=True)
    # print(driverId)
    # print(points)
    # print(data['MRData']['StandingsTable'])
    return driverId, points

@app.route("/data")
def send_data():
    drivers, points = parse_data(data)
    print("Parsed the data\n", flush=True)

    driverData = {
        "drivers": drivers,
        "points": points
    }
    return jsonify(driverData)

# @profile
@app.route("/")
def main():
    lib = ctypes.CDLL('./myCLibrary.dll')
    print("Opened Library\n", flush=True)

    # Part 1: Get data from C function
    getAPIData(host, DEFAULT_PORT, lib)
    print("WE GOT THE DATA!!!\n\n\n\n", flush=True)
    # print("DATA IS:", data)

    
    print("Created deliverable\n", flush=True)

    app.run(port=5000)

if __name__ == '__main__':
    main()