#include <stdexcept>
#include <string.h>
#include <curl/curl.h>
#include "soap.h"
#include "xml.h"

using namespace std;

static const unordered_map<string, string> namespaces = {
    { "soap", "http://schemas.xmlsoap.org/soap/envelope/" },
    { "a", "http://schemas.microsoft.com/exchange/2010/Autodiscover" },
    { "wsa", "http://www.w3.org/2005/08/addressing" },
    { "m", "http://schemas.microsoft.com/exchange/services/2006/messages" },
    { "t", "http://schemas.microsoft.com/exchange/services/2006/types" }
};

static size_t curl_read_cb(void* dest, size_t size, size_t nmemb, void* userdata) {
    auto& s = *(soap*)userdata;

    return s.read(dest, size * nmemb);
}

static size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto& s = *(soap*)userdata;

    s.write(ptr, size * nmemb);

    return size * nmemb;
}

string soap::create_xml(const string_view& header, const string_view& body) {
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
    req.end_document();

    return req.dump();
}

string soap::get(const string& url, const string& action, const string& header, const string& body) {
    auto payload = create_xml(header, body);
    string soap_action = "SOAPAction: " + action;

    CURLcode res;
    CURL* curl = curl_easy_init();

    if (!curl)
        throw runtime_error("Failed to initialize cURL.");

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
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.length());

        chunk = curl_slist_append(chunk, "Content-Type: text/xml;charset=UTF-8");
        res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        if (res != CURLE_OK)
            throw runtime_error(curl_easy_strerror(res));

        if (!action.empty()) {
            chunk = curl_slist_append(chunk, soap_action.c_str());
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            if (res != CURLE_OK)
                throw runtime_error(curl_easy_strerror(res));
        }

        payload_sv = payload;

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            throw runtime_error(curl_easy_strerror(res));

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &error_code);

        if (error_code >= 400)
            throw runtime_error("HTTP error " + to_string(error_code));
    } catch (...) {
        curl_easy_cleanup(curl);
        throw;
    }

    curl_easy_cleanup(curl);

    return extract_response(ret);
}

void soap::write(char* ptr, size_t size) {
    ret += string(ptr, size);
}

size_t soap::read(void* ptr, size_t size) {
    if (size > payload_sv.length())
        size = payload_sv.length();

    memcpy(ptr, payload_sv.data(), size);

    payload_sv = payload_sv.substr(size);

    return size;
}

string soap::extract_response(const string_view& ret) {
    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Invalid XML.");

    try {
        xmlNodePtr root, n;

        root = xmlDocGetRootElement(doc);

        if (!root)
            throw runtime_error("Root element not found.");

        if (!root->ns || strcmp((char*)root->ns->href, "http://schemas.xmlsoap.org/soap/envelope/") || strcmp((char*)root->name, "Envelope"))
            throw runtime_error("Root element was not soap:Envelope.");

        n = root->children;

        while (n) {
            if (n->type == XML_ELEMENT_NODE && n->ns && !strcmp((char*)n->ns->href, "http://schemas.xmlsoap.org/soap/envelope/") && !strcmp((char*)n->name, "Body")) {
                xmlBufferPtr buf = xmlBufferCreate();

                if (!buf)
                    throw runtime_error("xmlBufferCreate failed.");

                try {
                    xmlNodePtr copy = xmlCopyNode(n, 1);
                    if (!copy)
                        throw runtime_error("xmlCopyNode failed.");

                    if (xmlNodeDump(buf, doc, copy, 0, 0) == 0) {
                        xmlFreeNode(copy);
                        throw runtime_error("xmlNodeDump failed.");
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

        throw runtime_error("soap:Body not found in response.");
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);
}
