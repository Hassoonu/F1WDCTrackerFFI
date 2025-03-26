import tkinter as tk
import myModule
import mmap 

# port = 8000
# host = http://api.jolpi.ca/ergast/f1/current/driverStandings
DEFAULT_PORT = "8000"
DEFAULT_BUFLEN = 512
EXPECTED_MSG_SIZE = 31000 # 31kB
sharedMemName = "SharedMemory"


def main():

    # start GUI
    mainWindow = tk.Tk()
    rows = 0
    columns = 1
    # start C data script and put it in dictionary
    myModule.getData()

    # create mmap object & access the data
    mmapObj = mmap(-1, EXPECTED_MSG_SIZE, sharedMemName)

    
    # map it to the same file location as the C file
    data = mmapObj.read(EXPECTED_MSG_SIZE)

    # close the obj    
    mmapObj.close()

    drivers = []
    points = []
    
    for driverData in data["MRData"]["StandingsTable"]["StandingsList"]["DriverStandings"]:
        points.append(driverData["points"])
        drivers.append(driverData["Driver"]["familyName"])
        rows = rows + 1

    # assume the order is sorted in descending order. So points[0] is the leader's points
    leaderPoints = points[0]
    for i in points:
        points[i] = points[i] - leaderPoints
    
    # populate GUI with data
    
    for i in range(rows):
        for j in range(columns):
            e = tk.Entry(mainWindow, width=20, fg='blue', font=('Arial', 16, 'bold'))
            e.grid(row=i, column=j)
            e.insert(tk.END, drivers[i])
            e.insert(tk.END, points[i])
            

    mainWindow.mainloop()
    return 0

if __name__ == "__main__":
    main()