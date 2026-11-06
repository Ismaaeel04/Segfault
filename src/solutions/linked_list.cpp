// ============================================================================
// linked_list.cpp  -  Singly and doubly linked lists (Coding Exam 2, Part 2)
// Every node is created with `new` and destroyed with `delete` exactly once.
// ============================================================================
#include "linked_list.h"
#include "exam_output.h"

// ============================================================== LinkedList ==
LinkedList::LinkedList() : head(nullptr) {}

LinkedList::~LinkedList() {
    Clear();
}

// @begin LinkedList::InsertAtBeginning
void LinkedList::InsertAtBeginning(int value) {
    Node* newNode = new Node(value);    // allocated on the heap
    newNode->next = head;               // new node points at the old first node
    head = newNode;                     // head now points at the new node
}
// @end

// @begin LinkedList::InsertAtEnd
void LinkedList::InsertAtEnd(int value) {
    Node* newNode = new Node(value);
    if (head == nullptr) {
        head = newNode;                 // empty list: new node is the first
        return;
    }
    Node* temp = head;
    while (temp->next != nullptr) {
        temp = temp->next;              // walk to the last node
    }
    temp->next = newNode;               // last node now links to the new one
}
// @end

// @begin LinkedList::InsertAtPosition
bool LinkedList::InsertAtPosition(int value, int position) {
    if (position < 0 || position > Size()) return false;
    if (position == 0) {
        InsertAtBeginning(value);
        return true;
    }
    Node* newNode = new Node(value);
    Node* temp = head;
    for (int i = 0; i < position - 1; i++) {
        temp = temp->next;              // stop at the node BEFORE position
    }
    newNode->next = temp->next;         // 1) link new node to the rest first
    temp->next = newNode;               // 2) then link the previous node to it
    return true;
}
// @end

// @begin LinkedList::DeleteAtPosition
bool LinkedList::DeleteAtPosition(int position) {
    if (head == nullptr || position < 0 || position >= Size()) return false;
    if (position == 0) {
        Node* toDelete = head;
        head = head->next;              // unlink the first node
        delete toDelete;                // free its memory
        return true;
    }
    Node* temp = head;
    for (int i = 0; i < position - 1; i++) {
        temp = temp->next;
    }
    Node* toDelete = temp->next;
    temp->next = toDelete->next;        // bypass the node
    delete toDelete;
    return true;
}
// @end

// @begin LinkedList::Search
int LinkedList::Search(int value) const {
    int index = 0;
    Node* temp = head;
    while (temp != nullptr) {
        if (temp->data == value) return index;
        temp = temp->next;
        index++;
    }
    return -1;
}
// @end

// @begin LinkedList::Reverse
void LinkedList::Reverse() {
    Node* prev = nullptr;
    Node* current = head;
    while (current != nullptr) {
        Node* nextNode = current->next; // remember the rest of the list
        current->next = prev;           // flip the arrow
        prev = current;
        current = nextNode;
    }
    head = prev;
}
// @end

int LinkedList::Size() const {
    int count = 0;
    for (Node* temp = head; temp != nullptr; temp = temp->next) count++;
    return count;
}

// @begin LinkedList::Clear
void LinkedList::Clear() {
    Node* temp = head;
    while (temp != nullptr) {
        Node* nextNode = temp->next;    // save before deleting!
        delete temp;
        temp = nextNode;
    }
    head = nullptr;                     // no dangling pointer left behind
}
// @end

void LinkedList::Display() const {
    for (Node* temp = head; temp != nullptr; temp = temp->next) {
        examPrintf("%d -> ", temp->data);
    }
    examPrintf("nullptr\n");
}

// ======================================================== DoublyLinkedList ==
DoublyLinkedList::DoublyLinkedList() : head(nullptr), tail(nullptr) {}

DoublyLinkedList::~DoublyLinkedList() {
    Clear();
}

// @begin DoublyLinkedList::InsertAtBeginning
void DoublyLinkedList::InsertAtBeginning(int value) {
    DoublyLinkedNode* newNode = new DoublyLinkedNode(value);
    if (head == nullptr) {
        head = tail = newNode;                  // first node is head AND tail
        return;
    }
    newNode->nextPointer = head;
    head->prevPointer = newNode;
    head = newNode;
}
// @end

// @begin DoublyLinkedList::InsertAtEnd
void DoublyLinkedList::InsertAtEnd(int value) {
    DoublyLinkedNode* newNode = new DoublyLinkedNode(value);
    if (tail == nullptr) {
        head = tail = newNode;
        return;
    }
    newNode->prevPointer = tail;                // no walking needed: we have tail
    tail->nextPointer = newNode;
    tail = newNode;
}
// @end

// @begin DoublyLinkedList::InsertAtPosition
bool DoublyLinkedList::InsertAtPosition(int value, int position) {
    int size = Size();
    if (position < 0 || position > size) return false;
    if (position == 0) {
        InsertAtBeginning(value);
        return true;
    }
    if (position == size) {
        InsertAtEnd(value);
        return true;
    }
    DoublyLinkedNode* newNode = new DoublyLinkedNode(value);
    DoublyLinkedNode* temp = head;
    for (int i = 0; i < position - 1; i++) {
        temp = temp->nextPointer;               // node BEFORE the position
    }
    newNode->nextPointer = temp->nextPointer;   // 4 links to set:
    newNode->prevPointer = temp;
    temp->nextPointer->prevPointer = newNode;
    temp->nextPointer = newNode;
    return true;
}
// @end

// @begin DoublyLinkedList::DeleteAtPosition
bool DoublyLinkedList::DeleteAtPosition(int position) {
    if (head == nullptr || position < 0 || position >= Size()) return false;
    DoublyLinkedNode* toDelete = head;
    for (int i = 0; i < position; i++) {
        toDelete = toDelete->nextPointer;
    }
    if (toDelete->prevPointer != nullptr) toDelete->prevPointer->nextPointer = toDelete->nextPointer;
    else head = toDelete->nextPointer;
    if (toDelete->nextPointer != nullptr) toDelete->nextPointer->prevPointer = toDelete->prevPointer;
    else tail = toDelete->prevPointer;
    delete toDelete;
    return true;
}
// @end

// @begin DoublyLinkedList::Search
int DoublyLinkedList::Search(int value) const {
    int index = 0;
    DoublyLinkedNode* temp = head;
    while (temp != nullptr) {
        if (temp->data == value) return index;
        temp = temp->nextPointer;
        index++;
    }
    return -1;
}
// @end

int DoublyLinkedList::Size() const {
    int count = 0;
    for (DoublyLinkedNode* temp = head; temp != nullptr; temp = temp->nextPointer) count++;
    return count;
}

void DoublyLinkedList::Clear() {
    DoublyLinkedNode* temp = head;
    while (temp != nullptr) {
        DoublyLinkedNode* nextNode = temp->nextPointer;
        delete temp;
        temp = nextNode;
    }
    head = tail = nullptr;
}

void DoublyLinkedList::DisplayForward() const {
    examPrintf("nullptr <- ");
    for (DoublyLinkedNode* temp = head; temp != nullptr; temp = temp->nextPointer) {
        examPrintf("%d%s", temp->data, temp->nextPointer ? " <-> " : "");
    }
    examPrintf(" -> nullptr\n");
}

void DoublyLinkedList::DisplayBackward() const {
    for (DoublyLinkedNode* temp = tail; temp != nullptr; temp = temp->prevPointer) {
        examPrintf("%d ", temp->data);
    }
    examPrintf("\n");
}
