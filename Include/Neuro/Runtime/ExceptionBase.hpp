////////////////////////////////////////////////////////////////////////////////
// Base class for exceptions within the Neuro library, but outside the Neuro
// Runtime (since the runtime is a C-compatible library).
// -----
// Copyright (c) Kiruse 2018
#pragma once

#include <ostream>
#include <string>

namespace Neuro
{
    template<typename CharT, typename CharTraits = std::char_traits<CharT>>
    class ExceptionBase {
        std::basic_string<CharT, CharTraits> _title;
        std::basic_string<CharT, CharTraits> _message;
        const ExceptionBase* _cause;
        
    public:
        ExceptionBase(const std::basic_string<CharT, CharTraits>& message, const ExceptionBase* cause = nullptr) : _title("Exception"), _message(_message), _cause(cause) {}
        ExceptionBase(const std::basic_string<CharT, CharTraits>& title, const std::basic_string<CharT, CharTraits>& message, const ExceptionBase* cause = nullptr) : _title(title), _message(message), _cause(nullptr) {
            if (cause) {
                _cause = new ExceptionBase(*cause);
            }
        }
        ExceptionBase(const ExceptionBase& copy) : _title(copy._title), _message(copy._message), _cause(nullptr) {
            if (copy._cause) {
                _cause = new ExceptionBase(*copy._cause);
            }
        }
        virtual ExceptionBase& operator=(const ExceptionBase& copy) {
            _title = copy._title;
            _message = copy._message;
            if (copy._cause) {
                _cause = new ExceptionBase(*copy._cause);
            }
            return *this;
        }
        virtual ~ExceptionBase() {
            if (_cause) delete _cause;
        }
        
        virtual const std::basic_string<CharT, CharTraits>& title() const { return _title; }
        virtual const std::basic_string<CharT, CharTraits>& message() const { return _message; }
        virtual const ExceptionBase* cause() const { return _cause; }
    };
    
    typedef ExceptionBase<char> Exception;
    typedef ExceptionBase<wchar_t> WException;
    
    inline std::ostream& operator<<(std::ostream& lhs, const Neuro::Exception& rhs) {
        lhs << rhs.title() << ": " << rhs.message();
        if (rhs.cause()) lhs << ", caused by " << std::endl << *rhs.cause();
        return lhs;
    }
    inline std::wostream& operator<<(std::wostream& lhs, const Neuro::WException& rhs) {
        lhs << rhs.title() << L": " << rhs.message();
        if (rhs.cause()) lhs << L", caused by " << std::endl << *rhs.cause();
        return lhs;
    }
}
