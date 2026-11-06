// ============================================================================
// listlab.cpp  -  Linked List Lab. Every button calls the REAL LinkedList /
// DoublyLinkedList from src/solutions, then replays the operation as an
// animation: the temp pointer walking, each link changing, the matching code
// line highlighted. Node addresses shown are the real heap addresses.
// ============================================================================
#include "app/app.h"
#include "app/ui.h"
#include "content/content.h"
#include "linked_list.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>

using namespace ui;

namespace {

const int kMaxNodes = 6;

struct SNode { const void* ptr; int value; };          // snapshot of the real list

struct FNode {
    int id = 0;
    int value = 0;
    std::string addr;
    float x = 0, y = 0;
    int next = -1, prev = -1;      // indexes into AnimFrame::nodes, -1 = nullptr
    int tint = 0;                  // 0 normal, 1 new, 2 deleting, 3 found, 4 visited
    float alpha = 1;
};

struct Cursor { int at; std::string label; };          // at: node index, -1 = before, n = after (nullptr)

struct AnimFrame {
    std::vector<FNode> nodes;
    int head = -1, tail = -1;
    std::vector<Cursor> cursors;
    std::string note;
    std::string snippet;
    std::string lineText;          // code line to highlight (substring)
};

std::string Hex(const void* p) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%04x", (unsigned)((uintptr_t)p & 0xFFFF));
    return buf;
}

class ListLabScene : public Scene {
public:
    const char* Title() const override { return "Linked List Lab"; }

    void Enter(const std::string& param) override {
        if (param == "doubly") doubly = true;
        else if (param == "singly") doubly = false;
        if (!seeded) { Seed(); seeded = true; }
        ShowStatic("Pick an operation. Every button runs the real C++ code, then replays it step by step.");
    }

    void Frame(float dt) override {
        Animate(dt);
        Toolbar();
        Canvas();
        CodePanel();
        InfoPanel();
        if (!AnyTextFocused()) {
            if (IsKeyPressed(KEY_SPACE)) playing = !playing;
            if (IsKeyPressed(KEY_RIGHT)) StepForward();
        }
    }

private:
    LinkedList slist;
    DoublyLinkedList dlist;
    bool doubly = false, seeded = false;
    std::map<const void*, int> ids;
    int nextId = 1;
    std::string valueText = "42", posText = "1";
    std::vector<AnimFrame> frames;
    int fi = 0;
    AnimFrame prevShown;
    float animT = 1, holdT = 0;
    bool playing = true;
    float speed = 1;
    std::string console;
    Scroll codeScroll;
    int lastOpSize = 0;

    // ------------------------------------------------------------ model --
    void Seed() {
        int vals[3] = {5, 10, 20};
        for (int v : vals) { slist.InsertAtEnd(v); dlist.InsertAtEnd(v); }
    }

    std::vector<SNode> Snap() {
        std::vector<SNode> s;
        if (doubly) for (auto* n = dlist.GetHead(); n; n = n->nextPointer) s.push_back({n, n->data});
        else for (auto* n = slist.GetHead(); n; n = n->next) s.push_back({n, n->data});
        return s;
    }

    int Size() { return doubly ? dlist.Size() : slist.Size(); }

    int IdOf(const void* p) {
        auto it = ids.find(p);
        if (it != ids.end()) return it->second;
        ids[p] = nextId;
        return nextId++;
    }

    void ForgetMissing(const std::vector<SNode>& now) {
        std::set<const void*> alive;
        for (auto& n : now) alive.insert(n.ptr);
        for (auto it = ids.begin(); it != ids.end();) it = alive.count(it->first) ? std::next(it) : ids.erase(it);
    }

    void UpdateConsole() {
        CaptureOutput cap;
        if (doubly) dlist.DisplayForward(); else slist.Display();
        console = app.consoleCapture;
        if (!console.empty() && console.back() == '\n') console.pop_back();
    }

    // ----------------------------------------------------------- layout --
    float NodeW() const { return doubly ? 160.0f : 128.0f; }
    static constexpr float NodeH = 56;
    static constexpr float BaseY = 382;

    std::vector<float> Layout(int n) const {
        std::vector<float> xs;
        float area = 1160, w = NodeW();
        float gap = n > 1 ? std::min(96.0f, (area - n * w) / (n - 1)) : 0;
        float total = n * w + std::max(0, n - 1) * gap;
        float start = 60 + (area - total) / 2;
        for (int i = 0; i < n; i++) xs.push_back(start + i * (w + gap));
        return xs;
    }
    float Step(int n) const { auto xs = Layout(std::max(2, n)); return xs[1] - xs[0]; }

    AnimFrame Base(const std::vector<SNode>& s) {
        AnimFrame f;
        auto xs = Layout((int)s.size());
        for (size_t i = 0; i < s.size(); i++) {
            FNode n;
            n.id = IdOf(s[i].ptr); n.value = s[i].value; n.addr = Hex(s[i].ptr);
            n.x = xs[i]; n.y = BaseY;
            n.next = i + 1 < s.size() ? (int)i + 1 : -1;
            n.prev = doubly && i > 0 ? (int)i - 1 : -1;
            f.nodes.push_back(n);
        }
        f.head = s.empty() ? -1 : 0;
        f.tail = doubly && !s.empty() ? (int)s.size() - 1 : -1;
        return f;
    }

    int AddPending(AnimFrame& f, int id, int value, const std::string& addr, int pos) {
        int n = (int)f.nodes.size();
        float w = NodeW();
        float x;
        if (n == 0) x = 60 + (1160 - w) / 2;
        else if (pos <= 0) x = f.nodes[0].x - Step(n) * 0.55f;
        else if (pos >= n) x = f.nodes[n - 1].x + Step(n) * 0.55f;
        else x = (f.nodes[pos - 1].x + f.nodes[pos].x) / 2;
        x = std::clamp(x, 40.0f, 1240 - w);
        FNode p;
        p.id = id; p.value = value; p.addr = addr; p.x = x; p.y = BaseY - 124; p.tint = 1;
        f.nodes.push_back(p);
        return n;
    }

    // ------------------------------------------------------- operations --
    std::string Fn(const char* name) const { return std::string(doubly ? "DoublyLinkedList::" : "LinkedList::") + name; }

    AnimFrame& Push(AnimFrame f, const std::string& note, const std::string& snippet, const std::string& line) {
        f.note = note; f.snippet = snippet; f.lineText = line;
        frames.push_back(f);
        return frames.back();
    }

    void Start() {
        prevShown = Current();
        fi = 0; animT = 0; holdT = 0;
        codeScroll.Reset();
    }

    AnimFrame Current() const {
        if (frames.empty()) return AnimFrame();
        return frames[std::min(fi, (int)frames.size() - 1)];
    }

    void ShowStatic(const std::string& note) {
        AnimFrame f = Base(Snap());
        frames.clear();
        Push(f, note, Fn(doubly ? "InsertAtPosition" : "InsertAtPosition"), "");
        prevShown = frames[0];
        fi = 0; animT = 1;
        UpdateConsole();
    }

    void Final(const std::string& note) {
        auto post = Snap();
        AnimFrame f = Base(post);
        Push(f, note, frames.empty() ? "" : frames.back().snippet, "");
        ForgetMissing(post);
        UpdateConsole();
    }

    bool ReadValue(int& v) {
        try { size_t used; v = std::stoi(valueText, &used); if (used != valueText.size()) throw 1; }
        catch (...) { Toast("Enter a whole number in 'value'", col::danger); return false; }
        if (v < -999 || v > 9999) { Toast("Keep values between -999 and 9999", col::danger); return false; }
        return true;
    }
    bool ReadPos(int& p) {
        try { size_t used; p = std::stoi(posText, &used); if (used != posText.size()) throw 1; }
        catch (...) { Toast("Enter a position (0 = first)", col::danger); return false; }
        return true;
    }

    void InsertBeginningFrames(const std::vector<SNode>& pre, int id, int v, const std::string& addr, const std::string& sn) {
        AnimFrame f = Base(pre);
        int pi = AddPending(f, id, v, addr, 0);
        Push(f, "A new node is created on the HEAP with `new`. Its next pointer starts as nullptr.", sn, "new ");
        if (pre.empty()) {
            f.head = pi; if (doubly) f.tail = pi;
            Push(f, "The list was empty, so the new node becomes the first node" + std::string(doubly ? " AND the last (head and tail)." : "."),
                 sn, doubly ? "head = tail = newNode" : "newNode->next = head");
            return;
        }
        f.nodes[pi].next = 0;
        Push(f, "First link the new node to the current first node. Nothing is lost yet.", sn,
             doubly ? "newNode->nextPointer = head" : "newNode->next = head");
        if (doubly) {
            f.nodes[0].prev = pi;
            Push(f, "The old first node now points back at the new node.", sn, "head->prevPointer = newNode");
        }
        f.head = pi;
        Push(f, "Only now move head. (Doing this first would lose the whole list!)", sn, "head = newNode");
    }

    void InsertEndFrames(const std::vector<SNode>& pre, int id, int v, const std::string& addr, const std::string& sn) {
        AnimFrame f = Base(pre);
        int n = (int)pre.size();
        int pi = AddPending(f, id, v, addr, n);
        Push(f, "Create the new node on the heap. It will be the last node, so its next stays nullptr.", sn, "new ");
        if (n == 0) {
            f.head = pi; if (doubly) f.tail = pi;
            Push(f, "Empty list: the new node is the whole list.", sn, doubly ? "head = tail = newNode" : "head = newNode;");
            return;
        }
        if (doubly) {
            f.nodes[pi].prev = n - 1;
            Push(f, "We keep a tail pointer, so no walking is needed: link back to the old tail.", sn, "newNode->prevPointer = tail");
            f.nodes[n - 1].next = pi;
            Push(f, "The old tail links forward to the new node.", sn, "tail->nextPointer = newNode");
            f.tail = pi;
            Push(f, "Move tail. InsertAtEnd is O(1) in a doubly linked list with a tail.", sn, "tail = newNode");
            return;
        }
        f.cursors = {{0, "temp"}};
        Push(f, "Start walking from head.", sn, "Node* temp = head;");
        for (int k = 1; k < n; k++) {
            f.cursors = {{k, "temp"}};
            f.nodes[k - 1].tint = 4;
            Push(f, "temp->next is not nullptr, keep walking.", sn, "temp = temp->next;");
        }
        f.nodes[n - 1].next = pi;
        Push(f, "temp is the last node (its next was nullptr). Link it to the new node. Walking made this O(n).", sn, "temp->next = newNode;");
    }

    void DoInsert(int kind) {   // 0 beginning, 1 end, 2 position
        int v, p = 0;
        if (!ReadValue(v)) return;
        if (Size() >= kMaxNodes) { Toast("The lab shows up to 6 nodes. Delete one first.", col::warn); return; }
        auto pre = Snap();
        int n = (int)pre.size();
        if (kind == 2) {
            if (!ReadPos(p)) return;
            if (p < 0 || p > n) {
                bool ok = doubly ? dlist.InsertAtPosition(v, p) : slist.InsertAtPosition(v, p);
                Toast("InsertAtPosition returned " + std::string(ok ? "true" : "false") + ": position must be 0.." + std::to_string(n), col::danger);
                return;
            }
        }
        // 1) run the real code
        if (kind == 0) { if (doubly) dlist.InsertAtBeginning(v); else slist.InsertAtBeginning(v); }
        else if (kind == 1) { if (doubly) dlist.InsertAtEnd(v); else slist.InsertAtEnd(v); }
        else { if (doubly) dlist.InsertAtPosition(v, p); else slist.InsertAtPosition(v, p); }
        auto post = Snap();
        std::set<const void*> old;
        for (auto& s : pre) old.insert(s.ptr);
        const void* fresh = nullptr;
        for (auto& s : post) if (!old.count(s.ptr)) fresh = s.ptr;
        int id = IdOf(fresh);
        std::string addr = Hex(fresh);
        // 2) build the replay
        frames.clear();
        if (kind == 0 || (kind == 2 && p == 0)) InsertBeginningFrames(pre, id, v, addr, Fn("InsertAtBeginning"));
        else if (kind == 1 || (kind == 2 && p == n && doubly)) InsertEndFrames(pre, id, v, addr, Fn("InsertAtEnd"));
        else {
            std::string sn = Fn("InsertAtPosition");
            AnimFrame f = Base(pre);
            int pi = AddPending(f, id, v, addr, p);
            Push(f, "Create the new node on the heap.", sn, "new ");
            f.cursors = {{0, "temp"}};
            Push(f, "temp starts at head. We need the node BEFORE position " + std::to_string(p) + ".", sn,
                 doubly ? "DoublyLinkedNode* temp = head;" : "Node* temp = head;");
            for (int k = 1; k <= p - 1; k++) {
                f.nodes[k - 1].tint = 4;
                f.cursors = {{k, "temp"}};
                Push(f, "Move temp forward.", sn, doubly ? "temp = temp->nextPointer;" : "temp = temp->next;");
            }
            int after = p < n ? p : -1;
            f.nodes[pi].next = after;
            Push(f, "1) The new node points at the rest of the list first.", sn,
                 doubly ? "newNode->nextPointer = temp->nextPointer;" : "newNode->next = temp->next;");
            if (doubly) {
                f.nodes[pi].prev = p - 1;
                Push(f, "2) It points back at temp.", sn, "newNode->prevPointer = temp;");
                if (after >= 0) f.nodes[after].prev = pi;
                Push(f, "3) The next node points back at the new node.", sn, "temp->nextPointer->prevPointer = newNode;");
            }
            f.nodes[p - 1].next = pi;
            Push(f, std::string(doubly ? "4)" : "2)") + " Finally temp links to the new node. This line must come last.", sn,
                 doubly ? "temp->nextPointer = newNode;" : "temp->next = newNode;");
        }
        Final("Done. The list now has " + std::to_string(post.size()) + " nodes. The real list was updated by the solution code.");
        Start();
    }

    void DoDelete() {
        int p;
        if (!ReadPos(p)) return;
        auto pre = Snap();
        int n = (int)pre.size();
        if (p < 0 || p >= n) {
            Toast("DeleteAtPosition returned false: position must be 0.." + std::to_string(std::max(0, n - 1)), col::danger);
            if (doubly) dlist.DeleteAtPosition(p); else slist.DeleteAtPosition(p);
            return;
        }
        frames.clear();
        std::string sn = Fn("DeleteAtPosition");
        AnimFrame f = Base(pre);
        if (!doubly && p == 0) {
            f.cursors = {{0, "toDelete"}}; f.nodes[0].tint = 2;
            Push(f, "Remember the first node so we can free it.", sn, "Node* toDelete = head;");
            f.head = n > 1 ? 1 : -1;
            Push(f, "Move head to the second node.", sn, "head = head->next;");
        } else if (!doubly) {
            f.cursors = {{0, "temp"}};
            Push(f, "Walk temp to the node BEFORE position " + std::to_string(p) + ".", sn, "Node* temp = head;");
            for (int k = 1; k <= p - 1; k++) { f.nodes[k - 1].tint = 4; f.cursors = {{k, "temp"}}; Push(f, "Move temp forward.", sn, "temp = temp->next;"); }
            f.cursors.push_back({p, "toDelete"}); f.nodes[p].tint = 2;
            Push(f, "toDelete is the node after temp.", sn, "Node* toDelete = temp->next;");
            f.nodes[p - 1].next = p + 1 < n ? p + 1 : -1;
            Push(f, "Bypass it: temp now points past it.", sn, "temp->next = toDelete->next;");
        } else {
            f.cursors = {{0, "toDelete"}};
            Push(f, "Walk to the node at position " + std::to_string(p) + ".", sn, "DoublyLinkedNode* toDelete = head;");
            for (int k = 1; k <= p; k++) { f.nodes[k - 1].tint = 4; f.cursors = {{k, "toDelete"}}; Push(f, "Move forward.", sn, "toDelete = toDelete->nextPointer;"); }
            f.nodes[p].tint = 2;
            if (p > 0) { f.nodes[p - 1].next = p + 1 < n ? p + 1 : -1; Push(f, "The previous node skips over it.", sn, "toDelete->prevPointer->nextPointer"); }
            else { f.head = p + 1 < n ? p + 1 : -1; Push(f, "It was the first node: head moves forward.", sn, "else head = toDelete->nextPointer;"); }
            if (p + 1 < n) { f.nodes[p + 1].prev = p > 0 ? p - 1 : -1; Push(f, "The next node points back past it.", sn, "toDelete->nextPointer->prevPointer"); }
            else { f.tail = p > 0 ? p - 1 : -1; Push(f, "It was the last node: tail moves back.", sn, "else tail = toDelete->prevPointer;"); }
        }
        f.nodes[p].alpha = 0.18f;
        f.nodes[p].next = f.nodes[p].prev = -1;
        Push(f, "delete frees the node's memory. Without this line, the node would leak.", sn, "delete toDelete;");
        if (doubly) dlist.DeleteAtPosition(p); else slist.DeleteAtPosition(p);
        Final("Done. " + std::to_string(n - 1) + " nodes left.");
        Start();
    }

    void DoSearch() {
        int v;
        if (!ReadValue(v)) return;
        auto pre = Snap();
        int n = (int)pre.size();
        int found = doubly ? dlist.Search(v) : slist.Search(v);
        frames.clear();
        std::string sn = Fn("Search");
        AnimFrame f = Base(pre);
        f.cursors = {{0, "temp"}};
        Push(f, "Start at head with index 0.", sn, "* temp = head;");
        for (int k = 0; k < n; k++) {
            f.cursors = {{k, "temp"}};
            if (pre[k].value == v) {
                f.nodes[k].tint = 3;
                Push(f, "temp->data == " + std::to_string(v) + ": found at index " + std::to_string(k) + ".", sn, "if (temp->data == value) return index;");
                break;
            }
            f.nodes[k].tint = 4;
            Push(f, "temp->data is " + std::to_string(pre[k].value) + ", not " + std::to_string(v) + ". Move on.", sn,
                 doubly ? "temp = temp->nextPointer;" : "temp = temp->next;");
        }
        if (found < 0) {
            f.cursors = {{n, "temp"}};
            Push(f, "temp reached nullptr: the value is not in the list. Search returns -1.", sn, "return -1;");
        }
        Final(found >= 0 ? "Search returned " + std::to_string(found) + "." : "Search returned -1.");
        Start();
    }

    void DoReverse() {
        auto pre = Snap();
        int n = (int)pre.size();
        slist.Reverse();
        frames.clear();
        std::string sn = Fn("Reverse");
        AnimFrame f = Base(pre);
        f.cursors = {{-1, "prev"}, {0, "current"}};
        Push(f, "Three pointers: prev (nullptr), current (head) and nextNode.", sn, "Node* prev = nullptr;");
        for (int i = 0; i < n; i++) {
            f.cursors = {{i - 1, "prev"}, {i, "current"}, {i + 1, "nextNode"}};
            Push(f, "Save the rest of the list before breaking the link.", sn, "Node* nextNode = current->next;");
            f.nodes[i].next = i - 1;
            f.nodes[i].tint = 4;
            Push(f, "Flip the arrow: current now points backwards.", sn, "current->next = prev;");
        }
        f.cursors = {{n - 1, "prev"}, {n, "current"}};
        f.head = n - 1;
        Push(f, "current is nullptr: stop. The old last node is the new head.", sn, "head = prev;");
        Final("Reversed in place: no new nodes, only the arrows changed.");
        Start();
    }

    void DoRandom() {
        if (doubly) dlist.Clear(); else slist.Clear();
        int n = GetRandomValue(3, 5);
        for (int i = 0; i < n; i++) {
            int v = GetRandomValue(1, 99);
            if (doubly) dlist.InsertAtEnd(v); else slist.InsertAtEnd(v);
        }
        ids.clear();
        ShowStatic("A new random list. Notice the real addresses: nodes are NOT next to each other in memory.");
    }

    void DoClear() {
        if (doubly) dlist.Clear(); else slist.Clear();
        ids.clear();
        ShowStatic("Clear() deleted every node (saving next before each delete) and set head to nullptr.");
    }

    // ------------------------------------------------------------- anim --
    void StepForward() {
        if (fi + 1 < (int)frames.size()) { prevShown = Interpolated(); fi++; animT = 0; holdT = 0; }
    }

    void Animate(float dt) {
        animT = std::min(1.0f, animT + dt * 3.2f * std::max(1.0f, speed));
        if (playing && animT >= 1) {
            holdT += dt * speed;
            if (holdT > 1.25f) StepForward();
        }
    }

    static float Ease(float t) { return t < 0.5f ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2.0f) / 2; }

    AnimFrame Interpolated() const {
        AnimFrame cur = Current();
        float e = Ease(animT);
        for (FNode& n : cur.nodes) {
            for (const FNode& o : prevShown.nodes) if (o.id == n.id) {
                n.x = o.x + (n.x - o.x) * e;
                n.y = o.y + (n.y - o.y) * e;
                n.alpha = o.alpha + (n.alpha - o.alpha) * e;
            }
        }
        return cur;
    }

    // ------------------------------------------------------------- draw --
    void Toolbar() {
        Rectangle r = {24, 80, 1232, 104};
        Panel(r, col::panel, 14);
        if (Tab({r.x + 14, r.y + 10, 110, 38}, "Singly", !doubly) && doubly) { doubly = false; ids.clear(); ShowStatic("Singly linked list: each node has data and next."); }
        if (Tab({r.x + 128, r.y + 10, 110, 38}, "Doubly", doubly) && !doubly) { doubly = true; ids.clear(); ShowStatic("Doubly linked list: prevPointer AND nextPointer, plus a tail."); }
        float x = r.x + 266;
        Text("value", x, r.y + 20, 13, col::dim, F_BOLD);
        TextBox({x + 46, r.y + 10, 84, 38}, valueText, 201, 5, true);
        x += 150;
        Text("position", x, r.y + 20, 13, col::dim, F_BOLD);
        TextBox({x + 62, r.y + 10, 64, 38}, posText, 202, 3, true);
        // playback
        float px = r.x + r.width - 430;
        Text("speed", px, r.y + 20, 13, col::dim, F_BOLD);
        const float speeds[3] = {0.5f, 1, 2};
        const char* sl[3] = {"0.5x", "1x", "2x"};
        for (int i = 0; i < 3; i++)
            if (Tab({px + 50 + i * 58.0f, r.y + 10, 54, 38}, sl[i], speed == speeds[i])) speed = speeds[i];
        if (Button({px + 232, r.y + 10, 90, 38}, playing ? "Pause" : "Play", BTN_GHOST, true, 14)) playing = !playing;
        if (Button({px + 330, r.y + 10, 86, 38}, "Step >", BTN_GHOST, fi + 1 < (int)frames.size(), 14)) { playing = false; StepForward(); }

        // operations
        float bx = r.x + 14, by = r.y + 56;
        struct Op { const char* label; int w; };
        const Op ops[8] = {{"Insert at beginning", 168}, {"Insert at end", 124}, {"Insert at position", 160},
                           {"Delete at position", 160}, {"Search value", 124}, {"Reverse", 96}, {"Random", 90}, {"Clear", 80}};
        for (int i = 0; i < 8; i++) {
            bool enabled = !(i == 5 && doubly);
            ButtonKind k = i < 3 ? BTN_PRIMARY : i == 3 ? BTN_DANGER : BTN_DEFAULT;
            if (Button({bx, by, (float)ops[i].w, 38}, ops[i].label, k, enabled, 14)) {
                ClearTextFocus();
                switch (i) {
                    case 0: DoInsert(0); break; case 1: DoInsert(1); break; case 2: DoInsert(2); break;
                    case 3: DoDelete(); break; case 4: DoSearch(); break; case 5: DoReverse(); break;
                    case 6: DoRandom(); break; default: DoClear(); break;
                }
            }
            bx += ops[i].w + 10;
        }
    }

    Rectangle Box(const FNode& n) const { return {n.x, n.y - NodeH / 2, NodeW(), NodeH}; }

    void Canvas() {
        Rectangle r = {24, 196, 1232, 312};
        Panel(r, col::bg2, 14);
        Text(doubly ? "HEAP  -  DoublyLinkedList" : "HEAP  -  LinkedList", r.x + 18, r.y + 14, 12, col::heap, F_BOLD);
        AnimFrame f = Interpolated();
        BeginClip(r);
        // links first
        for (size_t i = 0; i < f.nodes.size(); i++) {
            const FNode& n = f.nodes[i];
            Rectangle b = Box(n);
            float a = n.alpha;
            if (n.next >= 0 && n.next < (int)f.nodes.size()) {
                Rectangle t = Box(f.nodes[n.next]);
                float yOff = doubly ? -NodeH * 0.2f : 0;
                Vector2 from = {b.x + b.width - (doubly ? 29 : 34), b.y + b.height / 2 + yOff};
                Link(f, (int)i, n.next, from, t, yOff, Alpha(col::pointer, a));
            }
            if (doubly && n.prev >= 0 && n.prev < (int)f.nodes.size()) {
                Rectangle t = Box(f.nodes[n.prev]);
                Vector2 from = {b.x + 29, b.y + b.height / 2 + NodeH * 0.2f};
                Vector2 to = {t.x + t.width + 2, t.y + t.height / 2 + NodeH * 0.2f};
                if (to.x < from.x - 10 && std::fabs(to.y - from.y) < 4) Arrow(from, to, Alpha(col::accent2, a), 2, 9);
                else CurvedArrow(from, {(from.x + to.x) / 2, std::max(from.y, to.y) + 60}, {t.x + t.width * 0.75f, t.y + t.height + 2}, Alpha(col::accent2, a), 2, 9);
            }
        }
        // nodes
        for (const FNode& n : f.nodes) DrawNode(f, n);
        // head / tail
        Label(f, f.head, "head", true);
        if (doubly) Label(f, f.tail, "tail", false);
        // cursors
        std::map<int, int> stackCount;
        for (const Cursor& c : f.cursors) {
            int count = (int)f.nodes.size();
            float x, y = BaseY + NodeH / 2 + 34;
            if (c.at >= 0 && c.at < count) x = f.nodes[c.at].x + NodeW() / 2;
            else if (c.at < 0) x = (count ? f.nodes[0].x : 640) - 40;
            else x = (count ? f.nodes[count - 1].x + NodeW() : 640) + 50;
            int k = stackCount[c.at]++;
            y += k * 30;
            bool isNull = c.at < 0 || c.at >= count;
            std::string label = c.label + (isNull ? " = nullptr" : "");
            float w = Measure(label, 13, F_MONOBOLD).x + 18;
            Rectangle lb = {x - w / 2, y + 10, w, 24};
            RoundRect(lb, 6, Alpha(col::warn, 0.16f));
            RoundRectLines(lb, 6, 1, col::warn);
            TextCenter(label, lb, 13, col::warn, F_MONOBOLD);
            if (!isNull && k == 0) Arrow({x, y + 10}, {x, BaseY + NodeH / 2 + 16}, col::warn, 2, 8);
        }
        if (f.nodes.empty()) TextCenter("empty list  -  head = nullptr", {r.x, r.y + 120, r.width, 40}, 17, col::faint, F_MONO);
        EndClip();
    }

    void Link(const AnimFrame& f, int src, int dst, Vector2 from, Rectangle t, float yOff, Color c) {
        Vector2 to = {t.x - 2, t.y + t.height / 2 + yOff};
        float dx = to.x - from.x, dy = to.y - from.y;
        bool blocked = false;     // is another node sitting between the two?
        for (int k = 0; k < (int)f.nodes.size(); k++) {
            if (k == src || k == dst) continue;
            const FNode& o = f.nodes[k];
            if (std::fabs(o.y - f.nodes[dst].y) < 10 && o.x > from.x && o.x < to.x) blocked = true;
        }
        if (dx > 0 && std::fabs(dy) < 4 && !blocked) {
            Arrow(from, to, c, 2.2f, 10);
        } else if (dx < -10 && std::fabs(dy) < 80) {
            // pointing backwards (reverse): curve underneath
            Vector2 end = {t.x + t.width * 0.5f, t.y + t.height + 2};
            CurvedArrow(from, {(from.x + end.x) / 2, std::max(from.y, end.y) + 70}, end, c, 2.2f, 10);
        } else if (std::fabs(dy) < 4) {
            // skipping over a node: arc above
            CurvedArrow(from, {(from.x + to.x) / 2, from.y - 120}, {t.x + 16, t.y - 2}, c, 2.2f, 10);
        } else {
            CurvedArrow(from, {from.x + dx * 0.5f, from.y + dy * 0.1f}, to, c, 2.2f, 10);
        }
    }

    void DrawNode(const AnimFrame& f, const FNode& n) {
        Rectangle b = Box(n);
        float a = n.alpha;
        Color border = n.tint == 1 ? col::success : n.tint == 2 ? col::danger : n.tint == 3 ? col::accent2 : col::border;
        Color fill = n.tint == 4 ? Mix(col::panelHi, col::warn, 0.08f) : col::panelHi;
        RoundRect(b, 8, Alpha(fill, a));
        RoundRectLines(b, 8, n.tint ? 2.0f : 1.0f, Alpha(border, a));
        auto addrOf = [&](int idx) { return idx >= 0 && idx < (int)f.nodes.size() ? f.nodes[idx].addr : std::string("nullptr"); };
        if (!doubly) {
            Rectangle data = {b.x, b.y, 58, b.height}, next = {b.x + 58, b.y, b.width - 58, b.height};
            DrawRectangleRec({next.x, b.y + 6, 1, b.height - 12}, Alpha(col::border, a));
            TextCenter(std::to_string(n.value), data, 20, Alpha(col::heading, a), F_MONOBOLD);
            Text("next", next.x + 8, b.y + 6, 10.5f, Alpha(col::dim, a), F_MONO);
            Text(addrOf(n.next), next.x + 8, b.y + 26, 12, Alpha(n.next >= 0 ? col::pointer : col::dim, a), F_MONO);
        } else {
            Rectangle prev = {b.x, b.y, 58, b.height}, data = {b.x + 58, b.y, 44, b.height}, next = {b.x + 102, b.y, 58, b.height};
            DrawRectangleRec({data.x, b.y + 6, 1, b.height - 12}, Alpha(col::border, a));
            DrawRectangleRec({next.x, b.y + 6, 1, b.height - 12}, Alpha(col::border, a));
            TextCenter(std::to_string(n.value), data, 17, Alpha(col::heading, a), F_MONOBOLD);
            Text("prev", prev.x + 7, b.y + 5, 10, Alpha(col::dim, a), F_MONO);
            Text(addrOf(n.prev), prev.x + 5, b.y + 36, 11, Alpha(n.prev >= 0 ? col::accent2 : col::dim, a), F_MONO);
            Text("next", next.x + 7, b.y + 5, 10, Alpha(col::dim, a), F_MONO);
            Text(addrOf(n.next), next.x + 5, b.y + 36, 11, Alpha(n.next >= 0 ? col::pointer : col::dim, a), F_MONO);
        }
        Text("@" + n.addr, b.x + 4, b.y + b.height + 4, 11, Alpha(col::heap, a * 0.9f), F_MONO);
        if (n.tint == 1 && n.y < BaseY - 60) Text("newNode", b.x + b.width - Measure("newNode", 13, F_MONOBOLD).x, b.y - 20, 13, col::success, F_MONOBOLD);
    }

    void Label(const AnimFrame& f, int idx, const char* name, bool left) {
        float w = 58;
        if (idx < 0 || idx >= (int)f.nodes.size()) {
            Rectangle lb = {44, 236, 130, 24};
            if (!left) lb.x += 140;
            if (!f.nodes.empty() && left) return;    // head always set when non-empty
            RoundRect(lb, 6, Alpha(col::accent, 0.18f));
            TextCenter(std::string(name) + " = nullptr", lb, 13, col::accent, F_MONOBOLD);
            return;
        }
        Rectangle b = Box(f.nodes[idx]);
        if (b.y < BaseY - NodeH / 2 - 40) {
            // node is raised (new node waiting above the list): label on its side
            float lx = std::max(30.0f, left ? b.x - w - 34 : b.x + b.width + 34);
            Rectangle lb = {lx, b.y + b.height / 2 - 12, w, 24};
            RoundRect(lb, 6, Alpha(col::accent, 0.2f));
            RoundRectLines(lb, 6, 1, col::accent);
            TextCenter(name, lb, 13, Mix(col::accent, col::heading, 0.4f), F_MONOBOLD);
            if (left) Arrow({lb.x + w, lb.y + 12}, {b.x - 2, lb.y + 12}, col::accent, 2, 8);
            else Arrow({lb.x, lb.y + 12}, {b.x + b.width + 2, lb.y + 12}, col::accent, 2, 8);
            return;
        }
        float x = left ? b.x + 4 : b.x + b.width - w - 4;
        Rectangle lb = {x, b.y - 62, w, 24};
        RoundRect(lb, 6, Alpha(col::accent, 0.2f));
        RoundRectLines(lb, 6, 1, col::accent);
        TextCenter(name, lb, 13, Mix(col::accent, col::heading, 0.4f), F_MONOBOLD);
        Arrow({x + w / 2, lb.y + lb.height}, {x + w / 2, b.y - 2}, col::accent, 2, 8);
    }

    void CodePanel() {
        AnimFrame f = Current();
        Rectangle r = {24, 520, 640, 264};
        std::string sn = f.snippet.empty() ? Fn("InsertAtPosition") : f.snippet;
        int line = f.lineText.empty() ? -1 : sources::FindLine(sn, f.lineText);
        CodeView(r, sources::Get(sn), codeScroll, line, 14, nullptr, true);
        float tw = Measure(sn, 12, F_MONO).x + 16;
        RoundRect({r.x + r.width - tw - 10, r.y + 8, tw, 22}, 6, col::panelHi);
        Text(sn, r.x + r.width - tw - 2, r.y + 12, 12, col::dim, F_MONO);
    }

    void InfoPanel() {
        AnimFrame f = Current();
        Rectangle r = {680, 520, 576, 264};
        Panel(r, col::panel, 14);
        float x = r.x + 22, w = r.width - 44;
        std::string stepInfo = frames.size() > 1 ? "STEP " + std::to_string(fi + 1) + " / " + std::to_string(frames.size()) : "READY";
        Text(stepInfo, x, r.y + 18, 12, col::accent2, F_BOLD);
        if (frames.size() > 1) ProgressBar({x + 110, r.y + 22, w - 110, 6}, (float)fi / (frames.size() - 1), col::accent2);
        RichText({x, r.y + 44, w, 0}, f.note, 16, col::text);
        Rectangle con = {x, r.y + r.height - 92, w, 74};
        RoundRect(con, 10, col::codeBg);
        Text(doubly ? "DisplayForward()" : "Display()", con.x + 12, con.y + 10, 11.5f, col::dim, F_MONO);
        Text(Ellipsize(console, 14, F_MONO, con.width - 24), con.x + 12, con.y + 32, 14, col::text, F_MONO);
        Text("size = " + std::to_string(Size()), con.x + 12, con.y + 52, 11.5f, col::faint, F_MONO);
    }
};

} // namespace

Scene* CreateListLabScene() { return new ListLabScene(); }
