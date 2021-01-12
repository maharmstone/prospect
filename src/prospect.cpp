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
#include "b64.h"
#include "misc.h"

using namespace std;

static const string autodiscover_ns = "http://schemas.microsoft.com/exchange/2010/Autodiscover";
static const string messages_ns = "http://schemas.microsoft.com/exchange/services/2006/messages";
static const string types_ns = "http://schemas.microsoft.com/exchange/services/2006/types";

namespace prospect {

static string get_domain_name() {
    char16_t buf[255];
    DWORD size = sizeof(buf) / sizeof(char16_t);

    if (!GetComputerNameExW(ComputerNameDnsDomain, (WCHAR*)buf, &size))
        throw last_error("GetComputerNameEx", GetLastError());

    return utf16_to_utf8(buf);
}

prospect::prospect(const string_view& domain) {
    string dom;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    if (domain.empty())
        dom = get_domain_name();
    else
        dom = domain;

    map<string, string> settings{ { "ExternalEwsUrl", "" } };

    get_domain_settings("https://autodiscover." + dom + "/autodiscover/autodiscover.svc", dom, settings);

    if (settings.at("ExternalEwsUrl").empty())
        throw runtime_error("Could not find value for ExternalEwsUrl.");

    url = settings.at("ExternalEwsUrl");
}

prospect::~prospect() {
    curl_global_cleanup();
}

static void parse_get_user_settings_response(xmlNodePtr n, map<string, string>& settings) {
    auto response = find_tag(n, autodiscover_ns, "Response");

    auto error_code = find_tag_content(response, autodiscover_ns, "ErrorCode");

    if (error_code != "NoError") {
        auto error_msg = find_tag_content(response, autodiscover_ns, "ErrorMessage");

        throw runtime_error("GetUserSettings failed (" + error_code + ", " + error_msg + ").");
    }

    auto user_responses = find_tag(response, autodiscover_ns, "UserResponses");

    auto user_response = find_tag(user_responses, autodiscover_ns, "UserResponse");

    auto user_settings = find_tag(user_response, autodiscover_ns, "UserSettings");

    find_tags(user_settings, autodiscover_ns, "UserSetting", [&](xmlNodePtr c) {
        auto name = find_tag_content(c, autodiscover_ns, "Name");
        auto value = find_tag_content(c, autodiscover_ns, "Value");

        if (settings.count(name) != 0)
            settings[name] = value;

        return true;
    });
}

void prospect::get_user_settings(const string& url, const string& mailbox, map<string, string>& settings) {
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

    auto error_code = find_tag_content(response, autodiscover_ns, "ErrorCode");

    if (error_code != "NoError") {
        auto error_msg = find_tag_content(response, autodiscover_ns, "ErrorMessage");

        throw runtime_error("GetDomainSettings failed (" + error_code + ", " + error_msg + ").");
    }

    auto user_responses = find_tag(response, autodiscover_ns, "DomainResponses");

    auto user_response = find_tag(user_responses, autodiscover_ns, "DomainResponse");

    auto user_settings = find_tag(user_response, autodiscover_ns, "DomainSettings");

    find_tags(user_settings, autodiscover_ns, "DomainSetting", [&](xmlNodePtr c) {
        auto name = find_tag_content(c, autodiscover_ns, "Name");
        auto value = find_tag_content(c, autodiscover_ns, "Value");

        if (settings.count(name) != 0)
            settings[name] = value;

        return true;
    });
}

void prospect::get_domain_settings(const string& url, const string& domain, map<string, string>& settings) {
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

void prospect::send_email(const string& subject, const string& body, const vector<string>& addressees,
                          const vector<string>& cc, const vector<string>& bcc, const string& reply_to) {
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

    if (!addressees.empty()) {
        req.start_element("t:ToRecipients");

        for (const auto& ad : addressees) {
            req.start_element("t:Mailbox");
            req.element_text("t:EmailAddress", ad);
            req.end_element();
        }

        req.end_element();
    }

    if (!cc.empty()) {
        req.start_element("t:CcRecipients");

        for (const auto& ad : cc) {
            req.start_element("t:Mailbox");
            req.element_text("t:EmailAddress", ad);
            req.end_element();
        }

        req.end_element();
    }

    if (!addressees.empty()) {
        req.start_element("t:BccRecipients");

        for (const auto& ad : bcc) {
            req.start_element("t:Mailbox");
            req.element_text("t:EmailAddress", ad);
            req.end_element();
        }

        req.end_element();
    }

    if (!reply_to.empty())
        req.element_text("t:InReplyTo", reply_to);

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
            auto response_code = find_tag_content(cirm, messages_ns, "ResponseCode");

            throw runtime_error("CreateItem failed (" + response_class + ", " + response_code + ").");
        }
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);
}

void prospect::send_reply(const string& item_id, const string& change_key, bool reply_all, const string& body,
                          const string& subject, const vector<string>& addressees, const vector<string>& cc,
                          const vector<string>& bcc) {
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

    req.start_element(reply_all ? "t:ReplyAllToItem" : "t:ReplyToItem");

    req.start_element("t:ReferenceItemId");
    req.attribute("Id", item_id);
    req.attribute("ChangeKey", change_key);
    req.end_element();

    req.start_element("t:NewBodyContent");
    req.attribute("BodyType", "HTML");
    req.text(body);
    req.end_element();

    if (!subject.empty())
        req.element_text("t:Subject", subject);

    if (!addressees.empty()) {
        req.start_element("t:ToRecipients");

        for (const auto& ad : addressees) {
            req.start_element("t:Mailbox");
            req.element_text("t:EmailAddress", ad);
            req.end_element();
        }

        req.end_element();
    }

    if (!cc.empty()) {
        req.start_element("t:CcRecipients");

        for (const auto& ad : cc) {
            req.start_element("t:Mailbox");
            req.element_text("t:EmailAddress", ad);
            req.end_element();
        }

        req.end_element();
    }

    if (!addressees.empty()) {
        req.start_element("t:BccRecipients");

        for (const auto& ad : bcc) {
            req.start_element("t:Mailbox");
            req.element_text("t:EmailAddress", ad);
            req.end_element();
        }

        req.end_element();
    }

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
            auto response_code = find_tag_content(cirm, messages_ns, "ResponseCode");

            throw runtime_error("CreateItem failed (" + response_class + ", " + response_code + ").");
        }
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);
}

static void field_uri(xml_writer& req, const string& uri) {
    req.start_element("t:FieldURI");
    req.attribute("FieldURI", uri);
    req.end_element();
}

vector<folder> prospect::find_folders(const string& mailbox) {
    soap s;
    xml_writer req;

    req.start_document();
    req.start_element("m:FindFolder");
    req.attribute("Traversal", "Deep");

    req.start_element("m:FolderShape");
    req.element_text("t:BaseShape", "Default");

    req.start_element("t:AdditionalProperties");
    field_uri(req, "folder:ParentFolderId");
    req.end_element();

    req.end_element();

    req.start_element("m:ParentFolderIds");
    req.start_element("t:DistinguishedFolderId");
    req.attribute("Id", "root");

    if (!mailbox.empty()) {
        req.start_element("t:Mailbox");
        req.element_text("t:EmailAddress", mailbox);
        req.end_element();
    }

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
            auto response_code = find_tag_content(ffrm, messages_ns, "ResponseCode");

            throw runtime_error("FindFolder failed (" + response_class + ", " + response_code + ").");
        }

        auto root_folder = find_tag(ffrm, messages_ns, "RootFolder");

        auto folders_tag = find_tag(root_folder, types_ns, "Folders");

        find_tags(folders_tag, types_ns, "Folder", [&](xmlNodePtr c) {
            auto folder_id = find_tag(c, types_ns, "FolderId");
            auto parent = get_prop(find_tag(c, types_ns, "ParentFolderId"), "Id");
            auto id = get_prop(folder_id, "Id");
            auto change_key = get_prop(folder_id, "ChangeKey");

            auto display_name = find_tag_content(c, types_ns, "DisplayName");
            auto total_count = stoul(find_tag_content(c, types_ns, "TotalCount"));
            auto child_folder_count = stoul(find_tag_content(c, types_ns, "ChildFolderCount"));
            auto unread_count = stoul(find_tag_content(c, types_ns, "UnreadCount"));

            folders.emplace_back(id, parent, change_key, display_name, total_count, child_folder_count, unread_count);

            return true;
        });
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);

    return folders;
}

void prospect::find_items(const string& folder, const function<bool(const mail_item&)>& func) {
    soap s;
    xml_writer req;

    req.start_document();
    req.start_element("m:FindItem");
    req.attribute("Traversal", "Shallow");

    req.start_element("m:ItemShape");
    req.element_text("t:BaseShape", "IdOnly");

    req.start_element("t:AdditionalProperties");
    field_uri(req, "item:Subject");
    field_uri(req, "item:DateTimeReceived");
    field_uri(req, "message:Sender");
    field_uri(req, "message:IsRead");
    field_uri(req, "item:HasAttachments");
    field_uri(req, "item:ConversationId");
    field_uri(req, "message:InternetMessageId");
    req.end_element();
    req.end_element();

    req.start_element("m:SortOrder");
    req.start_element("t:FieldOrder");
    req.attribute("Order", "Ascending");
    field_uri(req, "item:DateTimeReceived");
    req.end_element();
    req.end_element();

    req.start_element("m:ParentFolderIds");
    req.start_element("t:FolderId");
    req.attribute("Id", folder);
    req.end_element();
    req.end_element();

    req.end_element();

    req.end_document();

    // FIXME - only get so many at once?

    auto ret = s.get(url, "", "<t:RequestServerVersion Version=\"Exchange2010\" />", req.dump());

    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Could not parse response.");

    try {
        auto response = find_tag(xmlDocGetRootElement(doc), messages_ns, "FindItemResponse");

        auto response_messages = find_tag(response, messages_ns, "ResponseMessages");

        auto ffrm = find_tag(response_messages, messages_ns, "FindItemResponseMessage");

        auto response_class = get_prop(ffrm, "ResponseClass");

        if (response_class != "Success") {
            auto response_code = find_tag_content(ffrm, messages_ns, "ResponseCode");

            throw runtime_error("FindItem failed (" + response_class + ", " + response_code + ").");
        }

        auto root_folder = find_tag(ffrm, messages_ns, "RootFolder");

        auto items_tag = find_tag(root_folder, types_ns, "Items");

        find_tags(items_tag, types_ns, "Message", [&](xmlNodePtr c) {
            auto id = get_prop(find_tag(c, types_ns, "ItemId"), "Id");
            auto change_key = get_prop(find_tag(c, types_ns, "ItemId"), "ChangeKey");
            auto subj = find_tag_content(c, types_ns, "Subject");
            auto received = find_tag_content(c, types_ns, "DateTimeReceived");
            bool read = find_tag_content(c, types_ns, "IsRead") == "true";
            bool has_attachments = find_tag_content(c, types_ns, "HasAttachments") == "true";

            auto sender = find_tag(c, types_ns, "Sender");
            auto sender_mailbox = find_tag(sender, types_ns, "Mailbox");

            auto sender_name = find_tag_content(sender_mailbox, types_ns, "Name");
            auto sender_email = find_tag_content(sender_mailbox, types_ns, "EmailAddress");

            auto conversation_id = find_tag_prop(c, types_ns, "ConversationId", "Id");
            auto internet_id = find_tag_content(c, types_ns, "InternetMessageId");

            mail_item item(*this);

            item.id = id;
            item.subject = subj;
            item.received = received;
            item.read = read;
            item.sender_name = sender_name;
            item.sender_email = sender_email;
            item.has_attachments = has_attachments;
            item.conversation_id = conversation_id;
            item.internet_id = internet_id;
            item.change_key = change_key;

            return func(item);
        });
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);
}

bool prospect::get_item(const string& id, const function<bool(const mail_item&)>& func) {
    soap s;
    xml_writer req;
    bool found = false;

    req.start_document();
    req.start_element("m:GetItem");

    req.start_element("m:ItemShape");
    req.element_text("t:BaseShape", "IdOnly");

    req.start_element("t:AdditionalProperties");
    field_uri(req, "item:Subject");
    field_uri(req, "item:DateTimeReceived");
    field_uri(req, "message:Sender");
    field_uri(req, "message:IsRead");
    field_uri(req, "item:HasAttachments");
    field_uri(req, "item:ConversationId");
    field_uri(req, "message:InternetMessageId");
    req.end_element();
    req.end_element();

    req.start_element("m:ItemIds");
    req.start_element("t:ItemId");
    req.attribute("Id", id);
    req.end_element();
    req.end_element();

    req.end_element();

    req.end_document();

    auto ret = s.get(url, "", "<t:RequestServerVersion Version=\"Exchange2010\" />", req.dump());

    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Could not parse response.");

    try {
        auto response = find_tag(xmlDocGetRootElement(doc), messages_ns, "GetItemResponse");

        auto response_messages = find_tag(response, messages_ns, "ResponseMessages");

        auto girm = find_tag(response_messages, messages_ns, "GetItemResponseMessage");

        auto response_class = get_prop(girm, "ResponseClass");

        if (response_class != "Success") {
            auto response_code = find_tag_content(girm, messages_ns, "ResponseCode");

            if (response_code == "ErrorItemNotFound") {
                xmlFreeDoc(doc);
                return false;
            }

            throw runtime_error("GetItem failed (" + response_class + ", " + response_code + ").");
        }

        auto items_tag = find_tag(girm, messages_ns, "Items");

        find_tags(items_tag, types_ns, "Message", [&](xmlNodePtr c) {
            auto id = get_prop(find_tag(c, types_ns, "ItemId"), "Id");
            auto change_key = get_prop(find_tag(c, types_ns, "ItemId"), "ChangeKey");
            auto subj = find_tag_content(c, types_ns, "Subject");
            auto received = find_tag_content(c, types_ns, "DateTimeReceived");
            bool read = find_tag_content(c, types_ns, "IsRead") == "true";
            bool has_attachments = find_tag_content(c, types_ns, "HasAttachments") == "true";

            auto sender = find_tag(c, types_ns, "Sender");
            auto sender_mailbox = find_tag(sender, types_ns, "Mailbox");

            auto sender_name = find_tag_content(sender_mailbox, types_ns, "Name");
            auto sender_email = find_tag_content(sender_mailbox, types_ns, "EmailAddress");

            auto conversation_id = find_tag_prop(c, types_ns, "ConversationId", "Id");
            auto internet_id = find_tag_content(c, types_ns, "InternetMessageId");

            mail_item item(*this);

            item.id = id;
            item.subject = subj;
            item.received = received;
            item.read = read;
            item.sender_name = sender_name;
            item.sender_email = sender_email;
            item.has_attachments = has_attachments;
            item.conversation_id = conversation_id;
            item.internet_id = internet_id;
            item.change_key = change_key;

            found = true;
            func(item);

            return false;
        });
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);

    return found;
}

vector<attachment> prospect::get_attachments(const string& item_id) {
    soap s;
    xml_writer req;

    req.start_document();
    req.start_element("m:GetItem");

    req.start_element("m:ItemShape");
    req.element_text("t:BaseShape", "IdOnly");
    req.start_element("t:AdditionalProperties");
    field_uri(req, "item:Attachments");
    req.end_element();
    req.end_element();

    req.start_element("m:ItemIds");
    req.start_element("t:ItemId");
    req.attribute("Id", item_id);
    req.end_element();
    req.end_element();

    req.end_element();

    req.end_document();

    auto ret = s.get(url, "", "<t:RequestServerVersion Version=\"Exchange2010\" />", req.dump());

    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Could not parse response.");

    vector<attachment> v;

    try {
        auto response = find_tag(xmlDocGetRootElement(doc), messages_ns, "GetItemResponse");

        auto response_messages = find_tag(response, messages_ns, "ResponseMessages");

        auto ffrm = find_tag(response_messages, messages_ns, "GetItemResponseMessage");

        auto response_class = get_prop(ffrm, "ResponseClass");

        if (response_class != "Success") {
            auto response_code = find_tag_content(ffrm, messages_ns, "ResponseCode");

            throw runtime_error("GetItem failed (" + response_class + ", " + response_code + ").");
        }

        auto items_tag = find_tag(ffrm, messages_ns, "Items");

        find_tags(items_tag, types_ns, "Message", [&](xmlNodePtr c) {
            auto attachments = find_tag(c, types_ns, "Attachments");

            find_tags(attachments, types_ns, "FileAttachment", [&](xmlNodePtr c) {
                bool is_inline = find_tag_content(c, types_ns, "IsInline") == "true";
                bool is_contact_photo = find_tag_content(c, types_ns, "IsContactPhoto") == "true";

                if (!is_inline && !is_contact_photo) {
                    auto id = get_prop(find_tag(c, types_ns, "AttachmentId"), "Id");
                    auto name = find_tag_content(c, types_ns, "Name");
                    auto size = stoull(find_tag_content(c, types_ns, "Size"));
                    auto modified = find_tag_content(c, types_ns, "LastModifiedTime");

                    v.emplace_back(id, name, size, modified);
                }

                return true;
            });

            return true;
        });
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);

    return v;
}

string prospect::read_attachment(const string& id) {
    soap s;
    xml_writer req;

    req.start_document();
    req.start_element("m:GetAttachment");

    req.start_element("m:AttachmentIds");
    req.start_element("t:AttachmentId");
    req.attribute("Id", id);
    req.end_element();
    req.end_element();

    req.end_element();

    req.end_document();

    auto ret = s.get(url, "", "<t:RequestServerVersion Version=\"Exchange2010\" />", req.dump());

    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Could not parse response.");

    string content;

    try {
        auto response = find_tag(xmlDocGetRootElement(doc), messages_ns, "GetAttachmentResponse");

        auto response_messages = find_tag(response, messages_ns, "ResponseMessages");

        auto ffrm = find_tag(response_messages, messages_ns, "GetAttachmentResponseMessage");

        auto response_class = get_prop(ffrm, "ResponseClass");

        if (response_class != "Success") {
            auto response_code = find_tag_content(ffrm, messages_ns, "ResponseCode");

            throw runtime_error("GetAttachment failed (" + response_class + ", " + response_code + ").");
        }

        auto attachments = find_tag(ffrm, messages_ns, "Attachments");

        auto file_att = find_tag(attachments, types_ns, "FileAttachment");

        content = find_tag_content(file_att, types_ns, "Content");
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);

    return b64decode(content);
}

string prospect::move_item(const string& id, const string& folder) {
    soap s;
    xml_writer req;
    string new_id;

    req.start_document();
    req.start_element("m:MoveItem");

    req.start_element("m:ToFolderId");
    req.start_element("t:FolderId");
    req.attribute("Id", folder);
    req.end_element();
    req.end_element();

    req.start_element("m:ItemIds");
    req.start_element("t:ItemId");
    req.attribute("Id", id);
    req.end_element();
    req.end_element();

    req.end_element();

    req.end_document();

    auto ret = s.get(url, "", "<t:RequestServerVersion Version=\"Exchange2010\" />", req.dump());

    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Could not parse response.");

    try {
        auto response = find_tag(xmlDocGetRootElement(doc), messages_ns, "MoveItemResponse");

        auto response_messages = find_tag(response, messages_ns, "ResponseMessages");

        auto mirm = find_tag(response_messages, messages_ns, "MoveItemResponseMessage");

        auto response_class = get_prop(mirm, "ResponseClass");

        if (response_class != "Success") {
            auto response_code = find_tag_content(mirm, messages_ns, "ResponseCode");

            throw runtime_error("MoveItem failed (" + response_class + ", " + response_code + ").");
        }

        auto items = find_tag(mirm, messages_ns, "Items");

        auto msg = find_tag(items, types_ns, "Message");

        auto item_id = find_tag(msg, types_ns, "ItemId");

        new_id = get_prop(item_id, "Id");
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);

    return new_id;
}

string prospect::create_folder(const string_view& parent, const string_view& name, const vector<folder>& folders) {
    for (const auto& f : folders) {
        if (f.parent == parent && f.display_name == name)
            return f.id;
    }

    soap s;
    xml_writer req;

    req.start_document();
    req.start_element("m:CreateFolder");

    req.start_element("m:ParentFolderId");
    req.start_element("t:FolderId");
    req.attribute("Id", string(parent));
    req.end_element();
    req.end_element();

    req.start_element("m:Folders");
    req.start_element("t:Folder");
    req.element_text("t:FolderClass", "IPF.Note");
    req.element_text("t:DisplayName", string(name));
    req.end_element();
    req.end_element();

    req.end_element();

    req.end_document();

    auto ret = s.get(url, "", "<t:RequestServerVersion Version=\"Exchange2010\" />", req.dump());

    xmlDocPtr doc = xmlReadMemory(ret.data(), (int)ret.length(), nullptr, nullptr, 0);

    if (!doc)
        throw runtime_error("Could not parse response.");

    string id;

    try {
        auto response = find_tag(xmlDocGetRootElement(doc), messages_ns, "CreateFolderResponse");

        auto response_messages = find_tag(response, messages_ns, "ResponseMessages");

        auto ffrm = find_tag(response_messages, messages_ns, "CreateFolderResponseMessage");

        auto response_class = get_prop(ffrm, "ResponseClass");

        if (response_class != "Success") {
            auto response_code = find_tag_content(ffrm, messages_ns, "ResponseCode");

            throw runtime_error("CreateFolder failed (" + response_class + ", " + response_code + ").");
        }

        auto folders = find_tag(ffrm, messages_ns, "Folders");

        auto folder = find_tag(folders, types_ns, "Folder");

        id = get_prop(find_tag(folder, types_ns, "FolderId"), "Id");
    } catch (...) {
        xmlFreeDoc(doc);
        throw;
    }

    xmlFreeDoc(doc);

    return id;
}

}
