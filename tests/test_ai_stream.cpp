// ai_stream_tests: rebuilding a streamed Claude reply (src/app/ai_stream.h).
#include "../src/app/ai_stream.h"
#include <cstdio>
#include <cstring>

static int fails = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; std::printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static const char* kStream =
    "event: message_start\n"
    "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_1\",\"type\":\"message\",\"model\":\"claude-opus-5-5\",\"content\":[]}}\n\n"
    "event: content_block_start\n"
    "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"\"}}\n\n"
    "event: content_block_delta\n"
    "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"signature_delta\",\"signature\":\"SIG123\"}}\n\n"
    "event: content_block_stop\n"
    "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
    "event: ping\n"
    "data: {\"type\":\"ping\"}\n\n"
    "event: content_block_start\n"
    "data: {\"type\":\"content_block_start\",\"index\":1,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
    "event: content_block_delta\n"
    "data: {\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"text_delta\",\"text\":\"Line 3: `p` is \"}}\n\n"
    "event: content_block_delta\n"
    "data: {\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"text_delta\",\"text\":\"NULL.\\n```c\\n*p = 1;\\n```\"}}\n\n"
    "event: content_block_stop\n"
    "data: {\"type\":\"content_block_stop\",\"index\":1}\n\n"
    "event: message_delta\n"
    "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":12}}\n\n"
    "event: message_stop\n"
    "data: {\"type\":\"message_stop\"}\n\n";

int main() {
    // fed in one go, and fed one byte at a time, gives the same reply
    for (int mode = 0; mode < 3; mode++) {
        ai::StreamParser p;
        std::string s = kStream;
        if (mode == 2) {   // CRLF line ends
            std::string c;
            for (char ch : s) { if (ch == '\n') c += '\r'; c += ch; }
            s = c;
        }
        if (mode == 1) for (char ch : s) p.Feed(&ch, 1);
        else p.Feed(s.data(), s.size());
        CHECK(p.finished);
        CHECK(p.stopReason == "end_turn");
        CHECK(p.model == "claude-opus-5-5");
        CHECK(p.error.empty());
        CHECK(p.Text() == "Line 3: `p` is NULL.\n```c\n*p = 1;\n```");
        CHECK(p.blocks.size() == 2);
        // the thinking block goes back exactly as the API sent it
        json::Value echo = p.EchoContent();
        CHECK(echo.items.size() == 2);
        CHECK(json::Dump(echo.items[0]) == "{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"SIG123\"}");
        CHECK(json::Dump(echo.items[1]) == "{\"type\":\"text\",\"text\":\"Line 3: `p` is NULL.\\n```c\\n*p = 1;\\n```\"}");
    }
    // a fallback in the middle: thinking before the last fallback block is not sent back
    {
        ai::StreamParser p;
        const char* s =
            "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"A\"}}\n\n"
            "data: {\"type\":\"content_block_start\",\"index\":1,\"content_block\":{\"type\":\"text\",\"text\":\"Partial \"}}\n\n"
            "data: {\"type\":\"content_block_start\",\"index\":2,\"content_block\":{\"type\":\"fallback\",\"from\":{\"model\":\"claude-opus-5-5\"},\"to\":{\"model\":\"claude-opus-5\"}}}\n\n"
            "data: {\"type\":\"content_block_start\",\"index\":3,\"content_block\":{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"B\"}}\n\n"
            "data: {\"type\":\"content_block_start\",\"index\":4,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
            "data: {\"type\":\"content_block_delta\",\"index\":4,\"delta\":{\"type\":\"text_delta\",\"text\":\"rest.\"}}\n\n"
            "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"}}\n\n";
        p.Feed(s, std::strlen(s));
        CHECK(p.Text() == "Partial rest.");
        json::Value echo = p.EchoContent();
        CHECK(echo.items.size() == 4);
        CHECK(echo.items[0].Text("type") == "text");
        CHECK(echo.items[1].Text("type") == "fallback");
        CHECK(echo.items[2].Text("signature") == "B");
    }
    // errors and refusals
    {
        ai::StreamParser p;
        const char* s = "event: error\ndata: {\"type\":\"error\",\"error\":{\"type\":\"overloaded_error\",\"message\":\"Overloaded\"}}\n\n";
        p.Feed(s, std::strlen(s));
        CHECK(p.error == "Overloaded");
        ai::StreamParser q;
        const char* r = "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"refusal\"}}\n\ndata: {\"type\":\"message_stop\"}\n\n";
        q.Feed(r, std::strlen(r));
        CHECK(q.stopReason == "refusal" && q.finished && q.Text().empty());
    }
    // junk doesn't crash and doesn't add blocks
    {
        ai::StreamParser p;
        const char* s = "data: {not json}\n\ndata: {\"type\":\"content_block_delta\",\"index\":99999,\"delta\":{\"type\":\"text_delta\",\"text\":\"x\"}}\n\n: comment\n\n";
        p.Feed(s, std::strlen(s));
        CHECK(p.blocks.empty());
    }
    // Gemini: text arrives in pieces, signatures are kept, the finish reason ends it
    {
        ai::GeminiParser g;
        const char* s =
            "data: {\"candidates\": [{\"content\": {\"parts\": [{\"text\": \"Line 3 \"}],\"role\": \"model\"},\"index\": 0}]}\r\n\r\n"
            "data: {\"candidates\": [{\"content\": {\"parts\": [{\"text\": \"writes through NULL.\"}],\"role\": \"model\"},\"index\": 0}]}\r\n\r\n"
            "data: {\"candidates\": [{\"content\": {\"parts\": [{\"text\": \"\", \"thoughtSignature\": \"SIGX\"}],\"role\": \"model\"},\"finishReason\": \"STOP\",\"index\": 0}],\"usageMetadata\": {\"promptTokenCount\": 10}}\r\n\r\n";
        for (const char* c = s; *c; c++) g.Feed(c, 1);
        CHECK(g.finished && g.finishReason == "STOP" && !g.Refused());
        CHECK(g.Text() == "Line 3 writes through NULL.");
        json::Value echo = g.EchoParts();
        CHECK(echo.items.size() == 2);
        CHECK(echo.items[0].Text("text") == "Line 3 writes through NULL.");
        CHECK(echo.items[1].Text("thoughtSignature") == "SIGX");
    }
    {
        ai::GeminiParser g;
        const char* s = "data: {\"promptFeedback\": {\"blockReason\": \"SAFETY\"}}\n\n";
        g.Feed(s, std::strlen(s));
        CHECK(g.Refused() && g.finished && g.Text().empty());
        ai::GeminiParser h;
        const char* e = "data: {\"error\": {\"code\": 429, \"message\": \"Quota exceeded\", \"status\": \"RESOURCE_EXHAUSTED\"}}\n\n";
        h.Feed(e, std::strlen(e));
        CHECK(h.error == "Quota exceeded");
        ai::GeminiParser k;
        const char* th = "data: {\"candidates\": [{\"content\": {\"parts\": [{\"text\": \"thinking...\", \"thought\": true}, {\"text\": \"Answer\"}]}, \"finishReason\": \"MAX_TOKENS\"}]}\n\n";
        k.Feed(th, std::strlen(th));
        CHECK(k.Text() == "Answer" && k.finishReason == "MAX_TOKENS");
    }
    std::printf("ai_stream: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
