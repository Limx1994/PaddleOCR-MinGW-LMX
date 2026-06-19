#pragma once

#include "protocol.h"
#include <functional>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

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

using RequestHandler = std::function<ResponseMessage(const RequestMessage&)>;

class TcpServer {
public:
    TcpServer();
    ~TcpServer();

    // Start server
    bool Start(const std::string& host, int port, int thread_num = 4);

    // Stop server
    void Stop();

    // Set request handler
    void SetRequestHandler(RequestHandler handler);

    // Is running
    bool IsRunning() const { return running_; }

    // Get connection count
    int GetConnectionCount() const { return connection_count_; }

private:
    // Accept loop
    void AcceptLoop();

    // Handle client
    void HandleClient(SOCKET client_fd);

    // Receive full message
    bool RecvFull(SOCKET fd, uint8_t* buffer, size_t len);

    // Send full message
    bool SendFull(SOCKET fd, const uint8_t* data, size_t len);

private:
    SOCKET server_fd_ = INVALID_SOCKET;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    std::vector<std::thread> worker_threads_;
    std::mutex mutex_;
    RequestHandler handler_;
    std::atomic<int> connection_count_{0};

#ifdef _WIN32
    WSADATA wsa_data_;
#endif
};
