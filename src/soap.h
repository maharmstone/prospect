#pragma once

#include <string>
#include <functional>

using soap_stream_func = std::function<void(const std::string_view&)>;

class soap {
public:
    std::string get(const std::string& url, const std::string& action, const std::string& header, const std::string& body);
    void get_stream(const std::string& url, const std::string& action, const std::string& header, const std::string& body,
                    const soap_stream_func& func);
    void write(char* ptr, size_t size);
    size_t read(void* ptr, size_t size);
    int seek(curl_off_t offset, int origin);
    void write_stream(char* ptr, size_t size, size_t nmemb);

private:
    std::string create_xml(const std::string_view& header, const std::string_view& body);

    std::string ret;
    std::string payload;
    size_t payload_offset = 0;
    soap_stream_func stream_func;
};
