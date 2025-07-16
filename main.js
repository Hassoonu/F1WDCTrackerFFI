const { app, BrowserWindow, ipcMain } = require('electron/main')
const path = require('node:path')
const { spawn } = require('child_process')

const pythonProcess = spawn('python', ['main.py']);

pythonProcess.stdout.on('data', (data) => {
  console.log(`PYTHON: ${data}`);
});

pythonProcess.stderr.on('data', (data) => {
  console.error(`PYTHON ERROR: ${data}`);
});

pythonProcess.on('close', (code) => {
  console.log(`Python process exited with code ${code}`);
});

const createWindow = () => {
    const myWindow = new BrowserWindow({
        width: 200,
        height: 500,
        resizable: true,
        // titleBarStyle: 'hidden',
    })

    myWindow.loadFile('index.html')

    // win.webContents.openDevTools();
}

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