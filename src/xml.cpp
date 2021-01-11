#include "xml.h"
#include <stdexcept>
#include <string.h>

using namespace std;

xml_writer::xml_writer() {
    xmlInitParser();

    buf = xmlBufferCreate();
    if (!buf)
        throw runtime_error("xmlBufferCreate failed");

    writer = xmlNewTextWriterMemory(buf, 0);
    if (!writer) {
        xmlBufferFree(buf);
        throw runtime_error("xmlNewTextWriterMemory failed");
    }
}

xml_writer::~xml_writer() {
    xmlFreeTextWriter(writer);
    xmlBufferFree(buf);
}

string xml_writer::dump() const {
    return (char*)buf->content;
}

void xml_writer::start_document() {
    int rc = xmlTextWriterStartDocument(writer, nullptr, "UTF-8", nullptr);
    if (rc < 0)
        throw runtime_error("xmlTextWriterStartDocument failed (error " + to_string(rc) + ")");
}

void xml_writer::end_document() {
    int rc = xmlTextWriterEndDocument(writer);
    if (rc < 0)
        throw runtime_error("xmlTextWriterEndDocument failed (error " + to_string(rc) + ")");
}

void xml_writer::start_element(const string& tag, const unordered_map<string, string>& namespaces) {
    int rc = xmlTextWriterStartElement(writer, BAD_CAST tag.c_str());
    if (rc < 0)
        throw runtime_error("xmlTextWriterStartElement failed (error " + to_string(rc) + ")");

    for (const auto& ns : namespaces) {
        string att = ns.first.empty() ? "xmlns" : ("xmlns:" + ns.first);

        rc = xmlTextWriterWriteAttribute(writer, BAD_CAST att.c_str(), BAD_CAST ns.second.c_str());
        if (rc < 0)
            throw runtime_error("xmlTextWriterWriteAttribute failed (error " + to_string(rc) + ")");
    }
}

void xml_writer::end_element() {
    int rc = xmlTextWriterEndElement(writer);
    if (rc < 0)
        throw runtime_error("xmlTextWriterEndElement failed (error " + to_string(rc) + ")");
}

void xml_writer::text(const string& s) {
    int rc = xmlTextWriterWriteString(writer, BAD_CAST s.c_str());
    if (rc < 0)
        throw runtime_error("xmlTextWriterWriteString failed (error " + to_string(rc) + ")");
}

void xml_writer::attribute(const string& name, const string& value) {
    int rc = xmlTextWriterWriteAttribute(writer, BAD_CAST name.c_str(), BAD_CAST value.c_str());
    if (rc < 0)
        throw runtime_error("xmlTextWriterWriteAttribute failed (error " + to_string(rc) + ")");
}

void xml_writer::raw(const std::string_view& s) {
    int rc = xmlTextWriterWriteRawLen(writer, BAD_CAST s.data(), (int)s.length());
    if (rc < 0)
        throw runtime_error("xmlTextWriterWriteRawLen failed (error " + to_string(rc) + ")");
}

xmlNodePtr find_tag(xmlNodePtr root, const string& ns, const string& name) {
    xmlNodePtr n = root->children;

    while (n) {
        if (n->type == XML_ELEMENT_NODE && n->ns && !strcmp((char*)n->ns->href, ns.c_str()) && !strcmp((char*)n->name, name.c_str()))
            return n;

        n = n->next;
    }

    throw runtime_error("Could not find " + name + " tag");
}

string find_tag_content(xmlNodePtr root, const string& ns, const string& name) noexcept {
    xmlNodePtr n = root->children;

    while (n) {
        if (n->type == XML_ELEMENT_NODE && n->ns && !strcmp((char*)n->ns->href, ns.c_str()) && !strcmp((char*)n->name, name.c_str())) {
            auto xc = xmlNodeGetContent(n);

            if (!xc)
                return "";

            string ret{(char*)xc};

            xmlFree(xc);

            return ret;
        }

        n = n->next;
    }

    return "";
}

string find_tag_prop(xmlNodePtr root, const string& ns, const string& tag_name, const string& prop_name) noexcept {
    xmlNodePtr n = root->children;

    while (n) {
        if (n->type == XML_ELEMENT_NODE && n->ns && !strcmp((char*)n->ns->href, ns.c_str()) && !strcmp((char*)n->name, tag_name.c_str())) {
            auto xc = xmlGetProp(n, BAD_CAST prop_name.c_str());

            if (!xc)
                return "";

            string ret{(char*)xc};

            xmlFree(xc);

            return ret;
        }

        n = n->next;
    }

    return "";
}

void find_tags(xmlNodePtr n, const string& ns, const string& tag, const function<bool(xmlNodePtr)>& func) {
    auto c = n->children;

    while (c) {
        if (c->type == XML_ELEMENT_NODE && c->ns && !strcmp((char*)c->ns->href, ns.c_str()) && !strcmp((char*)c->name, tag.c_str()))
            if (!func(c))
                return;

        c = c->next;
    }
}

string get_prop(xmlNodePtr n, const string& name) noexcept {
    auto xc = xmlGetProp(n, BAD_CAST name.c_str());

    if (!xc)
        return "";

    string ret{(char*)xc};

    xmlFree(xc);

    return ret;
}
