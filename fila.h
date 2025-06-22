// linked_queue.hpp
#ifndef LINKED_QUEUE_HPP
#define LINKED_QUEUE_HPP

#include <cstddef>    // for size_t
#include <utility>    // for std::move
#include <stdexcept>  // for std::runtime_error

// Template de fila encadeada simples (FIFO) em C++
// Implementa operações básicas: enqueue, dequeue, front (peek), empty, size, clear.
// Não permite cópia padrão (por simplicidade), mas permite mover.

template <typename T>
class LinkedQueue {
private:
    struct Node {
        T data;
        Node* next;
        // Construtor para dados lvalue e rvalue
        Node(const T& value) : data(value), next(nullptr) {}
        Node(T&& value) : data(std::move(value)), next(nullptr) {}
    };

    Node* head; // primeiro nó
    Node* tail; // último nó
    size_t count;

    // Disable copy operations (poderia ser implementado, mas omitido para simplicidade)
    LinkedQueue(const LinkedQueue&) = delete;
    LinkedQueue& operator=(const LinkedQueue&) = delete;

public:
    // Construtor padrão: fila vazia
    LinkedQueue() : head(nullptr), tail(nullptr), count(0) {}

    // Move constructor
    LinkedQueue(LinkedQueue&& other) noexcept : head(other.head), tail(other.tail), count(other.count) {
        other.head = nullptr;
        other.tail = nullptr;
        other.count = 0;
    }

    // Move assignment
    LinkedQueue& operator=(LinkedQueue&& other) noexcept {
        if (this != &other) {
            clear();
            head = other.head;
            tail = other.tail;
            count = other.count;
            other.head = nullptr;
            other.tail = nullptr;
            other.count = 0;
        }
        return *this;
    }

    // Destrutor: libera todos os nós
    ~LinkedQueue() {
        clear();
    }

    // Retorna true se vazia
    bool empty() const noexcept {
        return head == nullptr;
    }

    // Retorna número de elementos
    size_t size() const noexcept {
        return count;
    }

    // Enfileira elemento por cópia
    void enqueue(const T& value) {
        Node* node = new Node(value);
        if (!node) {
            throw std::runtime_error("Falha ao alocar nó para enqueue");
        }
        if (tail) {
            tail->next = node;
            tail = node;
        } else {
            // fila vazia
            head = tail = node;
        }
        ++count;
    }

    // Enfileira elemento por movimento
    void enqueue(T&& value) {
        Node* node = new Node(std::move(value));
        if (!node) {
            throw std::runtime_error("Falha ao alocar nó para enqueue");
        }
        if (tail) {
            tail->next = node;
            tail = node;
        } else {
            head = tail = node;
        }
        ++count;
    }

    // Retorna referência ao elemento do início (peek). Lança exceção se vazia.
    T& front() {
        if (empty()) {
            throw std::runtime_error("Tentativa de front em fila vazia");
        }
        return head->data;
    }

    const T& front() const {
        if (empty()) {
            throw std::runtime_error("Tentativa de front em fila vazia");
        }
        return head->data;
    }

    // Desenfileira o elemento do início e retorna seu valor.
    // Lança exceção se fila vazia.
    T dequeue() {
        if (empty())
            return NULL;
        
        Node* node = head;
        T value = std::move(node->data);
        head = head->next;
        if (!head) {
            tail = nullptr;
        }
        delete node;
        --count;
        return value;
    }

    // Tenta desenfileirar: se vazia retorna false, senão atribui *out e retorna true
    bool try_dequeue(T& out) {
        if (empty()) {
            return false;
        }
        Node* node = head;
        out = std::move(node->data);
        head = head->next;
        if (!head) {
            tail = nullptr;
        }
        delete node;
        --count;
        return true;
    }

    T get_object (int i)
    {
        if (empty())
            return NULL;

        Node *node = head;
        for (int j = 0; j < (i - 1) && node; j++){
            node = node->next;
        }

        T value = std::move(node->data);
        return value;
    }

    // Limpa todos os elementos: destrói nós e seus dados
    void clear() noexcept {
        Node* cur = head;
        while (cur) {
            Node* next = cur->next;
            delete cur;
            cur = next;
        }
        head = nullptr;
        tail = nullptr;
        count = 0;
    }
};

#endif // LINKED_QUEUE_HPP
