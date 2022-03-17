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

enum class importance {
    normal,
    low,
    high
};

class PROSPECT folder {
public:
    folder(std::string_view id, std::string_view parent, std::string_view change_key,
           std::string_view display_name, unsigned int total_count, unsigned int child_folder_count,
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

    void send_email() const;
    void send_reply(std::string_view item_id, std::string_view change_key, bool reply_all) const;

    prospect& p;
    std::string id, subject, received;
    bool read;
    std::string sender_name, sender_email;
    bool has_attachments;
    std::string conversation_id, internet_id, change_key, body;
    std::vector<std::string> recipients, cc, bcc;
    enum importance importance = importance::normal;
};

class PROSPECT attachment {
public:
    attachment(std::string_view id, std::string_view name, size_t size, std::string_view modified) :
               id(id), name(name), size(size), modified(modified) {
    }

    std::string id, name;
    size_t size;
    std::string modified;
};

class subscription;

class PROSPECT prospect {
public:
    prospect(std::string_view domain = "");
    ~prospect();

    void get_domain_settings(const std::string& url, std::string_view domain, std::map<std::string, std::string>& settings);
    void get_user_settings(const std::string& url, std::string_view mailbox, std::map<std::string, std::string>& settings);
    std::vector<folder> find_folders(std::string_view mailbox = "");
    void find_items(std::string_view folder, const std::function<bool(const mail_item&)>& func);
    bool get_item(std::string_view id, const std::function<bool(const mail_item&)>& func);
    std::vector<attachment> get_attachments(std::string_view item_id);
    std::string read_attachment(std::string_view id);
    std::string move_item(std::string_view id, std::string_view folder);
    std::string create_folder(std::string_view parent, std::string_view name, const std::vector<folder>& folders);

    friend class mail_item;
    friend class subscription;

private:
    std::string url;
};

enum class event {
    new_mail,
    created,
    deleted,
    modified,
    moved,
    copied,
    free_busy_changed,
    status
};

class PROSPECT subscription {
public:
    subscription(prospect& p, std::string_view parent, const std::vector<enum event>& events);
    ~subscription();

    void wait(unsigned int timeout, const std::function<void(enum event type, std::string_view timestamp, std::string_view item_id,
                                                             std::string_view item_change_key, std::string_view parent_id,
                                                             std::string_view parent_change_key)>& func);
    void cancel();

    prospect& p;

private:
    std::string id;
    bool cancelled = false;
};

};

#ifdef _MSC_VER
#pragma warning(pop)
#endif
