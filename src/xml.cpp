#include "xml.h"
#include "misc.h"
#include <stdexcept>
#include <string.h>

using namespace std;

string xml_writer::dump() const {
    return buf;
}

void xml_writer::start_document() {
    buf = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
}

string xml_writer::escape(const string_view& s, bool att) {
    bool need_escape = false;

    for (auto c : s) {
        if (c == '<' || c == '>' || c == '&' || (att && c == '"')) {
            need_escape = true;
            break;
        }
    }

    if (!need_escape)
        return string(s);

    string ret;

    ret.reserve(s.length());

    for (auto c : s) {
        switch (c) {
            case '<':
                ret += "&lt;";
                break;

            case '>':
                ret += "&gt;";
                break;

            case '&':
                ret += "&amp;";
                break;

            case '"':
                if (att) {
                    ret += "&quot;";
                    break;
                }
                [[fallthrough]];

            default:
                ret += c;
        }
    }

    return ret;
}

void xml_writer::flush_tag() {
    buf += "<" + tag_names.top();

    for (const auto& att : atts) {
        buf += " " + escape(att.first, false) + "=\"" + escape(att.second, true) + "\"";
    }

    if (empty_tag)
        buf += " />";
    else
        buf += ">";

    unflushed = false;
    atts.clear();
}

void xml_writer::start_element(const string_view& tag, const unordered_map<string, string>& namespaces) {
    if (unflushed) {
        empty_tag = false;
        flush_tag();
    }

    tag_names.push(string(tag));
    unflushed = true;
    empty_tag = true;

    for (const auto& ns : namespaces) {
        atts.emplace(ns.first.empty() ? "xmlns" : ("xmlns:" + ns.first), ns.second);
    }
}

void xml_writer::end_element() {
    bool need_end = true;

    if (unflushed) {
        if (empty_tag)
            need_end = false;

        flush_tag();
    }

    if (need_end)
        buf += "</" + tag_names.top() + ">";

    tag_names.pop();
}

void xml_writer::text(const string_view& s) {
    if (unflushed) {
        empty_tag = false;
        flush_tag();
    }

    buf += escape(s, false);
}

void xml_writer::attribute(const string_view& name, const string_view& value) {
    atts.emplace(name, value);
}

void xml_writer::raw(const string_view& s) {
    buf += s;
}

xmlNodePtr find_tag(xmlNodePtr root, const string& ns, const string& name) {
    xmlNodePtr n = root->children;

    while (n) {
        if (n->type == XML_ELEMENT_NODE && n->ns && !strcmp((char*)n->ns->href, ns.c_str()) && !strcmp((char*)n->name, name.c_str()))
            return n;

        n = n->next;
    }

    throw formatted_error(FMT_STRING("Could not find {} tag"), name);
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
