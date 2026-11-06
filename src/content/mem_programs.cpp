// ============================================================================
// mem_programs.cpp  -  Programs for the Memory Visualizer.
// Each step says which line runs, what changes in memory, and (optionally)
// a prediction question asked BEFORE the line runs. Addresses follow the
// lecture style: stack from 1000, heap from 5000, int = 4 bytes, pointer = 8.
// ============================================================================
#include "content.h"
#include <algorithm>

// ------------------------------------------------------------ MemState ----
MemCell* MemState::Find(const std::string& name) {
    for (int i = (int)cells.size() - 1; i >= 0; i--)
        if (cells[i].name == name && cells[i].state == CellState::Alive) return &cells[i];
    for (int i = (int)cells.size() - 1; i >= 0; i--)
        if (cells[i].name == name) return &cells[i];
    return nullptr;
}

MemCell* MemState::At(int addr) {
    for (int i = (int)cells.size() - 1; i >= 0; i--)
        if (cells[i].addr == addr) return &cells[i];
    return nullptr;
}

void MemState::Decl(const std::string& frame, const std::string& name, const std::string& type, int addr,
                    const std::string& value) {
    MemCell c;
    c.frame = frame; c.name = name; c.type = type; c.addr = addr; c.value = value; c.changed = true;
    cells.push_back(c);
}

void MemState::Set(const std::string& name, const std::string& value) {
    if (MemCell* c = Find(name)) { c->value = value; c->changed = true; }
}

void MemState::Point(const std::string& name, int addr) {
    if (MemCell* c = Find(name)) { c->pointsTo = addr; c->value = std::to_string(addr); c->changed = true; }
}

void MemState::PointNull(const std::string& name) {
    if (MemCell* c = Find(name)) { c->pointsTo = -1; c->value = "nullptr"; c->changed = true; }
}

void MemState::Deref(const std::string& ptrName, const std::string& value) {
    MemCell* p = Find(ptrName);
    if (!p) return;
    if (MemCell* t = At(p->pointsTo)) { t->value = value; t->changed = true; }
}

void MemState::New(int addr, const std::string& type, const std::string& value) {
    // the allocator may hand out an address that was freed before
    cells.erase(std::remove_if(cells.begin(), cells.end(), [&](const MemCell& c) {
        return c.addr == addr && c.region == Region::Heap && c.state == CellState::Freed; }), cells.end());
    MemCell c;
    c.region = Region::Heap; c.frame = "heap"; c.type = type; c.addr = addr; c.value = value; c.changed = true;
    cells.push_back(c);
}

void MemState::Free(int addr) {
    if (MemCell* c = At(addr)) { c->state = CellState::Freed; c->changed = true; }
}

void MemState::EndScope(const std::string& name) {
    if (MemCell* c = Find(name)) { c->state = CellState::OutOfScope; c->changed = true; }
}

static void Pop(MemState& m, const std::string& frame) {
    m.cells.erase(std::remove_if(m.cells.begin(), m.cells.end(),
                                 [&](const MemCell& c) { return c.frame == frame; }), m.cells.end());
}

static void HeapNamed(MemState& m, int addr, const std::string& name, const std::string& value) {
    m.New(addr, "int", value);
    m.cells.back().name = name;
}

// ------------------------------------------------------------ programs ----
static MemStep S(int line, const std::string& note, std::function<void(MemState&)> fn,
                 const std::string& ask = "", std::vector<std::string> choices = {}) {
    return MemStep{line, note, fn, ask, choices};
}

const std::vector<MemProgram>& MemPrograms() {
    static std::vector<MemProgram> progs;
    if (!progs.empty()) return progs;

    // =========================================================== basics ===
    progs.push_back({"ptr_basics", S_POINTERS, "Pointer basics: & and *",
        "The lecture illustration: one pointer, two ints.",
        {"int i = 5, j = 10;", "int* ptr;", "ptr = &i;", "*ptr = 3;", "ptr = &j;", "*ptr = -2;"},
        {
        S(0, "Two ints on the stack. Each one has an address: i lives at 1000, j at 1004 (an int is 4 bytes).",
          [](MemState& m) { m.Decl("main", "i", "int", 1000, "5"); m.Decl("main", "j", "int", 1004, "10"); }),
        S(1, "ptr is declared but not initialised: it holds garbage. Never dereference a pointer like this.",
          [](MemState& m) { m.Decl("main", "ptr", "int*", 1008, "?"); }),
        S(2, "&i is the ADDRESS of i. ptr now stores 1000: it points to i.",
          [](MemState& m) { m.Point("ptr", 1000); },
          "After `ptr = &i;` what value is stored inside ptr?", {"1000 (the address of i)", "5 (the value of i)", "10", "Still garbage"}),
        S(3, "*ptr means 'the int at the address stored in ptr'. Writing to *ptr writes into i.",
          [](MemState& m) { m.Deref("ptr", "3"); },
          "After `*ptr = 3;` what is the value of i?", {"3", "5", "1000", "i does not change"}),
        S(4, "ptr now stores 1004, the address of j. i keeps its value 3.",
          [](MemState& m) { m.Point("ptr", 1004); }),
        S(5, "The same expression *ptr now writes into j, because ptr points somewhere else.",
          [](MemState& m) { m.Deref("ptr", "-2"); },
          "What are i and j after `*ptr = -2;`?", {"i = 3, j = -2", "i = -2, j = 10", "i = -2, j = -2", "i = 5, j = -2"}),
        }});

    // ================================================ pointer to pointer ===
    progs.push_back({"pptr_exercise", S_POINTERS, "Lecture exercise: int**",
        "Slide 21: predict every value with a pointer to a pointer.",
        {"int i = 2, j = 8;", "int* ptr;", "int** pptr;", "ptr = &i;", "pptr = &ptr;", "*ptr = 7;",
         "**pptr = 12;", "ptr = &j;", "**pptr = 34;", "*pptr = &i;", "*ptr = -25;"},
        {
        S(0, "i at 1000 and j at 1004.", [](MemState& m) { m.Decl("main", "i", "int", 1000, "2"); m.Decl("main", "j", "int", 1004, "8"); }),
        S(1, "A pointer to int. Pointers are 8 bytes on a 64-bit machine.", [](MemState& m) { m.Decl("main", "ptr", "int*", 1008, "?"); }),
        S(2, "A pointer to a pointer to int.", [](MemState& m) { m.Decl("main", "pptr", "int**", 1016, "?"); }),
        S(3, "ptr points to i.", [](MemState& m) { m.Point("ptr", 1000); }),
        S(4, "pptr stores the address of ptr (1008), not the address of i.",
          [](MemState& m) { m.Point("pptr", 1008); },
          "After `pptr = &ptr;` what does pptr contain?", {"1008 (the address of ptr)", "1000 (the address of i)", "2", "Garbage"}),
        S(5, "*ptr is i.", [](MemState& m) { m.Deref("ptr", "7"); },
          "`*ptr = 7;` changes which variable?", {"i", "j", "ptr", "pptr"}),
        S(6, "*pptr is ptr, so **pptr is *ptr, which is i.", [](MemState& m) { m.Set("i", "12"); },
          "After `**pptr = 12;` what is i?", {"12", "7", "2", "i is unchanged, j becomes 12"}),
        S(7, "ptr moves to j. pptr did not change: it still points at ptr.", [](MemState& m) { m.Point("ptr", 1004); }),
        S(8, "**pptr follows pptr -> ptr -> j.", [](MemState& m) { m.Set("j", "34"); },
          "`**pptr = 34;` changes which variable?", {"j, because ptr now points to j", "i", "ptr", "pptr"}),
        S(9, "*pptr IS ptr. Assigning to it makes ptr point at i again.", [](MemState& m) { m.Point("ptr", 1000); },
          "`*pptr = &i;` changes which variable?", {"ptr: it points to i again", "pptr", "i", "j"}),
        S(10, "Done. Compare these values with your answer to the lecture exercise.", [](MemState& m) { m.Set("i", "-25"); },
          "What are the final values of i and j?", {"i = -25, j = 34", "i = 12, j = 34", "i = -25, j = 8", "i = 7, j = -25"}),
        }});

    // =================================================== arithmetic ===
    progs.push_back({"ptr_arith", S_PTRARITH, "Pointer arithmetic on an array",
        "Lecture slides 24-32: ptr++ moves by sizeof(float).",
        {"float a[4];", "float* ptr;", "ptr = &(a[2]);", "*ptr = 3.14;", "ptr++;", "*ptr = 9.0;",
         "ptr = ptr - 3;", "*ptr = 6.0;", "ptr += 2;", "*ptr = 7.0;"},
        {
        S(0, "An array is a block of consecutive cells. A float is 4 bytes, so a[1] is at 1004, a[2] at 1008...",
          [](MemState& m) { for (int k = 0; k < 4; k++) m.Decl("main", "a[" + std::to_string(k) + "]", "float", 1000 + 4 * k); }),
        S(1, "A float pointer.", [](MemState& m) { m.Decl("main", "ptr", "float*", 1016); }),
        S(2, "&(a[2]) is the address of the third element: 1008. (Same as a + 2.)", [](MemState& m) { m.Point("ptr", 1008); }),
        S(3, "Writes into a[2].", [](MemState& m) { m.Deref("ptr", "3.14"); }),
        S(4, "ptr++ adds sizeof(float) = 4 bytes: 1008 -> 1012.", [](MemState& m) { m.Point("ptr", 1012); },
          "`ptr++;` moves ptr to which address?", {"1012 (a[3])", "1009", "1016", "It stays at 1008"}),
        S(5, "Writes into a[3].", [](MemState& m) { m.Deref("ptr", "9.0"); }),
        S(6, "ptr - 3 moves back 3 elements: 1012 - 3*4 = 1000.", [](MemState& m) { m.Point("ptr", 1000); },
          "`ptr = ptr - 3;` now points at...", {"a[0] (1000)", "a[1] (1004)", "Address 1009", "Before the array"}),
        S(7, "Writes into a[0].", [](MemState& m) { m.Deref("ptr", "6.0"); }),
        S(8, "Forward 2 elements: 1000 + 8 = 1008.", [](MemState& m) { m.Point("ptr", 1008); }),
        S(9, "a[2] is overwritten. a[1] was never written: it still holds garbage.", [](MemState& m) { m.Deref("ptr", "7.0"); },
          "What is the final value of a[2]?", {"7.0", "3.14", "9.0", "6.0"}),
        }});

    // ========================================================== swap ===
    progs.push_back({"swap", S_POINTERS, "Pass by value vs pass by pointer",
        "Why swap(a, b) fails and swap(&a, &b) works.",
        {"void swapByValue(int x, int y) {", "    int z = x;", "    x = y;", "    y = z;", "}",
         "void swap(int* x, int* y) {", "    int z = *x;", "    *x = *y;", "    *y = z;", "}",
         "int main() {", "    int a = 45, b = 35;", "    swapByValue(a, b);", "    swap(&a, &b);", "}"},
        {
        S(11, "main's variables live in main's stack frame.",
          [](MemState& m) { m.Decl("main", "a", "int", 1000, "45"); m.Decl("main", "b", "int", 1004, "35"); }),
        S(12, "Calling swapByValue(a, b) COPIES the values into new variables x and y in a new stack frame.",
          [](MemState& m) { m.Decl("swapByValue", "x", "int", 2000, "45"); m.Decl("swapByValue", "y", "int", 2004, "35"); }),
        S(1, "z saves x.", [](MemState& m) { m.Decl("swapByValue", "z", "int", 2008, "45"); }),
        S(2, "x and y are swapped...", [](MemState& m) { m.Set("x", "35"); }),
        S(3, "...but they are only copies.", [](MemState& m) { m.Set("y", "45"); }),
        S(4, "The frame is destroyed when the function returns. a and b never changed.",
          [](MemState& m) { Pop(m, "swapByValue"); },
          "When swapByValue returns, what are a and b?", {"a = 45, b = 35: only the copies were swapped", "a = 35, b = 45", "Both 45", "Both 35"}),
        S(13, "Now we pass ADDRESSES: x points to a and y points to b.",
          [](MemState& m) { m.Decl("swap", "x", "int*", 2000); m.Point("x", 1000); m.Decl("swap", "y", "int*", 2008); m.Point("y", 1004); }),
        S(6, "*x reads a through the pointer.", [](MemState& m) { m.Decl("swap", "z", "int", 2016, "45"); }),
        S(7, "*x = *y writes b's value INTO a.", [](MemState& m) { m.Set("a", "35"); },
          "After `*x = *y;` what is a?", {"35", "45", "1004", "a is unchanged"}),
        S(8, "*y = z writes into b.", [](MemState& m) { m.Set("b", "45"); }),
        S(9, "The frame is gone but a and b really were swapped.", [](MemState& m) { Pop(m, "swap"); }),
        }});

    // ===================================================== dangling ===
    progs.push_back({"dangling", S_MEMORY, "Dangling pointer",
        "Returning the address of a local variable.",
        {"int* getPtrToFive() {", "    int x = 5;", "    return &x;", "}", "int main() {",
         "    int* p = getPtrToFive();", "    cout << *p << endl;  // ???", "}"},
        {
        S(5, "main declares p and calls getPtrToFive().", [](MemState& m) { m.Decl("main", "p", "int*", 1000, "?"); }),
        S(1, "x is a LOCAL variable in the function's stack frame.", [](MemState& m) { m.Decl("getPtrToFive", "x", "int", 2000, "5"); }),
        S(2, "The function returns the address of x: 2000.", [](MemState&) {}),
        S(3, "The function ends: x goes out of scope and its memory can be reused. p received 2000, so p is DANGLING.",
          [](MemState& m) { m.EndScope("x"); m.Point("p", 2000); },
          "After getPtrToFive() returns, what does p point to?",
          {"Memory that is no longer valid (x is out of scope)", "A safe copy of 5 on the heap", "nullptr", "p itself"}),
        S(6, "Undefined behaviour: it might print 5, garbage, or crash. Fix: allocate with new (see 'new / delete').",
          [](MemState& m) { m.Print("??? (undefined behaviour)"); }),
        }});

    // ========================================================= leak ===
    progs.push_back({"leak", S_MEMORY, "Memory leak in a loop",
        "Allocating 3 times but deleting once.",
        {"int* getPtrToFive() {", "    int* x = new int;", "    *x = 5;", "    return x;", "}", "int main() {",
         "    int* p;", "    for (int i = 0; i < 3; ++i) {", "        p = getPtrToFive();", "        cout << *p << endl;",
         "    }", "    delete p;", "}"},
        {
        S(6, "p is declared on the stack.", [](MemState& m) { m.Decl("main", "p", "int*", 1000); }),
        S(7, "Loop starts: i = 0.", [](MemState& m) { m.Decl("main", "i", "int", 1008, "0"); }),
        S(1, "new int allocates 4 bytes on the HEAP. x (on the stack) points to it.",
          [](MemState& m) { m.Decl("getPtrToFive", "x", "int*", 2000); m.New(5000, "int", "?"); m.Point("x", 5000); }),
        S(2, "Store 5 in the heap block.", [](MemState& m) { m.Deref("x", "5"); }),
        S(3, "x dies with the function, but the HEAP block survives. p now points to it.",
          [](MemState& m) { Pop(m, "getPtrToFive"); m.Point("p", 5000); }),
        S(9, "Prints 5.", [](MemState& m) { m.Print("5"); }),
        S(8, "i = 1: a second block is allocated and p is overwritten with its address.",
          [](MemState& m) { m.Set("i", "1"); m.New(5004, "int", "5"); m.Point("p", 5004); },
          "p now points to the 2nd block. Who still points to the 1st block at 5000?",
          {"Nobody: it is leaked", "p", "x", "The heap frees it automatically"}),
        S(9, "Prints 5.", [](MemState& m) { m.Print("5"); }),
        S(8, "i = 2: a third block. Two blocks are now unreachable.",
          [](MemState& m) { m.Set("i", "2"); m.New(5008, "int", "5"); m.Point("p", 5008); }),
        S(9, "Prints 5.", [](MemState& m) { m.Print("5"); }),
        S(10, "Loop ends, i goes out of scope.", [](MemState& m) { m.Set("i", "3"); m.EndScope("i"); }),
        S(11, "Only the LAST block is freed. The other two can never be freed: a memory leak. Fix: delete p inside the loop.",
          [](MemState& m) { m.Free(5008); },
          "How many heap blocks are still allocated after `delete p;`?", {"2", "0", "1", "3"}),
        }});

    // ==================================================== leak fixed ===
    progs.push_back({"leak_fixed", S_MEMORY, "new / delete done right",
        "Delete inside the loop: no leaks.",
        {"int* getPtrToFive() {", "    int* x = new int;", "    *x = 5;", "    return x;", "}", "int main() {",
         "    int* p;", "    for (int i = 0; i < 3; ++i) {", "        p = getPtrToFive();", "        cout << *p << endl;",
         "        delete p;", "    }", "    p = nullptr;", "}"},
        {
        S(6, "p is declared on the stack.", [](MemState& m) { m.Decl("main", "p", "int*", 1000); }),
        S(7, "i = 0.", [](MemState& m) { m.Decl("main", "i", "int", 1008, "0"); }),
        S(1, "new int: a block on the heap.", [](MemState& m) { m.Decl("getPtrToFive", "x", "int*", 2000); m.New(5000, "int", "?"); m.Point("x", 5000); }),
        S(2, "Store 5.", [](MemState& m) { m.Deref("x", "5"); }),
        S(3, "Return the heap address: still valid after the function ends (unlike a local variable).",
          [](MemState& m) { Pop(m, "getPtrToFive"); m.Point("p", 5000); }),
        S(9, "Prints 5.", [](MemState& m) { m.Print("5"); }),
        S(10, "delete p frees the block. p still holds 5000 but must not be used until reassigned.",
          [](MemState& m) { m.Free(5000); },
          "After `delete p;`, is it safe to print *p?", {"No: that is use-after-delete", "Yes, it still holds 5", "Yes, delete only clears p", "Only in C"}),
        S(8, "i = 1: the allocator may reuse the same address.", [](MemState& m) { m.Set("i", "1"); m.New(5000, "int", "5"); m.Point("p", 5000); }),
        S(9, "Prints 5.", [](MemState& m) { m.Print("5"); }),
        S(10, "Freed again.", [](MemState& m) { m.Free(5000); }),
        S(8, "i = 2.", [](MemState& m) { m.Set("i", "2"); m.New(5000, "int", "5"); m.Point("p", 5000); }),
        S(9, "Prints 5.", [](MemState& m) { m.Print("5"); }),
        S(10, "Freed.", [](MemState& m) { m.Free(5000); }),
        S(12, "Good habit: set the pointer to nullptr so it cannot dangle. No leaks: every new had a delete.",
          [](MemState& m) { m.Set("i", "3"); m.EndScope("i"); m.PointNull("p"); }),
        }});

    // ===================================================== new[] ===
    progs.push_back({"dyn_array", S_MEMORY, "Dynamic arrays: new[] / delete[]",
        "An array whose size comes from the user.",
        {"int numItems;", "cin >> numItems;          // user types 3", "int* arr = new int[numItems];",
         "for (int i = 0; i < numItems; ++i)", "    cin >> arr[i];         // 7, 4, 9",
         "for (int i = 0; i < numItems; ++i)", "    cout << arr[i] << endl;", "delete[] arr;", "arr = nullptr;"},
        {
        S(0, "numItems is not known when the program is compiled.", [](MemState& m) { m.Decl("main", "numItems", "int", 1000); }),
        S(1, "The user types 3. A stack array `int arr[numItems]` is not allowed: its size must be constant.",
          [](MemState& m) { m.Set("numItems", "3"); }),
        S(2, "new int[3] allocates 3 consecutive ints on the heap and returns the address of the first.",
          [](MemState& m) { m.Decl("main", "arr", "int*", 1008); for (int k = 0; k < 3; k++) HeapNamed(m, 5000 + 4 * k, "arr[" + std::to_string(k) + "]", "?"); m.Point("arr", 5000); },
          "How many bytes does `new int[3]` allocate (int = 4 bytes)?", {"12", "3", "4", "8"}),
        S(4, "arr[0] is *(arr + 0).", [](MemState& m) { m.Decl("main", "i", "int", 1016, "0"); m.Set("arr[0]", "7"); }),
        S(4, "arr[1] is *(arr + 1): address 5004.", [](MemState& m) { m.Set("i", "1"); m.Set("arr[1]", "4"); }),
        S(4, "arr[2] is *(arr + 2): address 5008.", [](MemState& m) { m.Set("i", "2"); m.Set("arr[2]", "9"); }),
        S(6, "Print every element.", [](MemState& m) { m.EndScope("i"); m.Print("7"); m.Print("4"); m.Print("9"); }),
        S(7, "delete[] frees the whole block.", [](MemState& m) { for (int k = 0; k < 3; k++) m.Free(5000 + 4 * k); },
          "Which statement correctly frees `new int[numItems]`?", {"delete[] arr;", "delete arr;", "free(arr);", "Nothing: it is freed automatically"}),
        S(8, "No dangling pointer left behind.", [](MemState& m) { m.PointNull("arr"); }),
        }});

    // ===================================================== malloc ===
    progs.push_back({"malloc", S_MEMORY, "C memory: malloc / free",
        "The C version of new[] / delete[].",
        {"int size = 5;", "int* arr = (int*)malloc(size * sizeof(int));", "if (arr == NULL) {",
         "    printf(\"Memory allocation failed!\\n\");", "    return 1;", "}", "for (int i = 0; i < size; i++)",
         "    arr[i] = i * 10;", "free(arr);", "arr = NULL;"},
        {
        S(0, "size on the stack.", [](MemState& m) { m.Decl("main", "size", "int", 1000, "5"); }),
        S(1, "malloc takes a number of BYTES and returns void*, which we cast to int*. The memory is not initialised.",
          [](MemState& m) { m.Decl("main", "arr", "int*", 1008); for (int k = 0; k < 5; k++) HeapNamed(m, 5000 + 4 * k, "arr[" + std::to_string(k) + "]", "?"); m.Point("arr", 5000); },
          "How many bytes does `malloc(size * sizeof(int))` request?", {"20", "5", "4", "40"}),
        S(2, "Always check for NULL: allocation can fail. Here it worked.", [](MemState&) {}),
        S(7, "Fill the array: arr[i] = i * 10.", [](MemState& m) { for (int k = 0; k < 5; k++) m.Set("arr[" + std::to_string(k) + "]", std::to_string(k * 10)); },
          "After the loop, what is arr[3]?", {"30", "3", "40", "20"}),
        S(8, "free gives the memory back. Every successful malloc needs exactly one free.",
          [](MemState& m) { for (int k = 0; k < 5; k++) m.Free(5000 + 4 * k); }),
        S(9, "Set the pointer to NULL after freeing (good practice).", [](MemState& m) { m.PointNull("arr"); m.Find("arr")->value = "NULL"; }),
        }});

    return progs;
}
