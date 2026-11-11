// ============================================================================
// ai_stream.h  -  Reads the Claude API's streamed answer (server-sent events)
// and rebuilds the reply's content blocks, so the text can be shown as it
// arrives and the whole reply can be sent back, unchanged, on the next turn.
// No raylib here, so ai_stream_tests can test it.
// ============================================================================
#pragma once
#include "json.h"
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace ai {

class StreamParser {
public:
    std::vector<json::Value> blocks;   // the reply's content blocks, as the API sent them
    std::string stopReason;            // "end_turn", "max_tokens", "refusal", ...
    std::string error;                 // an "error" event's message
    std::string model;                 // the model that answered
    bool finished = false;             // message_stop seen

    void Feed(const char* data, size_t n) {
        pending.append(data, n);
        for (;;) {   // events end with a blank line ("\n\n", or "\r\n\r\n")
            size_t a = pending.find("\n\n"), b = pending.find("\r\n\r\n");
            size_t end = std::min(a, b);
            if (end == std::string::npos) break;
            size_t skip = end == b && b <= a ? 4 : 2;
            std::string event = pending.substr(0, end);
            pending.erase(0, end + skip);
            Event(event);
        }
    }

    // All the text so far (what the student sees).
    std::string Text() const {
        std::string t;
        for (const json::Value& b : blocks)
            if (b.Text("type") == "text") t += b.Text("text");
        return t;
    }

    // The reply as it must go back in the next request. After a mid-answer
    // fallback, thinking (and tool use) before the last fallback block is left
    // out; the fallback block itself stays where it was.
    json::Value EchoContent() const {
        int lastFallback = -1;
        for (size_t i = 0; i < blocks.size(); i++)
            if (blocks[i].Text("type") == "fallback") lastFallback = (int)i;
        json::Value out = json::Value::Arr();
        for (size_t i = 0; i < blocks.size(); i++) {
            std::string t = blocks[i].Text("type");
            if ((int)i < lastFallback && (t == "thinking" || t == "redacted_thinking" || t == "tool_use")) continue;
            out.Push(blocks[i]);
        }
        return out;
    }

private:
    std::string pending;

    json::Value* Block(const json::Value& ev) {
        const json::Value* idx = ev.Get("index");
        if (!idx || idx->type != json::Value::Number) return nullptr;
        long i = std::strtol(idx->s.c_str(), nullptr, 10);
        if (i < 0 || i > 1000) return nullptr;
        if ((size_t)i >= blocks.size()) blocks.resize((size_t)i + 1, json::Value::Obj());
        return &blocks[(size_t)i];
    }

    void Event(const std::string& text) {
        // only the data: lines matter (the event: line repeats the type)
        std::string data;
        size_t pos = 0;
        while (pos <= text.size()) {
            size_t nl = text.find('\n', pos);
            std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("data:", 0) == 0) {
                std::string d = line.substr(5);
                if (!d.empty() && d[0] == ' ') d.erase(0, 1);
                if (!data.empty()) data += '\n';
                data += d;
            }
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
        json::Value ev;
        if (data.empty() || !json::Parse(data, ev)) return;
        const std::string type = ev.Text("type");
        if (type == "message_start") {
            if (const json::Value* m = ev.Get("message")) model = m->Text("model");
        } else if (type == "content_block_start") {
            json::Value* b = Block(ev);
            if (b) { const json::Value* cb = ev.Get("content_block"); if (cb) *b = *cb; }
        } else if (type == "content_block_delta") {
            json::Value* b = Block(ev);
            const json::Value* d = ev.Get("delta");
            if (!b || !d) return;
            const std::string dt = d->Text("type");
            auto append = [&](const char* key, const std::string& more) {
                json::Value* f = b->Get(key);
                if (f && f->type == json::Value::String) f->s += more;
                else b->Set(key, json::Value::Str(more));
            };
            if (dt == "text_delta") append("text", d->Text("text"));
            else if (dt == "thinking_delta") append("thinking", d->Text("thinking"));
            else if (dt == "signature_delta") b->Set("signature", json::Value::Str(d->Text("signature")));
        } else if (type == "message_delta") {
            if (const json::Value* d = ev.Get("delta")) {
                std::string s = d->Text("stop_reason");
                if (!s.empty()) stopReason = s;
            }
        } else if (type == "message_stop") {
            finished = true;
        } else if (type == "error") {
            const json::Value* e = ev.Get("error");
            error = e ? e->Text("message", "The server reported an error.") : "The server reported an error.";
        }
    }
};

// ---- Gemini (Google): streamGenerateContent?alt=sse ------------------------
// Each event is a whole GenerateContentResponse; its candidate's parts carry
// the next piece of text. The parts are kept (thought signatures included) to
// go back unchanged as the model's turn.
class GeminiParser {
public:
    std::vector<json::Value> parts;    // the model's turn, ready to send back
    std::string finishReason;          // "STOP", "MAX_TOKENS", "SAFETY", ...
    std::string blockReason;           // the question itself was blocked
    std::string error;
    bool finished = false;             // a finishReason arrived

    void Feed(const char* data, size_t n) {
        pending.append(data, n);
        for (;;) {
            size_t a = pending.find("\n\n"), b = pending.find("\r\n\r\n");
            size_t end = std::min(a, b);
            if (end == std::string::npos) break;
            size_t skip = end == b && b <= a ? 4 : 2;
            std::string event = pending.substr(0, end);
            pending.erase(0, end + skip);
            Event(event);
        }
    }

    std::string Text() const {
        std::string t;
        for (const json::Value& p : parts)
            if (!IsThought(p)) t += p.Text("text");
        return t;
    }

    bool Refused() const {
        return !blockReason.empty() || finishReason == "SAFETY" || finishReason == "PROHIBITED_CONTENT" ||
               finishReason == "BLOCKLIST" || finishReason == "SPII" || finishReason == "RECITATION";
    }

    json::Value EchoParts() const {
        json::Value out = json::Value::Arr();
        for (const json::Value& p : parts) out.Push(p);
        return out;
    }

private:
    std::string pending;

    static bool IsThought(const json::Value& p) {
        const json::Value* t = p.Get("thought");
        return t && t->type == json::Value::Bool && t->b;
    }

    void Event(const std::string& text) {
        std::string data;
        size_t pos = 0;
        while (pos <= text.size()) {
            size_t nl = text.find('\n', pos);
            std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("data:", 0) == 0) {
                std::string d = line.substr(5);
                if (!d.empty() && d[0] == ' ') d.erase(0, 1);
                if (!data.empty()) data += '\n';
                data += d;
            }
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
        json::Value ev;
        if (data.empty() || !json::Parse(data, ev)) return;
        if (const json::Value* e = ev.Get("error")) { error = e->Text("message", "The server reported an error."); return; }
        if (const json::Value* pf = ev.Get("promptFeedback")) {
            std::string br = pf->Text("blockReason");
            if (!br.empty()) { blockReason = br; finished = true; }
        }
        const json::Value* cands = ev.Get("candidates");
        if (!cands || cands->type != json::Value::Array || cands->items.empty()) return;
        const json::Value& c = cands->items[0];
        if (const json::Value* content = c.Get("content")) {
            if (const json::Value* ps = content->Get("parts")) {
                for (const json::Value& p : ps->items) {
                    // plain text joins the text part before it; anything else is kept as it came
                    bool plain = p.fields.size() == 1 && p.Get("text");
                    if (plain && !parts.empty()) {
                        json::Value& last = parts.back();
                        if (last.fields.size() == 1 && last.Get("text")) { last.Get("text")->s += p.Text("text"); continue; }
                    }
                    if (p.IsObject()) parts.push_back(p);
                }
            }
        }
        std::string fr = c.Text("finishReason");
        if (!fr.empty()) { finishReason = fr; finished = true; }
    }
};

} // namespace ai
