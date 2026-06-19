#pragma once

#include <string>
#include <vector>

using uchar = unsigned char;

class Base64Utils {
public:
    // Encode
    static std::string Encode(const std::vector<uchar>& data);

    // Decode
    static std::vector<uchar> Decode(const std::string& encoded);

    // Encode string
    static std::string EncodeString(const std::string& str);

    // Decode to string
    static std::string DecodeToString(const std::string& encoded);
};
