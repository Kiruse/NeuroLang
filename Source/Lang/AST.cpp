#include "AST.hpp"

namespace Neuro {
    namespace Lang {
        AST::AST() : prev(NULL), next(NULL) {
            
        }
        
        AST::~AST() {
            
        }
        
        AST* AST::getPrev() const {
            return prev;
        }
        
        AST* AST::getNext() const {
            return next;
        }
        
        void AST::setPrev(AST* prev) {
            this->prev = prev;
        }
        
        void AST::setNext(AST* next) {
            this->next = next;
        }
    }
}
