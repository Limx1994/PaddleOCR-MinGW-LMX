#include "tcp_client.h"
#include <iostream>
#include <cstring>

TcpClient::TcpClient() {
#ifdef _WIN32
    WSAStartup(MAKEWORD(2, 2), &wsa_data_);
#endif
}

TcpClient::~TcpClient() {
    Disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool TcpClient::Connect(const std::string& host, int port) {
    // Create socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // Connect to server
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        closesocket(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
        return false;
    }

    connected_ = true;
    std::cout << "Connected to " << host << ":" << port << std::endl;
    return true;
}

void TcpClient::Disconnect() {
    if (connected_ && socket_fd_ != INVALID_SOCKET) {
        closesocket(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
        connected_ = false;
        std::cout << "Disconnected" << std::endl;
    }
}

ResponseMessage TcpClient::SendOCRRequest(const std::string& image_data,
                                           const std::string& params) {
    RequestMessage req;
    req.id = Protocol::GenerateRequestId();
    req.method = "ocr";
    req.params = params;
    req.image_data = image_data;

    return SendRequest(req);
}

ResponseMessage TcpClient::SendRequest(const RequestMessage& req) {
    ResponseMessage res;
    res.id = req.id;
    res.code = -1;
    res.message = "Not connected";
    res.data = "{}";

    if (!connected_) {
        return res;
    }

    // Encode request
    auto request_data = Protocol::EncodeRequest(req);

    // Send request
    if (!SendFull(request_data.data(), request_data.size())) {
        res.message = "Failed to send request";
        return res;
    }

    // Receive response header
    MessageHeader header;
    if (!RecvFull(reinterpret_cast<uint8_t*>(&header), sizeof(header))) {
        res.message = "Failed to receive response header";
        return res;
    }

    // Verify magic
    if (header.magic != MAGIC_RESPONSE) {
        res.message = "Invalid response magic";
        return res;
    }

    // Receive response body
    std::vector<uint8_t> body(header.length);
    if (!RecvFull(body.data(), header.length)) {
        res.message = "Failed to receive response body";
        return res;
    }

    // Decode response
    std::vector<uint8_t> full_message(sizeof(header) + header.length);
    std::memcpy(full_message.data(), &header, sizeof(header));
    std::memcpy(full_message.data() + sizeof(header),
                body.data(), header.length);

    if (!Protocol::DecodeResponse(full_message.data(),
                                  full_message.size(), res)) {
        res.message = "Failed to decode response";
        return res;
    }

    return res;
}

bool TcpClient::RecvFull(uint8_t* buffer, size_t len) {
    size_t received = 0;
    while (received < len) {
        int ret = recv(socket_fd_, reinterpret_cast<char*>(buffer + received),
                       static_cast<int>(len - received), 0);
        if (ret <= 0) {
            return false;
        }
        received += ret;
    }
    return true;
}

bool TcpClient::SendFull(const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int ret = send(socket_fd_, reinterpret_cast<const char*>(data + sent),
                       static_cast<int>(len - sent), 0);
        if (ret == SOCKET_ERROR) {
            return false;
        }
        sent += ret;
    }
    return true;
}
