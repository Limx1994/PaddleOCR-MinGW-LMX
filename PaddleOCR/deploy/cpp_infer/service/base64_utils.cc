#include "base64_utils.h"

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static inline bool is_base64(uchar c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string Base64Utils::Encode(const std::vector<uchar>& data) {
    std::string result;
    int i = 0;
    int j = 0;
    uchar char_array_3[3];
    uchar char_array_4[4];

    for (auto byte : data) {
        char_array_3[i++] = byte;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                              ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                              ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++) {
                result += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                          ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                          ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++) {
            result += base64_chars[char_array_4[j]];
        }

        while (i++ < 3) {
            result += '=';
        }
    }

    return result;
}

std::vector<uchar> Base64Utils::Decode(const std::string& encoded) {
    std::vector<uchar> result;
    int i = 0;
    int j = 0;
    uchar char_array_4[4];
    uchar char_array_3[3];

    for (auto c : encoded) {
        if (c == '=') {
            break;
        }

        if (!is_base64(static_cast<uchar>(c))) {
            continue;
        }

        char_array_4[i++] = static_cast<uchar>(base64_chars.find(c));
        if (i == 4) {
            char_array_3[0] = (char_array_4[0] << 2) +
                              ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0x0f) << 4) +
                              ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x03) << 6) +
                              char_array_4[3];

            for (i = 0; i < 3; i++) {
                result.push_back(char_array_3[i]);
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }

        char_array_3[0] = (char_array_4[0] << 2) +
                          ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0x0f) << 4) +
                          ((char_array_4[2] & 0x3c) >> 2);

        for (j = 0; j < i - 1; j++) {
            result.push_back(char_array_3[j]);
        }
    }

    return result;
}

std::string Base64Utils::EncodeString(const std::string& str) {
    std::vector<uchar> data(str.begin(), str.end());
    return Encode(data);
}

std::string Base64Utils::DecodeToString(const std::string& encoded) {
    std::vector<uchar> data = Decode(encoded);
    return std::string(data.begin(), data.end());
}
