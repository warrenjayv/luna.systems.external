#include "network.h"
#include <iostream>
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

NetworkClient g_netClient;
std::thread g_netConnectThread;
std::atomic<bool> g_netConnecting{false};

void ConnectWorker(std::string ip, int port) {
    g_netConnecting = true;
    g_netClient.Close();

    if (!g_netClient.Initialize()) {
        g_netConnecting = false;
        return;
    }

    if (g_netClient.ConnectToServer(ip, port)) {
        std::cout << "\n[+] Connected to host server " << ip << ":" << port << "\n" << std::flush;
    } else {
        g_netClient.Close();
    }
    g_netConnecting = false;
} //\ ConnectWorker

NetworkClient::NetworkClient() : sock(INVALID_SOCKET), initialized(false) {}

NetworkClient::~NetworkClient() {
    Close();
}

bool NetworkClient::Initialize() {
    WSADATA wsaData;
    std::lock_guard<std::mutex> lock(mtx);
    if (initialized && sock != INVALID_SOCKET) return true;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != 0) {
        std::cerr << "[-] WSAStartup failed with error: " << result << "\n";
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "[-] Socket creation failed with error: " << WSAGetLastError() << "\n";
        WSACleanup();
        return false;
    }

    initialized = true;
    return true;
}

bool NetworkClient::ConnectToServer(const std::string& ipAddress, int port) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!initialized) return false;

    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr(ipAddress.c_str());
    clientService.sin_port = htons(port);

    if (connect(sock, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        std::cerr << "[-] Connect failed with error: " << WSAGetLastError() << "\n";
        return false;
    }

    return true;
}

// In network.cpp
bool NetworkClient::SendData(const unsigned char* data, size_t size) {
    std::lock_guard<std::mutex> lock(mtx);
    if (sock == INVALID_SOCKET || !data || size == 0) return false;

    size_t totalSent = 0;
    while (totalSent < size) {
        int bytesSent = send(
            sock,
            reinterpret_cast<const char*>(data + totalSent),
            static_cast<int>(size - totalSent),
            0
        );
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "[-] Send failed with error: " << WSAGetLastError() << "\n";
            return false;
        }
        if (bytesSent == 0) {
            return false;
        }
        totalSent += static_cast<size_t>(bytesSent);
    }

    return true;
}//\ SendData

bool NetworkClient::SendString(const std::string& message) {
    return SendData(reinterpret_cast<const unsigned char*>(message.data()), message.size());
}//\ SendString

bool NetworkClient::IsConnected() const {
    std::lock_guard<std::mutex> lock(mtx);
    return initialized && sock != INVALID_SOCKET;
} //\ IsConnected

void NetworkClient::Close() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    if (initialized) {
        WSACleanup();
        initialized = false;
    }
} //\ Close

void StartConnectAsync(const std::string& ip, int port) {
    if (g_netConnectThread.joinable()) {
        g_netConnectThread.join();
    }
    g_netConnectThread = std::thread(ConnectWorker, ip, port);
} //\ StartConnectAsync

void DisconnectServer() {
    if (g_netConnectThread.joinable()) {
        g_netConnectThread.join();
    }
    g_netClient.Close();
} //\ DisconnectServer

bool IsServerConnected() {
    return g_netClient.IsConnected();
} //\ IsServerConnected

bool SendToServer(const std::string& message) {
    return g_netClient.SendString(message);
} //\ SendToServer