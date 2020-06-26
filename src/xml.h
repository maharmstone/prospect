#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <libxml/xmlwriter.h>

class xml_writer {
public:
    xml_writer();
    ~xml_writer();
    std::string dump() const;
    void start_document();
    void end_document();
    void start_element(const std::string& tag, const std::unordered_map<std::string, std::string>& namespaces = {});
    void end_element();
    void text(const std::string& s);
    void raw(const std::string& s);

    void element_text(const std::string& tag, const std::string& s, const std::unordered_map<std::string, std::string>& namespaces = {}) {
        start_element(tag, namespaces);
        text(s);
        end_element();
    }

    void attribute(const std::string& name, const std::string& value);

private:
    xmlBufferPtr buf;
    xmlTextWriterPtr writer;
};

xmlNodePtr find_tag(xmlNodePtr root, const std::string& ns, const std::string& name);
std::string get_tag_content(xmlNodePtr n);
void find_tags(xmlNodePtr n, const std::string& ns, const std::string& tag, const std::function<void(xmlNodePtr)>& func);
std::string get_prop(xmlNodePtr n, const std::string& name);
