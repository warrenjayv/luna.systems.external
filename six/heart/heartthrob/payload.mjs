// payload.mjs
import { app, BrowserWindow, ipcMain } from "electron";
import net from "node:net";
import { fileURLToPath } from "url";
import path from "path";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

let tcpServer = null;

function startTcpServer(host = "127.0.0.1", port = 9000) {
  if (tcpServer) {
    tcpServer.close();
    tcpServer = null;
  }

  return new Promise((resolve) => {
    const server = net.createServer((socket) => {
      socket.setEncoding("utf8");

      socket.on("data", (chunk) => {
        for (const win of BrowserWindow.getAllWindows()) {
          if (!win.isDestroyed()) {
            win.webContents.send("net:tcp-data", chunk);
          }
        }
      });

      socket.on("error", (err) => {
        console.error("[tcp socket error]", err.message);
      });
    });

    server.on("error", (err) => {
      resolve({ ok: false, error: err.message });
    });

    server.listen(port, host, () => {
      tcpServer = server;
      console.log(`[+] TCP server listening on ${host}:${port}`);
      resolve({ ok: true });
    });
  });
}

function stopTcpServer() {
  if (tcpServer) {
    tcpServer.close();
    tcpServer = null;
  }
}

ipcMain.handle("net:start-tcp", async (_event, host, port) => {
  return startTcpServer(host, port);
});

ipcMain.handle("net:stop-tcp", async () => {
  stopTcpServer();
});

function createWindow() {
  const win = new BrowserWindow({
    width: 1200,
    height: 800,
    webPreferences: {
      nodeIntegration: true,
      nodeIntegrationInWorker: true,
      contextIsolation: false,
    },
  });

  // Match this with whatever port your 'npm run dev' terminal displays
  const targetUrl = "http://localhost:8080";
  waitForServer(targetUrl)
    .then(() => {
      if (!win.isDestroyed()) {
        win.loadURL(targetUrl);
      }
    })
    .catch((err) => {
      console.error("[electron]", err.message);
    });
}
app.whenReady().then(() => {
  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("window-all-closed", () => {
  stopTcpServer();
  if (process.platform !== "darwin") app.quit();
});

import http from 'node:http';

function waitForServer(url, timeoutMs = 15000) {
  const start = Date.now();
  return new Promise((resolve, reject) => {
    const attempt = () => {
      const req = http.get(url, (res) => {
        res.resume();
        resolve();
      });
      req.on('error', () => {
        if (Date.now() - start >= timeoutMs) {
          reject(new Error(`Timed out waiting for ${url}`));
        } else {
          setTimeout(attempt, 200);
        }
      });
    };
    attempt();
  });
}
