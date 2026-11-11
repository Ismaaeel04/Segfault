// json_tests: the small JSON reader/writer used by SegFault AI (src/app/json.h).
#include "../src/app/json.h"
#include <cstdio>

static int fails = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; std::printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
    using json::Value;
    // round trip keeps order, numbers as written, nesting
    {
        std::string in = R"({"type":"content_block_start","index":0,"content_block":{"type":"thinking","thinking":"","signature":""},"n":-1.5e3,"ok":true,"no":null,"a":[1,2,[]]})";
        Value v;
        CHECK(json::Parse(in, v));
        CHECK(json::Dump(v) == in);
        CHECK(v.Text("type") == "content_block_start");
        CHECK(v.Get("content_block") && v.Get("content_block")->Text("type") == "thinking");
        CHECK(v.Get("missing") == nullptr);
        CHECK(v.Get("a")->items.size() == 3);
    }
    // escapes: quotes, backslash, newline, control chars, \u, surrogate pairs
    {
        Value v;
        CHECK(json::Parse(R"({"t":"a\"b\\c\ndñ😀\/"})", v));
        CHECK(v.Text("t") == "a\"b\\c\nd\xC3\xB1\xF0\x9F\x98\x80/");
        std::string out = json::Dump(Value::Str(std::string("x\"\\\n\t\x01y\xC3\xB1")));
        CHECK(out == "\"x\\\"\\\\\\n\\t\\u0001y\xC3\xB1\"");
        Value back;
        CHECK(json::Parse(out, back) && back.s == std::string("x\"\\\n\t\x01y\xC3\xB1"));
    }
    // C code survives a round trip
    {
        std::string code = "int main() {\n\tprintf(\"%d\\n\", *p);\r\n}\n";
        Value v;
        CHECK(json::Parse(json::Dump(Value::Str(code)), v) && v.s == code);
    }
    // broken input is rejected, not half-read
    {
        Value v;
        CHECK(!json::Parse("", v));
        CHECK(!json::Parse("{", v));
        CHECK(!json::Parse(R"({"a":1,})", v));
        CHECK(!json::Parse(R"({"a" 1})", v));
        CHECK(!json::Parse(R"(["a",])", v));
        CHECK(!json::Parse(R"("abc)", v));
        CHECK(!json::Parse("\"a\nb\"", v));   // raw newline in a string
        CHECK(!json::Parse(R"({"a":1} x)", v));
        CHECK(!json::Parse("-", v));
        std::string deep(1000, '[');
        CHECK(!json::Parse(deep, v));
    }
    // building values
    {
        Value o = Value::Obj();
        o.Set("model", Value::Str("claude-opus-5-5"));
        o.Set("max_tokens", Value::Num(16000));
        o.Set("stream", Value::Boolean(true));
        Value& msgs = o.Set("messages", Value::Arr());
        Value m = Value::Obj();
        m.Set("role", Value::Str("user"));
        m.Set("content", Value::Str("hi"));
        msgs.Push(m);
        o.Set("max_tokens", Value::Num(8000));   // replaces, keeps its place
        CHECK(json::Dump(o) == R"({"model":"claude-opus-5-5","max_tokens":8000,"stream":true,"messages":[{"role":"user","content":"hi"}]})");
    }
    std::printf("json: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
