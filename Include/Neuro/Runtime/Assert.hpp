////////////////////////////////////////////////////////////////////////////////
// Minimalistic assertion library
#pragma once

#include <string>

// Includes from the Neuro environment.
#include "ExceptionBase.hpp"
#include "StringBuilder.hpp"

#define NEURO_ASSERT_EXPR(Expr) Neuro::Assert::Value(#Expr, Expr)

namespace Neuro {
    namespace Assert
    {
        class AssertionException : public Exception {
        protected:
            AssertionException(const std::string& title, const std::string& message, const Exception* cause) : Exception(title, message, cause) {}
        public:
            AssertionException(const std::string& message = "Assertion failed", const Exception* cause = nullptr) : Exception("AssertionException", message, cause) {}
            AssertionException(const Exception* cause) : Exception("AssertionException", "Assertion failed", cause) {}
        };
        
        template<typename T>
        struct Value {
            std::string name;
            T value;
            
            Value(const T& value) : name(), value(value) {}
            Value(const std::string& name, const T& value) : name(name), value(value) {}
            Value(const char* name, const T& value) : name(name), value(value) {}
            
            template<typename U>
            const Value& operator==(const U& other) const {
                if (value != other) throw AssertionException((StringBuilder() << *this << " != " << other).str());
                return *this;
            }
            template<typename U>
            const Value<U>& operator==(const Value<U>& other) const {
                if (value != other.value) throw AssertionException((StringBuilder() << *this << " != " << other).str());
                return other;
            }
            
            template<typename U>
            const Value& operator!=(const U& other) const {
                if (value == other) throw AssertionException((StringBuilder() << *this << " == " << other).str());
                return *this;
            }
            template<typename U>
            const Value<U>& operator!=(const Value<U>& other) const {
                if (value == other.value) throw AssertionException((StringBuilder() << *this << " == " << other).str());
                return other;
            }
            
            template<typename U>
            const Value& operator<(const U& other) const {
                if (value >= other) throw AssertionException((StringBuilder() << *this << " >= " << other).str());
                return *this;
            }
            template<typename U>
            const Value<U>& operator<(const Value<U>& other) const {
                if (value >= other.value) throw AssertionException((StringBuilder() << *this << " >= " << other).str());
                return other;
            }
            
            template<typename U>
            const Value& operator<=(const U& other) const {
                if (value > other) throw AssertionException((StringBuilder() << *this << " > " << other).str());
                return *this;
            }
            template<typename U>
            const Value<U>& operator<=(const Value<U>& other) const {
                if (value > other.value) throw AssertionException((StringBuilder() << *this << " > " << other).str());
                return other;
            }
            
            template<typename U>
            const Value& operator>(const U& other) const {
                if (value <= other) throw AssertionException((StringBuilder() << *this << " <= " << other).str());
                return *this;
            }
            template<typename U>
            const Value<U>& operator>(const Value<U>& other) const {
                if (value <= other.value) throw AssertionException((StringBuilder() << *this << " <= " << other).str());
                return other;
            }
            
            template<typename U>
            const Value& operator>=(const U& other) const {
                if (value < other) throw AssertionException((StringBuilder() << *this << " < " << other).str());
                return *this;
            }
            template<typename U>
            const Value<U>& operator>=(const Value<U>& other) const {
                if (value < other.value) throw AssertionException((StringBuilder() << *this << " < " << other).str());
                return other;
            }
        };
        
        template<typename T>
        std::ostream& operator<<(std::ostream& lhs, const Value<T>& rhs) {
            if (rhs.name.length())
                return lhs << (StringBuilder() << rhs.name << '(' << rhs.value << ')');
            return lhs << (StringBuilder() << rhs.value);
        }
        
        inline void fail() { throw new AssertionException(); }
        inline void fail(const std::string& message) { throw new AssertionException(message); }
        inline void shouldNotEnter() { throw new AssertionException("The application took a branch it should have never entered."); }
    }
}
