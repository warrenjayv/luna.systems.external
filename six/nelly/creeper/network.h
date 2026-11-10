#pragma once
#include <winsock2.h>
#include <string>

class NetworkClient {
private:
    SOCKET sock;
    bool initialized;

public:
    NetworkClient();
    ~NetworkClient();

    bool Initialize();
    bool ConnectToServer(const std::string& ipAddress, int port);
    bool SendData(const unsigned char* data, size_t size);
    void Close();
};