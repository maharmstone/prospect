#include <stdexcept>
#include <string.h>
#include <curl/curl.h>
#include <stdint.h>
#include "soap.h"
#include "xml.h"
#include "misc.h"
#include <iostream>

using namespace std;

// #define DEBUG_CURL

static const unordered_map<string, string> namespaces = {
    { "soap", "http://schemas.xmlsoap.org/soap/envelope/" },
    { "a", "http://schemas.microsoft.com/exchange/2010/Autodiscover" },
    { "wsa", "http://www.w3.org/2005/08/addressing" },
    { "m", "http://schemas.microsoft.com/exchange/services/2006/messages" },
    { "t", "http://schemas.microsoft.com/exchange/services/2006/types" }
};

static string extract_response(string_view ret);

static size_t curl_read_cb(void* dest, size_t size, size_t nmemb, void* userdata) {
    auto& s = *(soap*)userdata;

    return s.read(dest, size * nmemb);
}

static size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto& s = *(soap*)userdata;

    s.write(ptr, size * nmemb);

    return size * nmemb;
}

string soap::create_xml(string_view header, string_view body) {
    string body2{body};
    xml_writer req;

    if (body2.length() > 2 && body2[0] == '<' && body2[1] == '?') {
        auto st = body2.find('>');

        body2 = body2.substr(st + 1);

        while (!body2.empty() && body2[0] == '\n') {
            body2 = body2.substr(1);
        }
    }

    req.start_document();
    req.start_element("soap:Envelope", namespaces);

    req.start_element("soap:Header");
    req.raw(header);
    req.end_element();

    req.start_element("soap:Body");
    req.raw(body2);
    req.end_element();

    req.end_element();

    return req.dump();
}

static int curl_seek_cb(void* userdata, curl_off_t offset, int origin) {
    auto& s = *(soap*)userdata;

    return s.seek(offset, origin);
}

int soap::seek(curl_off_t offset, int origin) {
    switch (origin) {
        case SEEK_SET:
            if (offset < 0 || (size_t)offset > payload.length())
                return CURL_SEEKFUNC_FAIL;

            payload_offset = offset;

            return CURL_SEEKFUNC_OK;

        case SEEK_CUR:
            if ((int64_t)payload_offset + offset < 0 || (size_t)(payload_offset + offset) > payload.length())
                return CURL_SEEKFUNC_FAIL;

            payload_offset += offset;

            return CURL_SEEKFUNC_OK;

        case SEEK_END:
            if ((int64_t)payload.length() + offset < 0 || offset > 0)
                return CURL_SEEKFUNC_FAIL;

            payload_offset = payload.length() + offset;

            return CURL_SEEKFUNC_OK;

        default:
            return CURL_SEEKFUNC_FAIL;
    }
}

#ifdef DEBUG_CURL
static void dump(const char* text, FILE* stream, unsigned char* ptr, size_t size) {
    size_t i;
    size_t c;

    const unsigned int width = 0x10;

    fprintf(stream, "%s, %10.10lu bytes (0x%8.8lx)\n",
            text, (unsigned long)size, (unsigned long)size);

    for(i = 0; i<size; i += width) {
        fprintf(stream, "%4.4lx: ", (unsigned long)i);

        for(c = 0; c < width; c++) {
            if(i + c < size)
                fprintf(stream, "%02x ", ptr[i + c]);
            else
                fputs("   ", stream);
        }

        for(c = 0; (c < width) && (i + c < size); c++) {
                fprintf(stream, "%c",
                        (ptr[i + c] >= 0x20) && (ptr[i + c]<0x80)?ptr[i + c]:'.');
        }

        fputc('\n', stream);
    }

    fflush(stream);
}

static int trace(CURL*, curl_infotype type, char* data, size_t size) {
    const char *text;

    switch (type) {
        case CURLINFO_TEXT:
            fprintf(stderr, "== Info: %s", data);
            return 0;

        case CURLINFO_HEADER_OUT:
            text = "=> Send header";
            break;
        case CURLINFO_DATA_OUT:
            text = "=> Send data";
            break;
        case CURLINFO_SSL_DATA_OUT:
            text = "=> Send SSL data";
            break;
        case CURLINFO_HEADER_IN:
            text = "<= Recv header";
            break;
        case CURLINFO_DATA_IN:
            text = "<= Recv data";
            break;
        case CURLINFO_SSL_DATA_IN:
            text = "<= Recv SSL data";
            break;

        default:
            return 0;
    }

    dump(text, stderr, (unsigned char*)data, size);

    return 0;
}
#endif

string soap::get(const string& url, const string& action, const string& header, const string& body) {
    string soap_action = "SOAPAction: " + action;

    CURLcode res;
    CURL* curl = curl_easy_init();

    if (!curl)
        throw formatted_error("Failed to initialize cURL.");

    payload = create_xml(header, body);

    try {
        struct curl_slist *chunk = NULL;
        long error_code;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

#ifdef DEBUG_CURL
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, trace);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_NEGOTIATE);
        curl_easy_setopt(curl, CURLOPT_USERPWD, ":");

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_cb);
        curl_easy_setopt(curl, CURLOPT_READDATA, this);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl, CURLOPT_SEEKFUNCTION, curl_seek_cb);
        curl_easy_setopt(curl, CURLOPT_SEEKDATA, this);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, payload.length());

        chunk = curl_slist_append(chunk, "Content-Type: text/xml;charset=UTF-8");
        res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        if (res != CURLE_OK)
            throw formatted_error("curl_easy_setopt failed: {}", curl_easy_strerror(res));

        if (!action.empty()) {
            chunk = curl_slist_append(chunk, soap_action.c_str());
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            if (res != CURLE_OK)
                throw formatted_error("curl_easy_setopt failed: {}", curl_easy_strerror(res));
        }

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            throw formatted_error("curl_easy_perform failed: {}", curl_easy_strerror(res));

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &error_code);

        if (error_code >= 400)
            throw formatted_error("HTTP error {}", error_code);
    } catch (...) {
        curl_easy_cleanup(curl);
        throw;
    }

    curl_easy_cleanup(curl);

    return extract_response(ret);
}

void soap::write_stream(char* ptr, size_t size, size_t nmemb) {
    auto sv = string_view(ptr, size * nmemb);

    while (!sv.empty() && sv[0] != '<') {
        sv.remove_prefix(1);
    }

    while (!sv.empty() && sv.back() != '>') {
        sv.remove_suffix(1);
    }

    if (!sv.empty())
        stream_func(extract_response(sv));
}

static size_t curl_write_stream_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto& s = *(soap*)userdata;

    s.write_stream(ptr, size, nmemb);

    return size * nmemb;
}

void soap::get_stream(const string& url, const string& action, const string& header, const string& body,
                      const soap_stream_func& func) {
    string soap_action = "SOAPAction: " + action;

    CURLcode res;
    CURL* curl = curl_easy_init();

    if (!curl)
        throw formatted_error("Failed to initialize cURL.");

    payload = create_xml(header, body);

    try {
        struct curl_slist *chunk = NULL;
        long error_code;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_NEGOTIATE);
        curl_easy_setopt(curl, CURLOPT_USERPWD, ":");

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_cb);
        curl_easy_setopt(curl, CURLOPT_READDATA, this);

        stream_func = func;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_stream_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

        curl_easy_setopt(curl, CURLOPT_SEEKFUNCTION, curl_seek_cb);
        curl_easy_setopt(curl, CURLOPT_SEEKDATA, this);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, payload.length());

        chunk = curl_slist_append(chunk, "Content-Type: text/xml;charset=UTF-8");
        res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        if (res != CURLE_OK)
            throw formatted_error("curl_easy_setopt failed: {}", curl_easy_strerror(res));

        if (!action.empty()) {
            chunk = curl_slist_append(chunk, soap_action.c_str());
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            if (res != CURLE_OK)
                throw formatted_error("curl_easy_setopt failed: {}", curl_easy_strerror(res));
        }

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            throw formatted_error("curl_easy_perform failed: {}", curl_easy_strerror(res));

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &error_code);

        if (error_code >= 400)
            throw formatted_error("HTTP error {}", error_code);
    } catch (...) {
        curl_easy_cleanup(curl);
        throw;
    }

    curl_easy_cleanup(curl);
}

void soap::write(char* ptr, size_t size) {
    ret += string(ptr, size);
}

size_t soap::read(void* ptr, size_t size) {
    if (size > payload.length() - payload_offset)
        size = payload.length() - payload_offset;

    memcpy(ptr, payload.data() + payload_offset, size);

    payload_offset += size;

    return size;
}

static string extract_response(string_view ret) {
    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw formatted_error("Invalid XML.");

    try {
        xmlNodePtr root, n;

        root = xmlDocGetRootElement(doc);

        if (!root)
            throw formatted_error("Root element not found.");

        if (!root->ns || strcmp((char*)root->ns->href, "http://schemas.xmlsoap.org/soap/envelope/") || strcmp((char*)root->name, "Envelope"))
            throw formatted_error("Root element was not soap:Envelope.");

        n = root->children;

        while (n) {
            if (n->type == XML_ELEMENT_NODE && n->ns && !strcmp((char*)n->ns->href, "http://schemas.xmlsoap.org/soap/envelope/") && !strcmp((char*)n->name, "Body")) {
                xmlBufferPtr buf = xmlBufferCreate();

                if (!buf)
                    throw formatted_error("xmlBufferCreate failed.");

                try {
                    xmlNodePtr copy = xmlCopyNode(n, 1);
                    if (!copy)
                        throw formatted_error("xmlCopyNode failed.");

                    if (xmlNodeDump(buf, doc, copy, 0, 0) == 0) {
                        xmlFreeNode(copy);
                        throw formatted_error("xmlNodeDump failed.");
                    }

                    xmlFreeNode(copy);
                } catch (...) {
                    xmlBufferFree(buf);
                    throw;
                }

                auto s = string((char*)buf->content, buf->use);

                xmlBufferFree(buf);

                return s;
            }

            n = n->next;
        }

        throw formatted_error("soap:Body not found in response.");
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);
}
