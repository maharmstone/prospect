#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <functional>
#include <iostream>
#include <map>
#include "xml.h"
#include "soap.h"

using namespace std;

static const string autodiscover_ns = "http://schemas.microsoft.com/exchange/2010/Autodiscover";

static xmlNodePtr find_tag(xmlNodePtr root, const string& ns, const string& name) {
    xmlNodePtr n = root->children;

    while (n) {
        if (n->type == XML_ELEMENT_NODE && n->ns && !strcmp((char*)n->ns->href, ns.c_str()) && !strcmp((char*)n->name, name.c_str()))
            return n;

        n = n->next;
    }

    throw runtime_error("Could not find " + name + " tag");
}

static string get_tag_content(xmlNodePtr n) {
    auto xc = xmlNodeGetContent(n);

    if (!xc)
        return "";

    string ret{(char*)xc};

    xmlFree(xc);

    return ret;
}

static void find_tags(xmlNodePtr n, const string& ns, const string& tag, const function<void(xmlNodePtr)>& func) {
    auto c = n->children;

    while (c) {
        if (c->type == XML_ELEMENT_NODE && c->ns && !strcmp((char*)c->ns->href, ns.c_str()) && !strcmp((char*)c->name, tag.c_str()))
            func(c);

        c = c->next;
    }
}

static void parse_get_user_settings_response(xmlNodePtr n, map<string, string>& settings) {
    auto response = find_tag(n, autodiscover_ns, "Response");

    auto error_code = get_tag_content(find_tag(response, autodiscover_ns, "ErrorCode"));

    if (error_code != "NoError") {
        auto error_msg = get_tag_content(find_tag(response, autodiscover_ns, "ErrorMessage"));

        throw runtime_error("GetUserSettings failed (" + error_code + ", " + error_msg + ").");
    }

    auto user_responses = find_tag(response, autodiscover_ns, "UserResponses");

    auto user_response = find_tag(user_responses, autodiscover_ns, "UserResponse");

    auto user_settings = find_tag(user_response, autodiscover_ns, "UserSettings");

    find_tags(user_settings, autodiscover_ns, "UserSetting", [&](xmlNodePtr c) {
        auto name = get_tag_content(find_tag(c, autodiscover_ns, "Name"));
        auto value = get_tag_content(find_tag(c, autodiscover_ns, "Value"));

        if (settings.count(name) != 0)
            settings[name] = value;
    });
}

static void get_user_settings(const string& url, const string& mailbox, map<string, string>& settings) {
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

    for (const auto& s : settings) {
        req.element_text("a:Setting", s.first);
    }

    req.end_element();

    req.end_element();
    req.end_element();
    req.end_document();

    auto ret = s.get(url, "http://schemas.microsoft.com/exchange/2010/Autodiscover/Autodiscover/GetUserSettings", req.dump());

    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Could not parse response.");

    try {
        parse_get_user_settings_response(find_tag(xmlDocGetRootElement(doc), autodiscover_ns, "GetUserSettingsResponseMessage"), settings);
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);
}

static void send_email(const string& url, const string& subject, const string& body, const string& addressee) {
    soap s;
    xml_writer req;

    req.start_document();
    req.start_element("m:CreateItem");
    req.attribute("MessageDisposition", "SendAndSaveCopy");

    req.start_element("m:SavedItemFolderId");
    req.start_element("t:DistinguishedFolderId");
    req.attribute("Id", "sentitems");
    req.end_element();
    req.end_element();

    req.start_element("m:Items");

    req.start_element("t:Message");
    req.element_text("t:Subject", subject);

    req.start_element("t:Body");
    req.attribute("BodyType", "HTML");
    req.text(body);
    req.end_element();

    req.start_element("t:ToRecipients");
    req.start_element("t:Mailbox");
    req.element_text("t:EmailAddress", addressee);
    req.end_element();
    req.end_element();

    req.end_element();

    req.end_element();

    req.end_element();

    req.end_document();

    auto ret = s.get(url, "", req.dump());

    printf("%s\n", ret.c_str());
}

static void main2() {
    map<string, string> settings{ { "InternalEwsUrl", "" } };

    get_user_settings("https://autodiscover.boltonft.nhs.uk/autodiscover/autodiscover.svc", "mark.harmstone@boltonft.nhs.uk", settings); // FIXME

    if (settings.at("InternalEwsUrl").empty())
        throw runtime_error("Could not find value for InternalEwsUrl.");

    send_email(settings.at("InternalEwsUrl"), "Interesting", "The merger is finalized.", "mark.harmstone@boltonft.nhs.uk");
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    try {
        main2();
    } catch (const exception& e) {
        cerr << e.what() << endl;
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();

    return 0;
}
