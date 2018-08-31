////////////////////////////////////////////////////////////////////////////////
// Available as a gist at https://gist.github.com/Kiruse/e2aee6a39ce944a9cf4bce09dda31303
// which is a header-only microlibrary wrapping around std::basic_stringstream
// to allow us to write simple one-liners for string building, and provides a
// "generic formatter" in case a formatter for a specific data type does not yet
// exist.
// -----
// Copyright (c) Kiruse 2018
// License: GNU GPL 3.0
#pragma once

#include <sstream>
#include <string>
#include "NeuroString.hpp"

namespace Neuro
{
    template<typename CharT, typename CharTraits = std::char_traits<CharT>>
    class StringBuilderBase {
    public:
        typedef std::basic_string<CharT, CharTraits> string_type;
        typedef std::basic_stringstream<CharT, CharTraits> stringstream_type;
        
    protected:
        stringstream_type ss;
        
    public:
        StringBuilderBase() = default;
        StringBuilderBase(const CharT* init) : ss() { ss << init; }
        StringBuilderBase(const string_type& init) : ss() { ss << init; }
        StringBuilderBase(const String& init) : ss() { ss << init; }
        virtual ~StringBuilderBase() = default;
        
        template<typename T>
        StringBuilderBase& operator<<(const T& value) {
            pushFormatted(value);
            return *this;
        }
        
        StringBuilderBase& operator<<(std::basic_ostream<CharT, CharTraits>& (*pf)(std::basic_ostream<CharT, CharTraits>&)) {
            ss << pf;
            return *this;
        }
        StringBuilderBase& operator<<(std::basic_ios<CharT, CharTraits>& (*pf)(std::basic_ios<CharT, CharTraits>&)) {
            ss << pf;
            return *this;
        }
        StringBuilderBase& operator<<(std::ios_base& (*pf)(std::ios_base&)) {
            ss << pf;
            return *this;
        }
        
        // Additional manipulator operator specifically for this StringBuilderBase!
        StringBuilderBase& operator<<(StringBuilderBase& (*pf)(StringBuilderBase&)) {
            return ss << pf(*this);
        }
        
        operator string_type() const {
            return str();
        }
        
        string_type str() const {
            return ss.str();
        }
        
        StringBuilderBase& str(const string_type& newstr) {
            ss.str(newstr);
            return *this;
        }
        
        operator String() const {
            return n_str();
        }
        
        String n_str() const {
            return String(str().c_str());
        }
        
        StringBuilderBase& n_str(const String& newstr) {
            ss.str(newstr.c_str());
            return *this;
        }
        
    protected:
        // This SFINAE method is the heart of both the forwarding to the appropriate
        // ostream operator, as well as the "generic formatting" feature.
        template<typename T>
        auto pushFormatted(const T& value) -> decltype(ss << value, void()) {
            ss << value;
        }
        
        // Optional "generic formatter" which simply prints "<some value>".
        void pushFormatted(...) {
            ss << "<some value>";
        }
        
    public:
        // Nice little helper to determine if an ostream << operator exists for a
        // particular data type. Both the above method pair and this one work with
        // SFINAE + template argument deduction (see https://en.cppreference.com/w/cpp/language/template_argument_deduction),
        // although I'm not sure which standard is the earliest supported. Possibly
        // C++11, but probably C++14, yet definitely C++17.
        template<typename T>
        static auto canFormat(const T& value) -> decltype(ss << value, bool()) {
            return true;
        }
        
        static bool canFormat(...) {
            return false;
        }
    };
    
    typedef StringBuilderBase<char> StringBuilder;
    typedef StringBuilderBase<wchar_t> WStringBuilder;
    
    template<typename CharT, typename CharTraits>
    inline std::basic_ostream<CharT, CharTraits>& operator<<(std::basic_ostream<CharT, CharTraits>& lhs, StringBuilderBase<CharT, CharTraits>& rhs) {
        return lhs << rhs.str();
    }
}
