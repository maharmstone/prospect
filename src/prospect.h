#pragma once

#include <windows.h>
#include <string>
#include <stdexcept>

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

static __inline std::u16string utf8_to_utf16(const std::string_view& s) {
    std::u16string ret;

    if (s.empty())
        return u"";

    auto len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.length(), nullptr, 0);

    if (len == 0)
        throw std::runtime_error("MultiByteToWideChar 1 failed.");

    ret.resize(len);

    len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.length(), (wchar_t*)ret.data(), len);

    if (len == 0)
        throw std::runtime_error("MultiByteToWideChar 2 failed.");

    return ret;
}

static __inline std::string utf16_to_utf8(const std::u16string_view& s) {
    std::string ret;

    if (s.empty())
        return "";

    auto len = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)s.data(), (int)s.length(), nullptr, 0,
                                   nullptr, nullptr);

    if (len == 0)
        throw std::runtime_error("WideCharToMultiByte 1 failed.");

    ret.resize(len);

    len = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)s.data(), (int)s.length(), ret.data(), len,
                              nullptr, nullptr);

    if (len == 0)
        throw std::runtime_error("WideCharToMultiByte 2 failed.");

    return ret;
}

class last_error : public std::exception {
public:
    last_error(const std::string_view& function, int le) {
        std::string nice_msg;

        {
            char16_t* fm;

            if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                le, 0, reinterpret_cast<LPWSTR>(&fm), 0, nullptr)) {
                try {
                    std::u16string_view s = fm;

                    while (!s.empty() && (s[s.length() - 1] == u'\r' || s[s.length() - 1] == u'\n')) {
                        s.remove_suffix(1);
                    }

                    nice_msg = utf16_to_utf8(s);
                } catch (...) {
                    LocalFree(fm);
                    throw;
                }

                LocalFree(fm);
            }
        }

        msg = std::string(function) + " failed (error " + std::to_string(le) + (!nice_msg.empty() ? (", " + nice_msg) : "") + ").";
    }

    const char* what() const noexcept {
        return msg.c_str();
    }

private:
    std::string msg;
};

class formatted_error : public std::exception {
public:
    template<typename... Args>
    formatted_error(const std::string_view& s, Args&&... args) {
        msg = fmt::format(s, std::forward<Args>(args)...);
    }

    const char* what() const noexcept {
        return msg.c_str();
    }

private:
    std::string msg;
};

class folder_item {
public:
    folder_item(const std::string_view& id, const std::string_view& subject, const std::string_view& received,
                bool read, const std::string_view& sender_name, const std::string_view& sender_email) :
                id(id), subject(subject), received(received), read(read), sender_name(sender_name),
                sender_email(sender_email) {
    }

    std::string id, subject, received;
    bool read;
    std::string sender_name, sender_email;
};
