// ============================================================================
// linked_list.h  -  Coding Exam 2, Part 2
//   1. LinkedList (singly linked, from the lecture notes)
//   2. DoublyLinkedList + DoublyLinkedNode (prevPointer AND nextPointer)
//   3. InsertAtBeginning, InsertAtEnd, InsertAtPosition (positions start at 0)
// ============================================================================
#pragma once

// ---------------------------------------------------------------- singly ----
class Node {
public:
    int data;
    Node* next;
    Node(int value) : data(value), next(nullptr) {}
};

class LinkedList {
public:
    LinkedList();
    ~LinkedList();                                // frees every node (no leaks)
    LinkedList(const LinkedList&) = delete;       // copying would share nodes
    LinkedList& operator=(const LinkedList&) = delete;

    void InsertAtBeginning(int value);
    void InsertAtEnd(int value);
    bool InsertAtPosition(int value, int position);
    bool DeleteAtPosition(int position);
    int  Search(int value) const;                 // index, or -1
    void Reverse();
    int  Size() const;
    void Clear();
    void Display() const;
    Node* GetHead() const { return head; }

private:
    Node* head;
};

// ---------------------------------------------------------------- doubly ----
class DoublyLinkedNode {
public:
    int data;
    DoublyLinkedNode* prevPointer;
    DoublyLinkedNode* nextPointer;
    DoublyLinkedNode(int value) : data(value), prevPointer(nullptr), nextPointer(nullptr) {}
};

class DoublyLinkedList {
public:
    DoublyLinkedList();
    ~DoublyLinkedList();
    DoublyLinkedList(const DoublyLinkedList&) = delete;
    DoublyLinkedList& operator=(const DoublyLinkedList&) = delete;

    void InsertAtBeginning(int value);
    void InsertAtEnd(int value);
    bool InsertAtPosition(int value, int position);
    bool DeleteAtPosition(int position);
    int  Search(int value) const;
    int  Size() const;
    void Clear();
    void DisplayForward() const;
    void DisplayBackward() const;
    DoublyLinkedNode* GetHead() const { return head; }
    DoublyLinkedNode* GetTail() const { return tail; }

private:
    DoublyLinkedNode* head;
    DoublyLinkedNode* tail;
};
