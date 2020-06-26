#include <stdio.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <curl/curl.h>
#include <string.h>
#include "xml.h"

using namespace std;

static size_t curl_read_cb(void* dest, size_t size, size_t nmemb, void* userdata);
static size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata);

class soap {
public:
    string create_xml(const string& url, const string& action, const string& body) {
        string body2 = body;
        xml_writer req;

        if (body2.length() > 2 && body2[0] == '<' && body2[1] == '?') {
            auto st = body2.find('>');

            body2 = body2.substr(st + 1);

            while (!body2.empty() && body2[0] == '\n') {
                body2 = body2.substr(1);
            }
        }

        req.start_document();
        req.start_element("soap:Envelope", { { "soap", "http://schemas.xmlsoap.org/soap/envelope/" }, { "a", "http://schemas.microsoft.com/exchange/2010/Autodiscover" }, { "wsa", "http://www.w3.org/2005/08/addressing" } });

        req.start_element("soap:Header");
        req.element_text("a:RequestedServerVersion", "Exchange2010");
        req.element_text("wsa:Action", action);
        req.element_text("wsa:To", url);
        req.end_element();

        req.start_element("soap:Body");
        req.raw(body2);
        req.end_element();

        req.end_element();
        req.end_document();

        return req.dump();
    }

    string get(const string& url, const string& action, const string& body) {
        auto payload = create_xml(url, action, body);
        string soap_action = "SOAPAction: " + action;

        CURLcode res;
        CURL* curl = curl_easy_init();

        if (!curl)
            throw runtime_error("Failed to initialize cURL.");

        try {
            struct curl_slist *chunk = NULL;
            long error_code;

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_NTLM);
            curl_easy_setopt(curl, CURLOPT_USERPWD, ":");

            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_cb);
            curl_easy_setopt(curl, CURLOPT_READDATA, this);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

            chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            if (res != CURLE_OK)
                throw runtime_error(curl_easy_strerror(res));

            chunk = curl_slist_append(chunk, "Content-Type: text/xml;charset=UTF-8");
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            if (res != CURLE_OK)
                throw runtime_error(curl_easy_strerror(res));

            chunk = curl_slist_append(chunk, soap_action.c_str());
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            if (res != CURLE_OK)
                throw runtime_error(curl_easy_strerror(res));

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

        return ret;
    }

    void write(char* ptr, size_t size) {
        ret += string(ptr, size);
    }

    size_t read(void* ptr, size_t size) {
        if (size > payload_sv.length())
            size = payload_sv.length();

        memcpy(ptr, payload_sv.data(), size);

        payload_sv = payload_sv.substr(size);

        return size;
    }

private:
    string ret;
    string_view payload_sv;
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

static void get_user_settings(const string& url, const string& mailbox) {
    soap s;
    xml_writer req;

    req.start_document();
    req.start_element("a:GetUserSettingsRequestMessage");
    req.start_element("a:Request");

    req.start_element("a:Users");
    req.start_element("a:User");
    req.element_text("a:Mailbox", mailbox);
    req.end_element();
    req.end_element();

    req.start_element("a:RequestedSettings");
    req.element_text("a:Setting", "InternalEwsUrl");
    req.end_element();

    req.end_element();
    req.end_element();
    req.end_document();

    auto ret = s.get(url, "http://schemas.microsoft.com/exchange/2010/Autodiscover/Autodiscover/GetUserSettings", req.dump());

    printf("%s\n", ret.c_str());

    // FIXME - parse response
}

static void main2() {
    get_user_settings("https://autodiscover.boltonft.nhs.uk/autodiscover/autodiscover.svc", "mark.harmstone@boltonft.nhs.uk");
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    try {
        main2();
    } catch (const exception& e) {
        cerr << e.what() << endl;
        return 1;
    }

    return 0;
}
