#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Protocol constants
static const uint32_t MAGIC_REQUEST = 0x4F435251;  // "OCRQ"
static const uint32_t MAGIC_RESPONSE = 0x4F435253; // "OCRS"

// Message header
struct MessageHeader {
    uint32_t magic;
    uint32_t length;
};

// Request message
struct RequestMessage {
    uint32_t id;
    std::string method;
    std::string params;
    std::string image_data;
};

// Response message
struct ResponseMessage {
    uint32_t id;
    int32_t code;
    std::string message;
    std::string data;
};

class Protocol {
public:
    // Encode request
    static std::vector<uint8_t> EncodeRequest(const RequestMessage& req);

    // Decode request
    static bool DecodeRequest(const uint8_t* data, size_t len,
                              RequestMessage& req);

    // Encode response
    static std::vector<uint8_t> EncodeResponse(const ResponseMessage& res);

    // Decode response
    static bool DecodeResponse(const uint8_t* data, size_t len,
                               ResponseMessage& res);

    // Generate request ID
    static uint32_t GenerateRequestId();
};
