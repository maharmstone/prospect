#include <curl/curl.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include "xml.h"
#include "soap.h"

using namespace std;

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
