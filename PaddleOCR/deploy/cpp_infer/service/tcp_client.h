#pragma once

#include "protocol.h"
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

class TcpClient {
public:
    TcpClient();
    ~TcpClient();

    // Connect to server
    bool Connect(const std::string& host, int port);

    // Disconnect
    void Disconnect();

    // Send OCR request
    ResponseMessage SendOCRRequest(const std::string& image_data,
                                   const std::string& params = "{}");

    // Send arbitrary request
    ResponseMessage SendRequest(const RequestMessage& req);

    // Is connected
    bool IsConnected() const { return connected_; }

private:
    // Receive full message
    bool RecvFull(uint8_t* buffer, size_t len);

    // Send full message
    bool SendFull(const uint8_t* data, size_t len);

private:
    SOCKET socket_fd_ = INVALID_SOCKET;
    bool connected_ = false;

#ifdef _WIN32
    WSADATA wsa_data_;
#endif
};
