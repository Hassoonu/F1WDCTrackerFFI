import ctypes # allows me to import a c file 
import json
import argparse
from datetime import datetime
# from memory_profiler import profile
from flask import Flask, jsonify

import logging # python logger, better than just using print statements

DEFAULT_PORT = '443'
host = 'api.jolpi.ca'
# host = 'localhost'
DEFAULT_BUFLEN = 512
EXPECTED_MSG_SIZE = 31000 # 31kB
# sharedMemName = "SharedMemory"
myMessage = "GET /ergast/f1/current/driverstandings/?format=json HTTP/1.1\r\n" \
            "Host: api.jolpi.ca\r\n" \
            "User-Agent: F1WdcTracker/0.1\r\n" \
            "Connection: close\r\n" \
            "\r\n"

LOG_PATH = "/home/hasan/Desktop/Code/f1wdcTrack/src/logs"

app = Flask(__name__)

data = []

logger = logging.getLogger(__name__)


def logger_config(logging_level):
    logging_level = logging_level.upper()

    # print(logging_level)

    conf_level = logging.WARNING
    
    match logging_level:
        case "DEBUG":
            conf_level = logging.DEBUG
        
        case "INFO":
            conf_level = logging.INFO

        case "WARNING":
            conf_level = logging.WARNING

        case "ERROR":
            conf_level = logging.ERROR

        case "CRITICAL":
            conf_level = logging.CRITICAL

        case _:
            print("Invalid logging argument. If you wish to modify the logging, add one of the following at the end of the program:\n--DEBUG\n--INFO\n--WARNING\n--ERROR\n--CRITICAL")
            exit(1)

    now = datetime.now()

    date_time = now.strftime("%m-%d-%Y_%H:%M:%S")

    full_filename = f"f1_wdc_tracker-{date_time}"

    logging.basicConfig(
        filename=f'{LOG_PATH}/{full_filename}.log', 
        encoding='utf-8', 
        level=conf_level,
        format="%(asctime)s - %(levelname)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S"
    )

    logger.info("Finished logger setup.")


class SSLConnection(ctypes.Structure):
    _fields_ = [
        ("ssl", ctypes.c_void_p),
        ("ctx", ctypes.c_void_p)
    ]

# class APIError(Exception):
#     """Base class for API-related errors."""
#     pass

# class ConnectionError(APIError):
#     print("Connection Error")
#     # clean(connectionSocket)
#     exit(1)

# class TimeoutError(APIError):
#     print("Timeout Error")
#     exit(1)

# class InvalidDataError(APIError):
#     print("Invalid Data Error")
#     exit(1)

# class SendError(APIError):
#     print("Send Error")
#     pass

# class RecvError(APIError):
#     print("Recieve Error")
#     pass

# class CleanupError():
#     print("Clean up Error")
#     pass

# class GeneralError(APIError):
#     print("Unknown Error")
#     pass


# def check_code(code):
#     if code == 0:
#         return
#     elif code == 1:
#         raise ConnectionError()
#         return
#     elif code == 2:
#         raise TimeoutError()
#     elif code == 3:
#         raise InvalidDataError()
#     elif code == 4:
#         raise SendError()
#     elif code == 5:
#         raise RecvError()    
#     elif code == 6:
#         raise CleanupError()
#     elif code == 2:
#         raise GeneralError()
    

def getAPIData(host, port, clib):
    global data
    # ----- Declare all foreign functions (FFIs) that will be used -----

    initSSL =       clib.init_openssl
    connectToAPI =  clib.connectToServer
    contextWrap =   clib.ssl_context_wrap
    sendRequest =   clib.sendDataToServer
    recvData =      clib.recvDataFromServer
    clean =         clib.cleanUp
    freeBuffer =    clib.freeBuffer
    remove_header = clib.remove_header

    logger.info("Finished loading clib libraries to python variables.")

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
    recvData.restype = ctypes.c_void_p

    clean.argtypes = [SSLConnection, ctypes.c_size_t]
    clean.restype = ctypes.c_int

    freeBuffer.argtypes = [ctypes.c_void_p]

    remove_header.argtypes = [ctypes.c_void_p]
    remove_header.restype = ctypes.c_void_p

    logger.info("Finished declaring arg and return types for all clib functions.")

    # -----------------------------------------------------------

    initSSL() # init all needed libraries for secure socket connection

    logger.info("Initialized SSL libraries and constants.")

    # Get socket to connect to API:
    connectionSocket = connectToAPI( host.encode('utf-8') , port.encode('utf-8') )
    if(connectionSocket == 0):
        logger.error("Could not connect TCP socket to server.")
        return None
    
    logger.info("Connected basic TCP socket to server")
        
    # Wrap connected socket with TCP and a context wrap:
    connection = contextWrap(connectionSocket, host.encode('utf-8'))
    logger.info("Wrapped the ssl connection and ctx together in one structure.")

    # Send data to server:
    amountSent = sendRequest(connection.ssl, connectionSocket, myMessage.encode('utf-8') )
    if(amountSent <= 0):
        # check_code(4)
        logger.error("Could not send message, aborting.")
        clean(connection, connectionSocket)
        return None

    logger.info("Sent request to server.")

    # Receive Data:
    reply_string = recvData(connection.ssl)
    if(reply_string == None):
        # check_code(5)
        clean(connection, connectionSocket)
        # freeBuffer(reply_string)

    logger.info("Received data.")

    # Convert data from JSON to Python tables
    # print("recv'd: ", reply_string_py_storage.decode('utf-8'))
    # print("\n\n")
    try:
        parsed_data = remove_header(reply_string)
        if(parsed_data):
            parsed_data_py_storage = ctypes.string_at(parsed_data)
            freeBuffer(parsed_data)
            parsed_text = parsed_data_py_storage.decode('utf-8')
        # print("\n\nparsed_text: ", parsed_text)
        convertedData = json.loads(parsed_text)
        # print("\n\n\nconvertedData: ", convertedData)
        logger.info("Converted data from string to JSON.")
    except Exception as e:
        logger.error("Could not convert data to JSON.")
        clean(connection, connectionSocket)
        # freeBuffer(dataString)

    # -----------------------------------------------------------------
    


    # Clean up sockets and close connections.
    cleanStatus = clean(connection, connectionSocket)
    if(cleanStatus != 0):

        # check_code(6)
        clean(connection, connectionSocket)
    
    # print("CONVERTED STRING IS:", convertedData['MRData']['StandingsTable']['StandingsLists'][0]["DriverStandings"])
    data = convertedData


def parse_data(data):
    standings_list = data['MRData']['StandingsTable']['StandingsLists'][0]["DriverStandings"]

    driverId = []
    points = []

    for i in range(len(standings_list)):
        points.append(standings_list[i]['points'])
        driverId.append(standings_list[i]['Driver']['givenName'] + " " + standings_list[i]['Driver']['familyName'])
    
    return driverId, points

@app.route("/data")
def send_data():
    drivers, points = parse_data(data)
    logger.info("Parsed the data\n")

    driverData = {
        "drivers": drivers,
        "points": points
    }
    logger.info("Combined data to one JSON structure.\n")

    return jsonify(driverData)

# @profile
@app.route("/")
def main():
    parser = argparse.ArgumentParser(description="F1 WDC Tracker")
    parser.add_argument("--log-level", type=str, default="WARNING",
                     choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"],
                     help="Set the logging level")

    args = parser.parse_args()
    logger_config(args.log_level)
        

    lib = ctypes.CDLL('src/Backend_Logic/myCLibrary_linux.so')
    logger.info("Opened Library\n")

    ret = getAPIData(host, DEFAULT_PORT, lib)
    if(ret == None):
        # something went wrong :(
        logger.critical("Something went wrong in getAPIData, cannot solve, failing.")
        return
    logger.info("Received data")
    # print("DATA IS:", data)

    app.run(port=5000)

if __name__ == '__main__':
    main()