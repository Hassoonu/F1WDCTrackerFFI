import ctypes # allows me to import a c file 
import asyncio

DEFAULT_PORT = 8000
host = 'http://api.jolpi.ca/ergast/f1/current/driverStandings'
# DEFAULT_PORT = "8000"
DEFAULT_BUFLEN = 512
EXPECTED_MSG_SIZE = 31000 # 31kB
# sharedMemName = "SharedMemory"
myMessage = "GET /ergast/f1/current/driverstandings/"

async def getAPIData(host, port, clib):
    # ----- Declare all foreign functions (FFIs) that will be used -----

    connectToAPI = clib.connectToServer
    sendRequest =  clib.sendDataToServer
    recvData =     clib.recvDataFromServer
    clean =        clib.cleanUp
    # ------------------------------------------------------------
    # ----- Declare all argument and return types for FFIs -----

    connectToAPI.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    connectToAPI.restypes = ctypes.c_int

    sendRequest.argtypes = [ctypes.c_int, ctypes.c_char_p]
    sendRequest.restypes = ctypes.c_int

    recvData.argtypes = []
    recvData.restypes = ctypes.c_int

    clean.argtypes = []
    clean.restypes = ctypes.c_int
    # -----------------------------------------------------------------
    # Get socket to connect to API
    connectionSocket = await connectToAPI(ctypes.c_char_p(host), ctypes.c_char_p(port))

    amountSent = await sendRequest(ctypes.int(connectionSocket), ctypes.c_char_p(myMessage))

    return 0

def main():
    lib = ctypes.CDLL('./myCLibrary.so')
    # Part 1: Get data from C function, do this asynchronously so that you load electronJS while this is communicating with API
    data = getAPIData(host, DEFAULT_PORT, lib)
    # Part 1 Async: Load ElectronJS output.

    # Part 2: If data loaded, give electronJS data, else put down a loading screen/loop in electronJS output

    # Part 3: conditional if we're waiting for data, once data arrives, load it to screen
    return 0

if __name__ == "__main__":
    main()