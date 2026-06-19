#include "protocol.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <chrono>
#include <atomic>

using json = nlohmann::json;

std::vector<uint8_t> Protocol::EncodeRequest(const RequestMessage& req) {
    // Build JSON
    json j = {
        {"id", req.id},
        {"method", req.method},
        {"params", json::parse(req.params.empty() ? "{}" : req.params)},
        {"image_data", req.image_data}
    };

    std::string json_str = j.dump();
    uint32_t json_len = static_cast<uint32_t>(json_str.size());

    // Build message
    std::vector<uint8_t> buffer(sizeof(MessageHeader) + json_len);

    // Write header
    MessageHeader header;
    header.magic = MAGIC_REQUEST;
    header.length = json_len;
    std::memcpy(buffer.data(), &header, sizeof(header));

    // Write data
    std::memcpy(buffer.data() + sizeof(header), json_str.c_str(), json_len);

    return buffer;
}

bool Protocol::DecodeRequest(const uint8_t* data, size_t len,
                             RequestMessage& req) {
    if (len < sizeof(MessageHeader)) {
        return false;
    }

    // Read header
    MessageHeader header;
    std::memcpy(&header, data, sizeof(header));

    // Verify magic
    if (header.magic != MAGIC_REQUEST) {
        return false;
    }

    // Verify length
    if (len < sizeof(header) + header.length) {
        return false;
    }

    // Parse JSON
    std::string json_str(
        reinterpret_cast<const char*>(data + sizeof(header)),
        header.length);

    try {
        json j = json::parse(json_str);

        req.id = j.value("id", 0u);
        req.method = j.value("method", "");
        req.params = j.value("params", json::object()).dump();
        req.image_data = j.value("image_data", "");

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<uint8_t> Protocol::EncodeResponse(const ResponseMessage& res) {
    // Build JSON
    json j = {
        {"id", res.id},
        {"code", res.code},
        {"message", res.message},
        {"data", json::parse(res.data.empty() ? "{}" : res.data)}
    };

    std::string json_str = j.dump();
    uint32_t json_len = static_cast<uint32_t>(json_str.size());

    // Build message
    std::vector<uint8_t> buffer(sizeof(MessageHeader) + json_len);

    // Write header
    MessageHeader header;
    header.magic = MAGIC_RESPONSE;
    header.length = json_len;
    std::memcpy(buffer.data(), &header, sizeof(header));

    // Write data
    std::memcpy(buffer.data() + sizeof(header), json_str.c_str(), json_len);

    return buffer;
}

bool Protocol::DecodeResponse(const uint8_t* data, size_t len,
                              ResponseMessage& res) {
    if (len < sizeof(MessageHeader)) {
        return false;
    }

    // Read header
    MessageHeader header;
    std::memcpy(&header, data, sizeof(header));

    // Verify magic
    if (header.magic != MAGIC_RESPONSE) {
        return false;
    }

    // Verify length
    if (len < sizeof(header) + header.length) {
        return false;
    }

    // Parse JSON
    std::string json_str(
        reinterpret_cast<const char*>(data + sizeof(header)),
        header.length);

    try {
        json j = json::parse(json_str);

        res.id = j.value("id", 0u);
        res.code = j.value("code", 0);
        res.message = j.value("message", "");
        res.data = j.value("data", json::object()).dump();

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

uint32_t Protocol::GenerateRequestId() {
    static std::atomic<uint32_t> counter{0};
    return ++counter;
}
