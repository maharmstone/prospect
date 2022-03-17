#include <prospect.h>
#include <vector>
#include <string>
#include <iostream>
#include <fmt/format.h>

using namespace std;

static const prospect::folder& find_inbox(const vector<prospect::folder>& folders) {
    for (const auto& f : folders) {
        if (f.display_name == "Inbox")
            return f;
    }

    throw runtime_error("Folder \"Inbox\" not found.");
}

static const prospect::folder& find_folder(string_view parent, string_view name, const vector<prospect::folder>& folders) {
    for (const auto& f : folders) {
        if (f.parent == parent && f.display_name == name)
            return f;
    }

    throw runtime_error("Could not find folder " + string(name) + " with parent " + string(parent) + ".");
}

static void main2() {
    prospect::prospect p;

    //     p.send_email("Interesting", "The merger is finalized.", "mark.harmstone@" + domain);

    auto folders = p.find_folders();

    for (const auto& f : folders) {
        fmt::print("Folder: ID {}, parent {}, change key {}, display name {}, total {}, child folder count {}, unread {}\n",
                   f.id, f.parent, f.change_key, f.display_name, f.total_count, f.child_folder_count, f.unread_count);
    }

    const auto& inbox = find_inbox(folders);

    const auto& dir = find_folder(inbox.id, "Juno", folders);
    const auto& processed_dir_id = p.create_folder(dir.id, "processed", folders);

    p.find_items(dir.id, [&](const prospect::mail_item& item) {
        fmt::print("Message {}, subject {}, received {}, read {}, has attachments {}, sender {} <{}>\n", item.id, item.subject,
                   item.received, item.read, item.has_attachments, item.sender_name, item.sender_email);

        if (item.has_attachments) {
            auto attachments = p.get_attachments(item.id);

            for (const auto& att : attachments) {
                fmt::print("Attachment: ID {}, name {}, size {}, modified {}\n", att.id, att.name, att.size, att.modified);

                auto str = p.read_attachment(att.id);

                fmt::print("Content: {}\n", str);

                // FIXME - save attachment
            }
        }

        p.move_item(item.id, processed_dir_id);

        return true;
    });

    prospect::subscription sub(p, inbox.id, { prospect::event::new_mail });

    sub.wait(1, [](enum prospect::event type, string_view timestamp, string_view item_id, string_view item_change_key, string_view parent_id, string_view parent_change_key) {
        fmt::print("type = {}, timestamp = {}, item_id = {}, item_change_key = {}, parent_id = {}, parent_change_key = {}\n",
                   (unsigned int)type, timestamp, item_id, item_change_key, parent_id, parent_change_key);
    });
}

int main() {
    try {
        main2();
    } catch (const exception& e) {
        cerr << e.what() << endl;
        return 1;
    }

    return 0;
}
