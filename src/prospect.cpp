#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <map>
#include <fmt/format.h>
#include "prospect.h"
#include "xml.h"
#include "soap.h"

using namespace std;

static const string autodiscover_ns = "http://schemas.microsoft.com/exchange/2010/Autodiscover";
static const string messages_ns = "http://schemas.microsoft.com/exchange/services/2006/messages";
static const string types_ns = "http://schemas.microsoft.com/exchange/services/2006/types";

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

    static const string action = "http://schemas.microsoft.com/exchange/2010/Autodiscover/Autodiscover/GetUserSettings";

    req.start_document();
    req.start_element("a:GetUserSettingsRequestMessage");
    req.start_element("a:Request");

    req.start_element("a:Users");
    req.start_element("a:User");
    req.element_text("a:Mailbox", mailbox);
    req.end_element();
    req.end_element();

    req.start_element("a:RequestedSettings");

    for (const auto& setting : settings) {
        req.element_text("a:Setting", setting.first);
    }

    req.end_element();

    req.end_element();
    req.end_element();
    req.end_document();

    string header = "<a:RequestedServerVersion>Exchange2010</a:RequestedServerVersion><wsa:Action>" + action + "</wsa:Action><wsa:To>" + url + "</wsa:To>";

    auto ret = s.get(url, "http://schemas.microsoft.com/exchange/2010/Autodiscover/Autodiscover/GetUserSettings", header, req.dump());

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

static void parse_get_domain_settings_response(xmlNodePtr n, map<string, string>& settings) {
    auto response = find_tag(n, autodiscover_ns, "Response");

    auto error_code = get_tag_content(find_tag(response, autodiscover_ns, "ErrorCode"));

    if (error_code != "NoError") {
        auto error_msg = get_tag_content(find_tag(response, autodiscover_ns, "ErrorMessage"));

        throw runtime_error("GetDomainSettings failed (" + error_code + ", " + error_msg + ").");
    }

    auto user_responses = find_tag(response, autodiscover_ns, "DomainResponses");

    auto user_response = find_tag(user_responses, autodiscover_ns, "DomainResponse");

    auto user_settings = find_tag(user_response, autodiscover_ns, "DomainSettings");

    find_tags(user_settings, autodiscover_ns, "DomainSetting", [&](xmlNodePtr c) {
        auto name = get_tag_content(find_tag(c, autodiscover_ns, "Name"));
        auto value = get_tag_content(find_tag(c, autodiscover_ns, "Value"));

        if (settings.count(name) != 0)
            settings[name] = value;
    });
}

static void get_domain_settings(const string& url, const string& domain, map<string, string>& settings) {
    soap s;
    xml_writer req;

    static const string action = "http://schemas.microsoft.com/exchange/2010/Autodiscover/Autodiscover/GetDomainSettings";

    req.start_document();
    req.start_element("a:GetDomainSettingsRequestMessage");
    req.start_element("a:Request");

    req.start_element("a:Domains");
    req.element_text("a:Domain", domain);
    req.end_element();

    req.start_element("a:RequestedSettings");

    for (const auto& setting : settings) {
        req.element_text("a:Setting", setting.first);
    }

    req.end_element();

    req.end_element();
    req.end_element();
    req.end_document();

    string header = "<a:RequestedServerVersion>Exchange2010</a:RequestedServerVersion><wsa:Action>" + action + "</wsa:Action><wsa:To>" + url + "</wsa:To>";

    auto ret = s.get(url, "http://schemas.microsoft.com/exchange/2010/Autodiscover/Autodiscover/GetDomainSettings", header, req.dump());

    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Could not parse response.");

    try {
        parse_get_domain_settings_response(find_tag(xmlDocGetRootElement(doc), autodiscover_ns, "GetDomainSettingsResponseMessage"), settings);
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

    auto ret = s.get(url, "", "<t:RequestServerVersion Version=\"Exchange2010\" />", req.dump());

    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Could not parse response.");

    try {
        auto response = find_tag(xmlDocGetRootElement(doc), messages_ns, "CreateItemResponse");

        auto response_messages = find_tag(response, messages_ns, "ResponseMessages");

        auto cirm = find_tag(response_messages, messages_ns, "CreateItemResponseMessage");

        auto response_class = get_prop(cirm, "ResponseClass");

        if (response_class != "Success") {
            auto response_code = get_tag_content(find_tag(cirm, messages_ns, "ResponseCode"));

            throw runtime_error("CreateItem failed (" + response_class + ", " + response_code + ").");
        }
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);
}

static vector<folder> find_folders(const string& url) {
    soap s;
    xml_writer req;

    req.start_document();
    req.start_element("m:FindFolder");
    req.attribute("Traversal", "Deep");

    req.start_element("m:FolderShape");
    req.element_text("t:BaseShape", "Default");

    req.start_element("t:AdditionalProperties");
    req.start_element("t:FieldURI");
    req.attribute("FieldURI", "folder:ParentFolderId");
    req.end_element();
    req.end_element();

    req.end_element();

    req.start_element("m:ParentFolderIds");
    req.start_element("t:DistinguishedFolderId");
    req.attribute("Id", "root");
    req.end_element();

    req.end_element();

    req.end_element();

    req.end_document();

    auto ret = s.get(url, "", "<t:RequestServerVersion Version=\"Exchange2010\" />", req.dump());

    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Could not parse response.");

    vector<folder> folders;

    try {
        auto response = find_tag(xmlDocGetRootElement(doc), messages_ns, "FindFolderResponse");

        auto response_messages = find_tag(response, messages_ns, "ResponseMessages");

        auto ffrm = find_tag(response_messages, messages_ns, "FindFolderResponseMessage");

        auto response_class = get_prop(ffrm, "ResponseClass");

        if (response_class != "Success") {
            auto response_code = get_tag_content(find_tag(ffrm, messages_ns, "ResponseCode"));

            throw runtime_error("FindFolder failed (" + response_class + ", " + response_code + ").");
        }

        auto root_folder = find_tag(ffrm, messages_ns, "RootFolder");

        auto folders_tag = find_tag(root_folder, types_ns, "Folders");

        find_tags(folders_tag, types_ns, "Folder", [&](xmlNodePtr c) {
            auto folder_id = find_tag(c, types_ns, "FolderId");
            auto parent = get_prop(find_tag(c, types_ns, "ParentFolderId"), "Id");
            auto id = get_prop(folder_id, "Id");
            auto change_key = get_prop(folder_id, "ChangeKey");

            auto display_name = get_tag_content(find_tag(c, types_ns, "DisplayName"));
            auto total_count = stoul(get_tag_content(find_tag(c, types_ns, "TotalCount")));
            auto child_folder_count = stoul(get_tag_content(find_tag(c, types_ns, "ChildFolderCount")));
            auto unread_count = stoul(get_tag_content(find_tag(c, types_ns, "UnreadCount")));

            folders.emplace_back(id, parent, change_key, display_name, total_count, child_folder_count, unread_count);
        });
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);

    return folders;
}

static string get_domain_name() {
    char16_t buf[255];
    DWORD size = sizeof(buf) / sizeof(char16_t);

    if (!GetComputerNameExW(ComputerNameDnsDomain, (WCHAR*)buf, &size))
        throw last_error("GetComputerNameEx", GetLastError());

    return utf16_to_utf8(buf);
}

static void main2() {
    map<string, string> settings{ { "ExternalEwsUrl", "" } };

    auto domain = get_domain_name();

    get_domain_settings("https://autodiscover." + domain + "/autodiscover/autodiscover.svc", domain, settings);
//     get_user_settings("https://autodiscover." + domain + "/autodiscover/autodiscover.svc", "mark.harmstone@" + domain, settings); // FIXME

    if (settings.at("ExternalEwsUrl").empty())
        throw runtime_error("Could not find value for ExternalEwsUrl.");

//     send_email(settings.at("ExternalEwsUrl"), "Interesting", "The merger is finalized.", "mark.harmstone@" + domain);

    auto folders = find_folders(settings.at("ExternalEwsUrl"));

    for (const auto& f : folders) {
        fmt::print("Folder: ID {}, parent {}, change key {}, display name {}, total {}, child folder count {}, unread {}\n",
                   f.id, f.parent, f.change_key, f.display_name, f.total_count, f.child_folder_count, f.unread_count);
    }
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
