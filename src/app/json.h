// ============================================================================
// json.h  -  A small JSON reader and writer, enough for the Claude API:
// parse what the server streams, keep it as it came (key order and number
// text included) and write it back. No raylib here, so json_tests can test it.
// ============================================================================
#pragma once
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace json {

struct Value {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool b = false;
    std::string s;                                    // String, or the Number's text
    std::vector<Value> items;                         // Array
    std::vector<std::pair<std::string, Value>> fields;   // Object, in order

    static Value Str(const std::string& v) { Value x; x.type = String; x.s = v; return x; }
    static Value Num(long long v) { Value x; x.type = Number; x.s = std::to_string(v); return x; }
    static Value Boolean(bool v) { Value x; x.type = Bool; x.b = v; return x; }
    static Value Arr() { Value x; x.type = Array; return x; }
    static Value Obj() { Value x; x.type = Object; return x; }

    bool IsObject() const { return type == Object; }
    // Object lookup: nullptr when missing (or not an object)
    const Value* Get(const std::string& key) const {
        if (type != Object) return nullptr;
        for (const auto& f : fields) if (f.first == key) return &f.second;
        return nullptr;
    }
    Value* Get(const std::string& key) {
        return const_cast<Value*>(static_cast<const Value*>(this)->Get(key));
    }
    // the string at key, or def
    std::string Text(const std::string& key, const std::string& def = "") const {
        const Value* v = Get(key);
        return v && v->type == String ? v->s : def;
    }
    Value& Set(const std::string& key, Value v) {
        type = Object;
        for (auto& f : fields) if (f.first == key) { f.second = std::move(v); return f.second; }
        fields.emplace_back(key, std::move(v));
        return fields.back().second;
    }
    Value& Push(Value v) { type = Array; items.push_back(std::move(v)); return items.back(); }
};

// ------------------------------------------------------------------ write --
inline void Escape(const std::string& in, std::string& out) {
    out += '"';
    for (unsigned char c : in) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); out += b; }
                else out += (char)c;   // UTF-8 bytes go through as they are
        }
    }
    out += '"';
}

inline void DumpTo(const Value& v, std::string& out) {
    switch (v.type) {
        case Value::Null: out += "null"; break;
        case Value::Bool: out += v.b ? "true" : "false"; break;
        case Value::Number: out += v.s.empty() ? "0" : v.s; break;
        case Value::String: Escape(v.s, out); break;
        case Value::Array:
            out += '[';
            for (size_t i = 0; i < v.items.size(); i++) { if (i) out += ','; DumpTo(v.items[i], out); }
            out += ']';
            break;
        case Value::Object:
            out += '{';
            for (size_t i = 0; i < v.fields.size(); i++) {
                if (i) out += ',';
                Escape(v.fields[i].first, out);
                out += ':';
                DumpTo(v.fields[i].second, out);
            }
            out += '}';
            break;
    }
}

inline std::string Dump(const Value& v) { std::string s; DumpTo(v, s); return s; }

// ------------------------------------------------------------------- read --
namespace detail {
struct Reader {
    const std::string& t;
    size_t i = 0;
    int depth = 0;
    explicit Reader(const std::string& text) : t(text) {}

    void Ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) i++; }
    bool Lit(const char* w) {
        size_t n = std::char_traits<char>::length(w);
        if (t.compare(i, n, w) != 0) return false;
        i += n;
        return true;
    }
    static void Utf8(unsigned cp, std::string& out) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    bool Hex4(unsigned& v) {
        if (i + 4 > t.size()) return false;
        v = 0;
        for (int k = 0; k < 4; k++) {
            char c = t[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return false;
        }
        return true;
    }
    bool Str(std::string& out) {
        if (i >= t.size() || t[i] != '"') return false;
        i++;
        while (i < t.size()) {
            char c = t[i++];
            if (c == '"') return true;
            if ((unsigned char)c < 0x20) return false;
            if (c != '\\') { out += c; continue; }
            if (i >= t.size()) return false;
            char e = t[i++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp;
                    if (!Hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {   // a surrogate pair
                        unsigned lo;
                        if (i + 6 <= t.size() && t[i] == '\\' && t[i + 1] == 'u') {
                            i += 2;
                            if (!Hex4(lo)) return false;
                            if (lo >= 0xDC00 && lo <= 0xDFFF) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            else { Utf8(0xFFFD, out); cp = lo; }
                        } else cp = 0xFFFD;
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) cp = 0xFFFD;
                    Utf8(cp, out);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }
    bool Val(Value& v) {
        if (++depth > 200) return false;
        Ws();
        if (i >= t.size()) return false;
        char c = t[i];
        bool ok = false;
        if (c == '{') {
            v.type = Value::Object;
            i++;
            Ws();
            if (i < t.size() && t[i] == '}') { i++; ok = true; }
            else for (;;) {
                Ws();
                std::string key;
                if (!Str(key)) break;
                Ws();
                if (i >= t.size() || t[i] != ':') break;
                i++;
                Value item;
                if (!Val(item)) break;
                v.fields.emplace_back(std::move(key), std::move(item));
                Ws();
                if (i < t.size() && t[i] == ',') { i++; continue; }
                if (i < t.size() && t[i] == '}') { i++; ok = true; }
                break;
            }
        } else if (c == '[') {
            v.type = Value::Array;
            i++;
            Ws();
            if (i < t.size() && t[i] == ']') { i++; ok = true; }
            else for (;;) {
                Value item;
                if (!Val(item)) break;
                v.items.push_back(std::move(item));
                Ws();
                if (i < t.size() && t[i] == ',') { i++; continue; }
                if (i < t.size() && t[i] == ']') { i++; ok = true; }
                break;
            }
        } else if (c == '"') {
            v.type = Value::String;
            ok = Str(v.s);
        } else if (Lit("true")) { v.type = Value::Bool; v.b = true; ok = true; }
        else if (Lit("false")) { v.type = Value::Bool; v.b = false; ok = true; }
        else if (Lit("null")) { v.type = Value::Null; ok = true; }
        else if (c == '-' || (c >= '0' && c <= '9')) {
            size_t s = i;
            if (t[i] == '-') i++;
            bool digits = false;
            while (i < t.size() && ((t[i] >= '0' && t[i] <= '9') || t[i] == '.' || t[i] == 'e' || t[i] == 'E' || t[i] == '+' || t[i] == '-')) {
                if (t[i] >= '0' && t[i] <= '9') digits = true;
                i++;
            }
            v.type = Value::Number;
            v.s = t.substr(s, i - s);
            ok = digits;
        }
        depth--;
        return ok;
    }
};
} // namespace detail

// False if text isn't one whole JSON value.
inline bool Parse(const std::string& text, Value& out) {
    detail::Reader r(text);
    out = Value();
    if (!r.Val(out)) return false;
    r.Ws();
    return r.i == text.size();
}

} // namespace json
