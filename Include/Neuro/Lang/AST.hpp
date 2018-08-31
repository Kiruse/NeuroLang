////////////////////////////////////////////////////////////////////////////////
// Provides the base class for AST nodes.
// ---
// Copyright (c) Kiruse. See license in LICENSE.txt, or online at http://carbon.kirusifix.com/license.
#pragma once

#include "DLLDecl.h"
#include "Numeric.hpp"

namespace Neuro {
    namespace Lang {
        class NEURO_API AST {
        protected:
            AST* prev;
            AST* next;
            
        public:
            AST();
            AST(const AST&) = delete;
            AST(AST&&) = default;
            AST& operator=(const AST&) = delete;
            virtual AST& operator=(AST&&) = default;
            virtual ~AST();
            
        public:
            /**
             * @brief Get the previous AST node in the sequence, if any.
             * 
             * @return AST* The previous AST node, or NULL if none.
             */
            AST* getPrev() const;
            
            /**
             * @brief Get the next AST node in the sequence, if any.
             * 
             * @return AST* The next AST node, or NULL if none.
             */
            AST* getNext() const;
            
            /**
             * @brief Set the previous AST node in the sequence, if any.
             * 
             * @param prev Previous AST node in sequence. Nullable.
             */
            void setPrev(AST* prev);
            
            /**
             * @brief Set the next AST node in the sequence, if any.
             * 
             * @param next Next AST node in sequence. Nullable.
             */
            void setNext(AST* next);
            
            /**
             * @brief Get the name of this AST node. Used for code generation.
             * 
             * @return const char* 
             */
            virtual const char* getName() const = NULL;
            
            /**
             * @brief Get the op code that this AST node represents.
             * 
             * @return uint32 
             */
            virtual uint32 getOpCode() const = NULL;
        };
    }
}
