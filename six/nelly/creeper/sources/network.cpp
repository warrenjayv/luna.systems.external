#include "network.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

NetworkClient::NetworkClient() : sock(INVALID_SOCKET), initialized(false) {}

NetworkClient::~NetworkClient() {
    Close();
}

bool NetworkClient::Initialize() {
    WSADATA wsaData;
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

bool NetworkClient::SendData(const unsigned char* data, size_t size) {
    if (sock == INVALID_SOCKET) return false;

    int bytesSent = send(sock, (const char*)data, static_cast<int>(size), 0);
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "[-] Send failed with error: " << WSAGetLastError() << "\n";
        return false;
    }

    return true;
}

void NetworkClient::Close() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    if (initialized) {
        WSACleanup();
        initialized = false;
    }
}