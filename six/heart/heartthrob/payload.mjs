// payload.js
import { app, BrowserWindow } from 'electron';
import { fileURLToPath } from 'url';
import path from 'path';

// Fixes missing __dirname variable in ES Module environments
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

function createWindow() {
  const win = new BrowserWindow({
    width: 1200,
    height: 800,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false
    }
  });

  // Match this with whatever port your 'npm run dev' terminal displays
  win.loadURL('http://localhost:8080'); 
}

app.whenReady().then(() => {
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});