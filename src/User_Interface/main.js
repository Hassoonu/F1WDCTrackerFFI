const { app, BrowserWindow, Menu, screen } = require('electron/main')
const path = require('node:path')
const { spawn } = require('child_process')

const pythonProcess = spawn('python', ['src/Backend_Logic/main.py', '--log-level=INFO']);

pythonProcess.stdout.on('data', (data) => {
  console.log(`PYTHON STDOUT STREAM: ${data}`);
});

pythonProcess.stderr.on('data', (data) => {
  console.error(`PYTHON ERROR STREAM: ${data}`);
});

pythonProcess.on('close', (code) => {
  console.log(`Python process exited with code ${code}`);
});
2
const createWindow = () => {
    Menu.setApplicationMenu(null)

    const windowWidth = 270;
    const windowHeight = 400;

    const primaryDisplay = screen.getPrimaryDisplay();
    const { x: workX, y: workY, width: workWidth, height: workHeight } = primaryDisplay.workArea;

    const x = workX + workWidth - windowWidth - 10;
    const y = workY + workHeight - windowHeight - 10;

    const myWindow = new BrowserWindow({
        width: windowWidth,
        height: windowHeight,
        x: x,
        y: y,
        resizable: true,
        webPreferences: {
          nodeIntegration: true,
          contextIsolation: false,
        }
    });

    myWindow.loadFile('src/User_Interface/index.html')

    // win.webContents.openDevTools();
}


// app.commandLine.appendSwitch('disable-features', 'WaylandFractionalScaleV1,WaylandColorManagement');

app.whenReady().then(() => {
    createWindow()

    app.on('activate', () => {
        if(BrowserWindow.getAllWindows().length == 0){
            createWindow()
        }
    })
})

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin'){
        app.quit()
    }
})