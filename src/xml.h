#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <stack>
#include <libxml/tree.h>

class xml_writer {
public:
    std::string dump() const;
    void start_document();
    void start_element(std::string_view tag, const std::unordered_map<std::string, std::string>& namespaces = {});
    void end_element();
    void text(std::string_view s);
    void raw(std::string_view s);

    void element_text(std::string_view tag, std::string_view s, const std::unordered_map<std::string, std::string>& namespaces = {}) {
        start_element(tag, namespaces);
        text(s);
        end_element();
    }

    void attribute(std::string_view name, std::string_view value);

private:
    void flush_tag();
    std::string escape(std::string_view s, bool att);

    std::string buf;
    bool unflushed = false;
    std::stack<std::string> tag_names;
    std::unordered_map<std::string, std::string> atts;
    bool empty_tag;
};

xmlNodePtr find_tag(xmlNodePtr root, const std::string& ns, const std::string& name);
void find_tags(xmlNodePtr n, const std::string& ns, const std::string& tag, const std::function<bool(xmlNodePtr)>& func);
std::string find_tag_prop(xmlNodePtr root, const std::string& ns, const std::string& tag_name, const std::string& prop_name) noexcept;
std::string get_prop(xmlNodePtr n, const std::string& name) noexcept;
std::string find_tag_content(xmlNodePtr root, const std::string& ns, const std::string& name) noexcept;
