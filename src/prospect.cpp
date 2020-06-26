#include <stdio.h>
#include <string>
#include "xml.h"

using namespace std;

static void get_user_settings(const string& url, const string& mailbox) {
    xml_writer req;

    req.start_document();
    req.start_element("soap:Envelope", { { "soap", "http://schemas.xmlsoap.org/soap/envelope/" }, { "a", "http://schemas.microsoft.com/exchange/2010/Autodiscover" }, { "wsa", "http://www.w3.org/2005/08/addressing" } });

    req.start_element("soap:Header");
    req.element_text("a:RequestedServerVersion", "Exchange2010");
    req.element_text("wsa:Action", "http://schemas.microsoft.com/exchange/2010/Autodiscover/Autodiscover/GetUserSettings");
    req.element_text("wsa:To", url);
    req.end_element();

    req.start_element("soap:Body");
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
    req.end_element();

    req.end_element();
    req.end_document();

    printf("%s\n", req.dump().c_str());

    // FIXME - send via libcurl
    // FIXME - parse response
}

int main() {
    get_user_settings("https://autodiscover.boltonft.nhs.uk/autodiscover/autodiscover.svc", "mark.harmstone@boltonft.nhs.uk");

    return 0;
}
