#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>

namespace prospect {

class folder {
public:
    folder(const std::string_view& id, const std::string_view& parent, const std::string_view& change_key,
           const std::string_view& display_name, unsigned int total_count, unsigned int child_folder_count,
           unsigned int unread_count) :
           id(id), parent(parent), change_key(change_key), display_name(display_name), total_count(total_count),
           child_folder_count(child_folder_count), unread_count(unread_count) {
    }

    std::string id, parent, change_key, display_name;
    unsigned int total_count, child_folder_count, unread_count;
};

class folder_item {
public:
    folder_item(const std::string_view& id, const std::string_view& subject, const std::string_view& received,
                bool read, const std::string_view& sender_name, const std::string_view& sender_email,
                bool has_attachments) :
                id(id), subject(subject), received(received), read(read), sender_name(sender_name),
                sender_email(sender_email), has_attachments(has_attachments) {
    }

    std::string id, subject, received;
    bool read;
    std::string sender_name, sender_email;
    bool has_attachments;
};

class attachment {
public:
    attachment(const std::string_view& id, const std::string_view& name, size_t size, const std::string_view& modified) :
               id(id), name(name), size(size), modified(modified) {
    }

    std::string id, name;
    size_t size;
    std::string modified;
};

class prospect {
public:
    prospect(const std::string_view& domain = "");

    void get_domain_settings(const std::string& url, const std::string& domain, std::map<std::string, std::string>& settings);
    void get_user_settings(const std::string& url, const std::string& mailbox, std::map<std::string, std::string>& settings);
    void send_email(const std::string& subject, const std::string& body, const std::string& addressee);
    std::vector<folder> find_folders();
    void find_items(const std::string& folder, const std::function<bool(const folder_item&)>& func);
    std::vector<attachment> get_attachments(const std::string& item_id);
    std::string read_attachment(const std::string& id);
    void move_item(const std::string& id, const std::string& folder);
    std::string create_folder(const std::string_view& parent, const std::string_view& name, const std::vector<folder>& folders);

private:
    std::string url;
};

};