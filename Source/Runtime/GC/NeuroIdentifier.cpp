////////////////////////////////////////////////////////////////////////////////
// Implementation of the Neuro Identifier's global registry (the identifier
// itself is a PoD type).
// 
// The Garbage Collector's Main Thread will occasionally rebalance the graph when
// not busy.
// 
// TODO: Optimize the tree!
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include <atomic>
#include <memory>

#include "Assert.hpp"
#include "NeuroIdentifier.hpp"
#include "GC/Queue.hpp"

namespace Neuro {
    namespace Runtime
    {
        /**
         * An element in the identifier registry's search tree.
         */
        struct IdentifierRegistryNode {
            /**
             * Name associated with this identifier.
             */
            String name;
            
            /**
             * Globally unique number of this identifier.
             */
            uint32 number;
            
            /**
             * Number of requests for this identifier. The more often an identifier
             * has been requested, the higher it rises in the graph in order to
             * shorten the search times.
             * 
             * CURRENTLY UNUSED
             */
            uint32 requests;
            
            /**
             * Pointer to the parent node.
             */
            std::atomic<IdentifierRegistryNode*> parent;
            
            /**
             * Pointer to the left subtree.
             */
            std::atomic<IdentifierRegistryNode*> left;
            
            /**
             * Pointer to the right subtree.
             */
            std::atomic<IdentifierRegistryNode*> right;
        };
        
        namespace IdentifierRegistryGlobals
        {
            /**
             * Number to assign to the next newly registered identifier. This number
             * may not be strictly sequential in order to avoid data races.
             */
            std::atomic<uint32> nextNumber = 0;
            
            /**
             * Number of currently active insertions in the identifier registry.
             */
            std::atomic<uint8> activeInsertions = 0;
            
            /**
             * Pointer to the root element of the identifier registry's search tree.
             */
            std::atomic<IdentifierRegistryNode*> root = nullptr;
            
            /**
             * Helper class similar to the scoped_lock to increment the
             * `activeInsertions` counter upon instantiation and decrement it
             * again upon destruction through RAII.
             */
            class InsertionGuard {
            public:
                InsertionGuard() { activeInsertions.fetch_add(1); }
                InsertionGuard(const InsertionGuard&) = delete;
                InsertionGuard(InsertionGuard&&) = delete;
                InsertionGuard& operator=(const InsertionGuard&) = delete;
                InsertionGuard& operator=(InsertionGuard&&) = delete;
                ~InsertionGuard() { activeInsertions.fetch_sub(1); }
            };
        }
        
        Identifier Identifier::lookup(const String& name) {
            using namespace IdentifierRegistryGlobals;
            
            // More or less a semaphore for the tree optimization routine.
            InsertionGuard{};
            
            // Atomic comparison value.
            IdentifierRegistryNode* cmp = nullptr;
            
            IdentifierRegistryNode* elem = new IdentifierRegistryNode;
            elem->name = name;
            elem->number = 0;
            elem->parent = elem->left = elem->right = nullptr;
            
            // Double-checked root replacement in order to avoid increasing the
            // nextNumber for every single lookup.
            if (root.load() == nullptr) {
                elem->number = nextNumber.fetch_add(1, std::memory_order_release);
                if (root.compare_exchange_strong(cmp, elem)) {
                    return Identifier(elem->number);
                }
            }
            
            // Double-checked insertion, to avoid incrementing nextNumber if not
            // necessary.
            IdentifierRegistryNode* curr = root.load();
            if (!curr) Assert::shouldNotEnter(); // because once set, root should never become nullptr again!
            
            while (curr) {
                int8 side = name.compareTo(curr->name);
                
                // Left side (DO NOT INSERT YET)
                if (side == -1) {
                    if (curr->left.load() == nullptr) {
                        break;
                    }
                    curr = curr->left.load();
                }
                
                // Right side (DO NOT INSERT YET)
                else if (side == 1) {
                    if (curr->right.load() == nullptr) {
                        break;
                    }
                    curr = curr->right.load();
                }
                
                // Exact match -> delete the temporary and return the existing's number.
                else {
                    delete elem;
                    return Identifier(curr->number);
                }
            }
            
            if (!curr) Assert::shouldNotEnter(); // because we're trying to find a valid attachment point with curr!
            
            // Element not (yet) found, fetch and increment nextNumber.
            elem->number = nextNumber.fetch_add(1);
            
            // Second loop differs significantly in that it actually inserts
            // the element if not found, because this time we have an actual
            // number associated.
            while (curr) {
                int8 side = name.compareTo(curr->name);
                
                // Left side
                if (side == -1) {
                    if (curr->left.compare_exchange_strong(cmp, elem)) {
                        return Identifier(elem->number);
                    }
                    curr = curr->left.load();
                }
                
                // Right side
                else if (side == 1) {
                    if (curr->right.compare_exchange_strong(cmp, elem)) {
                        return Identifier(elem->number);
                    }
                    curr = curr->right.load();
                }
                
                // Exact match -> delete the temporary and return the existing's number.
                // We also don't care about the increment above, the error is negligible.
                else {
                    delete elem;
                    return Identifier(curr->number);
                }
            }
            
            Assert::shouldNotEnter(); // because either we've found the node, or we've inserted it. The loop should
                                      // not terminate through curr == false, but through the return exits!
            return Identifier(0);
        }
        
        void Identifier::resetRegistry() {
            IdentifierRegistryNode* oldRoot = IdentifierRegistryGlobals::root.exchange(nullptr);
            Buffer<IdentifierRegistryNode*> nodes({oldRoot});
            
            while (nodes.length()) {
                auto* curr = nodes[0];
                nodes.splice(0);
                nodes.add(curr->left);
                nodes.add(curr->right);
                delete curr;
            }
        }
    }
}
