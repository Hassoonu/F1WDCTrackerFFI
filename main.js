const { app, BrowserWindow, ipcMain } = require('electron/main')
const path = require('node:path')

const createWindow = () => {
    const myWindow = new BrowserWindow({
        width: 200,
        height: 500,
        resizable: true,
        titleBarStyle: 'hidden',
        webPreferences: {
            preload: path.join(__dirname, 'preload.js')
        }
    })

    myWindow.loadFile('myApp.html')

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