#pragma once

#include <string>

class soap {
public:
    std::string get(const std::string& url, const std::string& action, const std::string& header, const std::string& body);
    void write(char* ptr, size_t size);
    size_t read(void* ptr, size_t size);
    int seek(curl_off_t offset, int origin);

private:
    std::string create_xml(const std::string_view& header, const std::string_view& body);
    std::string extract_response(const std::string_view& ret);

    std::string ret;
    std::string payload;
    size_t payload_offset = 0;
};
