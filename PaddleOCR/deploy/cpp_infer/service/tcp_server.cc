#include "tcp_server.h"
#include <iostream>
#include <cstring>

TcpServer::TcpServer() {
#ifdef _WIN32
    WSAStartup(MAKEWORD(2, 2), &wsa_data_);
#endif
}

TcpServer::~TcpServer() {
    Stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool TcpServer::Start(const std::string& host, int port, int thread_num) {
    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // Set address reuse
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Bind address
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind socket" << std::endl;
        closesocket(server_fd_);
        return false;
    }

    // Start listening
    if (listen(server_fd_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Failed to listen" << std::endl;
        closesocket(server_fd_);
        return false;
    }

    running_ = true;

    // Start accept thread
    accept_thread_ = std::thread(&TcpServer::AcceptLoop, this);

    std::cout << "TCP Server started on " << host << ":" << port << std::endl;

    return true;
}

void TcpServer::Stop() {
    running_ = false;

    if (server_fd_ != INVALID_SOCKET) {
        closesocket(server_fd_);
        server_fd_ = INVALID_SOCKET;
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    worker_threads_.clear();
}

void TcpServer::SetRequestHandler(RequestHandler handler) {
    handler_ = handler;
}

void TcpServer::AcceptLoop() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        SOCKET client_fd = accept(server_fd_,
                                  reinterpret_cast<struct sockaddr*>(&client_addr),
                                  &client_len);

        if (client_fd == INVALID_SOCKET) {
            if (running_) {
                std::cerr << "Failed to accept connection" << std::endl;
            }
            continue;
        }

        connection_count_++;
        std::cout << "Client connected. Total connections: "
                  << connection_count_ << std::endl;

        // Start new thread to handle client
        std::lock_guard<std::mutex> lock(mutex_);
        worker_threads_.emplace_back(
            &TcpServer::HandleClient, this, client_fd);
    }
}

void TcpServer::HandleClient(SOCKET client_fd) {
    while (running_) {
        // Receive message header
        MessageHeader header;
        if (!RecvFull(client_fd, reinterpret_cast<uint8_t*>(&header),
                      sizeof(header))) {
            break;
        }

        // Verify magic
        if (header.magic != MAGIC_REQUEST) {
            std::cerr << "Invalid magic: 0x" << std::hex << header.magic << std::endl;
            break;
        }

        // Receive message body
        std::vector<uint8_t> body(header.length);
        if (!RecvFull(client_fd, body.data(), header.length)) {
            break;
        }

        // Decode request
        RequestMessage request;
        std::vector<uint8_t> full_message(sizeof(header) + header.length);
        std::memcpy(full_message.data(), &header, sizeof(header));
        std::memcpy(full_message.data() + sizeof(header),
                    body.data(), header.length);

        if (!Protocol::DecodeRequest(full_message.data(),
                                     full_message.size(), request)) {
            std::cerr << "Failed to decode request" << std::endl;
            break;
        }

        // Process request
        ResponseMessage response;
        if (handler_) {
            response = handler_(request);
        } else {
            response.id = request.id;
            response.code = -1;
            response.message = "No handler registered";
            response.data = "{}";
        }

        // Encode response
        auto response_data = Protocol::EncodeResponse(response);

        // Send response
        if (!SendFull(client_fd, response_data.data(),
                      response_data.size())) {
            break;
        }
    }

    closesocket(client_fd);
    connection_count_--;
    std::cout << "Client disconnected. Total connections: "
              << connection_count_ << std::endl;
}

bool TcpServer::RecvFull(SOCKET fd, uint8_t* buffer, size_t len) {
    size_t received = 0;
    while (received < len) {
        int ret = recv(fd, reinterpret_cast<char*>(buffer + received),
                       static_cast<int>(len - received), 0);
        if (ret <= 0) {
            return false;
        }
        received += ret;
    }
    return true;
}

bool TcpServer::SendFull(SOCKET fd, const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int ret = send(fd, reinterpret_cast<const char*>(data + sent),
                       static_cast<int>(len - sent), 0);
        if (ret == SOCKET_ERROR) {
            return false;
        }
        sent += ret;
    }
    return true;
}
