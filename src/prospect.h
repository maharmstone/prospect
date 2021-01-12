#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>

#ifdef _WIN32

#ifdef PROSPECT_EXPORT
#define PROSPECT __declspec(dllexport)
#elif !defined(PROSPECT_STATIC)
#define PROSPECT __declspec(dllimport)
#else
#define PROSPECT
#endif

#else

#ifdef PROSPECT_EXPORT
#define PROSPECT __attribute__ ((visibility ("default")))
#elif !defined(PROSPECT_STATIC)
#define PROSPECT __attribute__ ((dllimport))
#else
#define PROSPECT
#endif

#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

namespace prospect {

class PROSPECT folder {
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

class prospect;

class PROSPECT mail_item {
public:
    mail_item(prospect& p) : p(p) { }

    prospect& p;
    std::string id, subject, received;
    bool read;
    std::string sender_name, sender_email;
    bool has_attachments;
    std::string conversation_id, internet_id, change_key;
};

class PROSPECT attachment {
public:
    attachment(const std::string_view& id, const std::string_view& name, size_t size, const std::string_view& modified) :
               id(id), name(name), size(size), modified(modified) {
    }

    std::string id, name;
    size_t size;
    std::string modified;
};

class PROSPECT prospect {
public:
    prospect(const std::string_view& domain = "");
    ~prospect();

    void get_domain_settings(const std::string& url, const std::string& domain, std::map<std::string, std::string>& settings);
    void get_user_settings(const std::string& url, const std::string& mailbox, std::map<std::string, std::string>& settings);
    void send_email(const std::string& subject, const std::string& body, const std::vector<std::string>& addressee,
                    const std::vector<std::string>& cc, const std::vector<std::string>& bcc, const std::string& conversation_id);
    void send_reply(const std::string& item_id, const std::string& change_key, bool reply_all, const std::string& body,
                    const std::string& subject = "", const std::vector<std::string>& addressees = {},
                    const std::vector<std::string>& cc = {}, const std::vector<std::string>& bcc = {});
    std::vector<folder> find_folders(const std::string& mailbox = "");
    void find_items(const std::string& folder, const std::function<bool(const mail_item&)>& func);
    bool get_item(const std::string& id, const std::function<bool(const mail_item&)>& func);
    std::vector<attachment> get_attachments(const std::string& item_id);
    std::string read_attachment(const std::string& id);
    std::string move_item(const std::string& id, const std::string& folder);
    std::string create_folder(const std::string_view& parent, const std::string_view& name, const std::vector<folder>& folders);

    friend class mail_item;

private:
    std::string url;
};

};

#ifdef _MSC_VER
#pragma warning(pop)
#endif
