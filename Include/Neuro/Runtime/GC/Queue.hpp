////////////////////////////////////////////////////////////////////////////////
// A queue with a very specific niche use:
// 
//  - Queue is, as per definition, FIFO.
//  - Pushing elements to the queue is completely wait-free.
//  - Pushing elements performs at O(1) (queue is reversed, more like a stack).
//  - Insertion and removal are completely prohibited.
//  - Entire queue can be extracted by atomically swapping the root pointer.
//  - Queue is written like a linked list to enable atomic operations.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "Numeric.hpp"

namespace Neuro {
    namespace Runtime {
        namespace GC
        {
            template<typename T>
            struct QueueElement {
                T value;
                std::atomic<QueueElement*> next;
                
                QueueElement() = default;
                QueueElement(const T& value) : value(value), next(nullptr) {}
                QueueElement(T&& value) : value(value), next(nullptr) {}
            };
            
            template<typename T>
            class Queue {
                std::atomic<QueueElement<T>*> first;
                
            public:
                Queue() : first(nullptr) {}
                Queue(const Queue& other) : first(nullptr) {
                    enqueue(other);
                }
                Queue(Queue&& other) : first(other.first.exchange(nullptr)) {}
                Queue& operator=(const Queue& other) {
                    // Order matters!
                    QueueElement<T>* oldFirst = first.exchange(nullptr);
                    enqueue(other);
                    purge(oldFirst);
                    return *this;
                }
                Queue& operator=(Queue&& other) {
                    return extract(other);
                }
                ~Queue() {
                    purge(first.exchange(nullptr));
                }
                
                
                template<typename U>
                auto enqueue(const U& value)
                 -> decltype(QueueElement<T>(value), *this)
                {
                    return enqueue(new QueueElement<T>(value));
                }
                
                template<typename U>
                auto enqueue(U&& value)
                 -> decltype(QueueElement<T>(value), *this)
                {
                    return enqueue(new QueueElement<T>(value));
                }
                
                /**
                 * Moves the contents of the target queue to this queue. Both
                 * queues are left in a valid state. This queue's old contents
                 * will be deleted.
                 * 
                 * Essentially a move-assignment where the moved object is left
                 * valid. Accessing moved objects is undefined behavior and thus
                 * heavily implementation specific. In this case, move-assignment
                 * is extraction, but I moved the logic here for clear distinction.
                 * 
                 * We don't "simply" append the new queue to the old queue because
                 * it is not 100% thread-safe. In fact, it can probably be made
                 * around 99.999% thread-safe, but our use-case foresees finishing
                 * the entire queue before we schedule the next scan (and extraction),
                 * so why take risks?
                 */
                Queue& extract(Queue& target) {
                    // Order here matters again!
                    purge(first.exchange(target.first.exchange(nullptr)));
                    return *this;
                }
                
                QueueElement<T>* getFirst() const {
                    return first.load();
                }
                
                operator Buffer<T>() const {
                    Buffer<T> result;
                    auto* elem = first.load();
                    while (elem) {
                        result.add(elem->value);
                        elem = elem->next;
                    }
                    return result;
                }
                
                operator StandardHashSet<T>() const {
                    StandardHashSet<T> result;
                    auto* elem = first.load();
                    while (elem) {
                        result.add(elem->value);
                        elem = elem->next;
                    }
                    return result;
                }
                
            protected:
                Queue& enqueue(QueueElement<T>* elem) {
                    // There is an atomically small window here where iterating
                    // over the queue can stop spuriously in between the assignment
                    // and the exchange, but usually iteration should happen on
                    // an extracted queue in a thread-safe environment.
                    // What matters is that enqueueing is O(1) and wait-free.
                    elem->next = first.exchange(elem);
                    return *this;
                }
                
                QueueElement<T>* findLast(QueueElement<T>* start) {
                    QueueElement<T>* curr = start;
                    while (curr->next.load()) curr = curr->next;
                    return curr;
                }
                
                void purge(QueueElement<T>* start) {
                    QueueElement<T>* curr = start;
                    while (curr) {
                        QueueElement<T>* old = curr;
                        curr = curr->next;
                        delete old;
                    }
                }
            };
        }
    }
}
