#ifndef JSON_HPP
#define JSON_HPP
// Minimal JSON parser (enough for scenario.json and iperf3 -J output).
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

struct Json {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> arr;
    std::vector<std::string> keys;
    std::vector<Json> vals;

    const Json* get(const std::string& k) const {
        if (type != Obj) return nullptr;
        for (size_t i = 0; i < keys.size(); ++i) if (keys[i] == k) return &vals[i];
        return nullptr;
    }
    bool has(const std::string& k) const { return get(k) != nullptr; }
};

class JsonParser {
    const std::string& t;
    size_t i = 0;
    [[noreturn]] void fail(const char* m) const {
        throw std::runtime_error(std::string("JSON parse error: ") + m + " at offset " + std::to_string(i));
    }
    void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\t' || t[i] == '\r')) ++i; }
    bool lit(const char* w) {
        size_t n = std::char_traits<char>::length(w);
        if (t.compare(i, n, w) == 0) { i += n; return true; }
        return false;
    }
    std::string str() {
        std::string o;
        ++i;  // opening quote
        while (i < t.size() && t[i] != '"') {
            char c = t[i++];
            if (c != '\\') { o += c; continue; }
            if (i >= t.size()) fail("bad escape");
            char e = t[i++];
            switch (e) {
                case 'n': o += '\n'; break; case 't': o += '\t'; break; case 'r': o += '\r'; break;
                case 'b': o += '\b'; break; case 'f': o += '\f'; break;
                case 'u': {
                    if (i + 4 > t.size()) fail("bad \\u");
                    unsigned cp = (unsigned)std::strtoul(t.substr(i, 4).c_str(), nullptr, 16);
                    i += 4;
                    if (cp < 0x80) o += (char)cp;
                    else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
                    else { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
                    break;
                }
                default: o += e;
            }
        }
        if (i >= t.size()) fail("unterminated string");
        ++i;
        return o;
    }
    Json value() {
        ws();
        if (i >= t.size()) fail("unexpected end");
        Json j;
        char c = t[i];
        if (c == '{') {
            j.type = Json::Obj; ++i; ws();
            if (i < t.size() && t[i] == '}') { ++i; return j; }
            while (true) {
                ws();
                if (i >= t.size() || t[i] != '"') fail("expected key");
                j.keys.push_back(str()); ws();
                if (i >= t.size() || t[i] != ':') fail("expected ':'");
                ++i; j.vals.push_back(value()); ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == '}') { ++i; break; }
                fail("expected ',' or '}'");
            }
        } else if (c == '[') {
            j.type = Json::Arr; ++i; ws();
            if (i < t.size() && t[i] == ']') { ++i; return j; }
            while (true) {
                j.arr.push_back(value()); ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == ']') { ++i; break; }
                fail("expected ',' or ']'");
            }
        } else if (c == '"') { j.type = Json::Str; j.s = str(); }
        else if (lit("true")) { j.type = Json::Bool; j.b = true; }
        else if (lit("false")) { j.type = Json::Bool; }
        else if (lit("null")) { j.type = Json::Null; }
        else {
            char* end = nullptr;
            j.n = std::strtod(t.c_str() + i, &end);
            if (end == t.c_str() + i) fail("unexpected character");
            i = (size_t)(end - t.c_str());
            j.type = Json::Num;
        }
        return j;
    }
public:
    explicit JsonParser(const std::string& text) : t(text) {}
    Json parse() { return value(); }
};

inline Json parseJson(const std::string& text) { return JsonParser(text).parse(); }

// jget(&root, {"end","sum_received","bits_per_second"}) -> pointer or nullptr
inline const Json* jget(const Json* j, std::initializer_list<const char*> path) {
    for (const char* p : path) { if (!j) return nullptr; j = j->get(p); }
    return j;
}

#endif
