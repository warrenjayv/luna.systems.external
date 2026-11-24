#pragma once
#include <winsock2.h>
#include <string>
#include <mutex>

class NetworkClient {
private:
    SOCKET sock;
    bool initialized;
    mutable std::mutex mtx;
public:
    NetworkClient();
    ~NetworkClient();

    bool Initialize();
    bool ConnectToServer(const std::string& ipAddress, int port);
    bool SendData(const unsigned char* data, size_t size);
    bool SendString(const std::string& message);
    bool IsConnected() const;
    void Close();

};

    // Background connection & shared client helpers
    void StartConnectAsync(const std::string& ip, int port);
    void DisconnectServer();
    bool IsServerConnected();
    bool SendToServer(const std::string& message);