#include "tcp_client.h"
#include "base64_utils.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string read_file_as_base64(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return "";
    }

    std::vector<char> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    std::vector<uchar> udata(data.begin(), data.end());
    return Base64Utils::Encode(udata);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: client_example <image_path> [host] [port]" << std::endl;
        return 1;
    }

    std::string image_path = argv[1];
    std::string host = (argc > 2) ? argv[2] : "127.0.0.1";
    int port = (argc > 3) ? std::stoi(argv[3]) : 8080;

    // Read image as base64
    std::string image_base64 = read_file_as_base64(image_path);
    if (image_base64.empty()) {
        std::cerr << "Failed to read image" << std::endl;
        return 1;
    }

    std::cout << "Image size: " << image_base64.size() << " bytes (base64)" << std::endl;

    // Create client
    TcpClient client;
    if (!client.Connect(host, port)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    // Send OCR request
    json params = {
        {"use_doc_orientation_classify", true},
        {"use_doc_unwarping", false},
        {"use_textline_orientation", false}
    };

    std::cout << "Sending OCR request..." << std::endl;
    ResponseMessage res = client.SendOCRRequest(image_base64, params.dump());

    // Output result
    std::cout << "Response:" << std::endl;
    std::cout << "  Code: " << res.code << std::endl;
    std::cout << "  Message: " << res.message << std::endl;

    if (res.code == 0) {
        try {
            json data = json::parse(res.data);
            std::cout << "  Data: " << data.dump(2) << std::endl;
        } catch (const std::exception& e) {
            std::cout << "  Data: " << res.data << std::endl;
        }
    }

    // Send status request
    std::cout << "\nSending status request..." << std::endl;
    RequestMessage status_req;
    status_req.id = Protocol::GenerateRequestId();
    status_req.method = "status";
    status_req.params = "{}";
    status_req.image_data = "";

    ResponseMessage status_res = client.SendRequest(status_req);
    std::cout << "Status:" << std::endl;
    if (status_res.code == 0) {
        try {
            json status = json::parse(status_res.data);
            std::cout << status.dump(2) << std::endl;
        } catch (const std::exception& e) {
            std::cout << status_res.data << std::endl;
        }
    }

    client.Disconnect();
    return 0;
}
