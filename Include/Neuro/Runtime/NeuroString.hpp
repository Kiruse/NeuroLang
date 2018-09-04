/*******************************************************************************
 * The Runtime Library of the Neuro Language. Writing the RTLib in C allows us
 * to link to it from most other languages, such that the NeuroLib technically
 * is a standalone library.
 * ---
 * Copyright (c) Kiruse. See license in LICENSE.txt, or online at http://neuro.kirusifix.com/license.
 */
#pragma once

#include "DLLDecl.h"
#include "Numeric.hpp"
#include "NeuroBuffer.hpp"

namespace Neuro
{
    template<typename CharT>
    NEURO_API uint32 strlen(const CharT* str, uint32 maxLen = 65536) {
        if (!str) return npos;
        const CharT* curr = str;
        uint32 iteration = 0;
        while (*(curr++) != 0) {
            if (++iteration >= maxLen) return npos;
        }
        return iteration;
    }
    
    template<typename CharT, typename Allocator = StringAllocator<CharT>>
    class NEURO_API StringBase : public Buffer<CharT, Allocator> {
        typedef Buffer<CharT, Allocator> Base;
        
    public:
        typedef CharT CharType;
        
    public:
        StringBase() = default;
        StringBase(const CharT* init) : Buffer(strlen(init)) {
            add(init);
        }
        StringBase(uint32 size) : Buffer(size) {}
        StringBase(const StringBase&) = default;
        StringBase& operator=(const StringBase&) = default;
        StringBase& operator=(const CharT* raw) {
            clear();
            const uint32 length = strlen(raw);
            if (length) {
                add(raw);
            }
            return *this;
        }
        ~StringBase() = default;
        
        StringBase& insert(uint32 before, const CharT& singleChar) {
            Base::insert(before, singleChar);
            return *this;
        }
        StringBase& insert(uint32 before, const CharT* raw) {
            const auto rawlen = strlen(raw);
            if (rawlen != npos) {
                Base::insert(before, raw, raw + rawlen);
            }
            return *this;
        }
        StringBase& insert(uint32 before, const StringBase& what) {
            Base::insert(before, what);
            return *this;
        }
        template<typename Iterator>
        StringBase& insert(uint32 before, const Iterator& begin, const Iterator& end) {
            Base::insert(before, begin, end);
            return *this;
        }
        
        StringBase& concat(const StringBase& other) {
            merge(other);
            return *this;
        }
        StringBase& append(const StringBase& other) {
            return concat(other);
        }
        StringBase& append(const CharT* other) {
            return add(other);
        }
        StringBase& append(const CharT& character) {
            return add(character);
        }
        StringBase& add(const StringBase& other) {
            Buffer::add(other);
            return *this;
        }
        StringBase& add(const CharT* other) {
            Buffer::add(other, other + strlen(other));
            return *this;
        }
        StringBase& add(const CharT& character) {
            Buffer::add(character);
            return *this;
        }
        template<typename Iterator>
        StringBase& add(const Iterator& begin, const Iterator& end) {
            Base::add(begin, end);
            return *this;
        }
        
        template<bool (*Comparator)(const CharT&, const CharT&) = is::equal<CharT>>
        uint32 find(const CharT& elem, uint32 leftOffset = 0, uint32 rightOffset = 0) const {
			return Buffer::find(elem, leftOffset, rightOffset);
        }
        
        template<bool (*Comparator)(const CharT&, const CharT&) = is::equal<CharT>>
        uint32 find(const StringBase& what, uint32 leftOffset = 0, uint32 rightOffset = 0) const {
			rightOffset = std::max(rightOffset, what.length() - 1); // length - 1 for the special case where *this == what
			convertOffsets(leftOffset, rightOffset);

            for (uint32 i = leftOffset; i < rightOffset; ++i) {
                bool found = Comparator(get(i), what[0]);
                for (uint32 j = 0; found && j < what.length() && j < length(); ++j) {
                    found = Comparator(get(i + j), what[j]);
                }
                if (found) return i;
            }
            return npos;
        }
        
        template<bool (*Comparator)(const CharT&, const CharT&) = is::equal<CharT>>
        uint32 findLast(const CharT& elem, uint32 leftOffset = 0, uint32 rightOffset = 0) const {
			return Buffer::findLast(elem, leftOffset, rightOffset);
        }
        
        template<bool (*Comparator)(const CharT&, const CharT&) = is::equal<CharT>>
        uint32 findLast(const StringBase& what, uint32 leftOffset = 0, uint32 rightOffset = 0) const {
			rightOffset = std::max(rightOffset, what.length() - 1); // length - 1 for the special case where *this == what
			convertOffsets(leftOffset, rightOffset);

			for (uint32 i = rightOffset - 1; i >= leftOffset && i != npos; --i) {
				bool found = Comparator(get(i), what[0]);
				for (uint32 j = 0; found && j < what.length(); ++j) { // No need to test j < length() because we're moving away from the end anyway.
					found = Comparator(get(i + j), what[j]);
				}
				if (found) return i;
			}
			return npos;
        }
        
        template<typename Predicate>
        uint32 findByPredicate(const Predicate& pred) {
            return findByPredicate(0, pred);
        }
        template<typename Predicate>
        uint32 findByPredicate(uint32 offset, const Predicate& pred) {
            for (uint32 i = 0; i < length(); ++i) {
                if (pred(*this, i, get(i))) return true;
            }
            return npos;
        }
        
        bool contains(CharT character) const { return find(character) != npos; }
        bool contains(const StringBase& what) const { return find(what) != npos; }
        
        StringBase& replace(CharT character, CharT replacer, uint32 leftOffset = 0, uint32 rightOffset = 0) {
            uint32 pos = find(character, leftOffset, rightOffset);
            if (pos != npos) get(pos) = replacer;
            return *this;
        }
        StringBase& replaceAll(CharT character, CharT replacer, uint32 leftOffset = 0, uint32 rightOffset = 0) {
            uint32 pos = find(character, leftOffset, rightOffset);
            while (pos != npos) {
                get(pos) = replacer;
                pos = find(character, leftOffset + pos + 1, rightOffset);
            }
            return *this;
        }
        StringBase& replaceLast(CharT character, CharT replacer, uint32 leftOffset = 0, uint32 rightOffset = 0) {
            uint32 pos = findLast(character, leftOffset, rightOffset);
            if (pos != npos) get(pos) = replacer;
            return *this;
        }
        StringBase& replace(const StringBase& what, const StringBase& with, uint32 leftOffset = 0, uint32 rightOffset = 0) {
            uint32 pos = find(what, leftOffset, rightOffset);
            if (pos != npos) {
                replace(pos, pos + what.length(), with);
            }
            return *this;
        }
        StringBase& replaceAll(const StringBase& what, const StringBase& with, uint32 leftOffset = 0, uint32 rightOffset = 0) {
            uint32 pos = find(what, leftOffset, rightOffset);
            while (pos != npos) {
                replace(pos, pos + what.length(), with);
                pos = find(what, leftOffset + pos, rightOffset);
            }
            return *this;
        }
        StringBase& replaceLast(const StringBase& what, const StringBase& with, uint32 leftOffset = 0, uint32 rightOffset = 0) {
            uint32 pos = findLast(what, leftOffset, rightOffset);
            if (pos != npos) {
                replace(pos, pos + what.length(), with);
            }
            return *this;
        }
        StringBase& replace(uint32 from, uint32 to, const StringBase& with) {
            if (from > to) return *this;
            to = std::min(to, length());
            
            uint32 replaceLength = to - from;
            if (with.length() > replaceLength) {
                const uint32 diff = with.length() - replaceLength;
                if (length() + diff > size()) {
                    fit(length() + diff);
                }
                alloc.move(from + diff, alloc.data() + from, length() - from);
                m_length += diff;
            }
            else if (with.length() < replaceLength) {
                splice(from, replaceLength - with.length());
                for (uint32 i = length(); i < size(); ++i) {
                    get(i) = 0;
                }
            }
            
            alloc.copy(from, with.data(), with.length());
            return *this;
        }
        
        StringBase substr(uint32 start, uint32 count = npos) const {
            if (start >= length()) return StringBase();
            
            uint32 end = count != npos ? std::min(length(), start + count) : length();
            
            StringBase result(end - start);
            result.add(data() + start, data() + end);
            
            return result;
        }
        
        StringBase& splice(size_t index, size_t numberOfElements = 1) {
            Buffer::splice(index, numberOfElements);
            return *this;
        }
        
        StringBase& clear() {
            Buffer::clear();
            return *this;
        }
        
        StringBase& operator+=(const StringBase& other) {
            return concat(other);
        }
        StringBase operator+(const StringBase& other) const {
            return StringBase(*this) += other;
        }
        StringBase& operator+=(const CharT& character) {
            return add(character);
        }
        StringBase operator+(const CharT& character) const {
            return StringBase(*this) + character;
        }
        
        bool operator==(const StringBase& other) const {
            if (length() != other.length()) return false;
            for (uint32 i = 0; i < length(); ++i) {
                if (get(i) != other[i]) return false;
            }
            return true;
        }
        bool operator!=(const StringBase& other) const { return !(*this == other); }
        
        bool operator==(const CharT* raw) const {
            const uint32 rawlen = strlen(raw);
            if (rawlen != length()) return false;
            for (uint32 i = 0; i < rawlen; ++i) {
                if (get(i) != raw[i]) return false;
            }
            return true;
        }
        bool operator!=(const CharT* raw) const { return !(*this == raw); }
        
        CharT* c_str() { return data(); }
        const CharT* c_str() const { return data(); }
    };
    
    template<typename CharT, typename StringAlloc, typename BufferAlloc = AutoHeapAllocator<StringBase<CharT, StringAlloc>>>
    Buffer<StringBase<CharT, StringAlloc>, BufferAlloc> split(const StringBase<CharT, StringAlloc>& source, const CharT& delim) {
        typedef StringBase<CharT, StringAlloc> String;
        
        Buffer<String, BufferAlloc> result;
        uint32 pos = source.find(delim);
        if (pos == npos) {
            result.add(source);
        }
        else {
            uint32 prev = 0; // -1 so in the first iteration -1 + 1 == 0.
            do {
                result.add(source.substr(prev, pos - prev));
                prev = pos + 1;
                pos = source.find(delim, pos + 1);
            } while (pos != npos);
            
            // Add the final substring.
            result.add(source.substr(prev));
        }
        return result;
    }
    
    template<typename CharT, typename StringAlloc, typename BufferAlloc>
    static StringBase<CharT, StringAlloc> join(const Buffer<StringBase<CharT, StringAlloc>, BufferAlloc>& buffer, const CharT& character) {
        StringBase<CharT, StringAlloc> result;
        for (size_t i = 0; i < buffer.length(); ++i) {
            result += buffer[i];
            if (i != buffer.length() - 1) result += character;
        }
        return result;
    }
    
    typedef StringBase<char> String;
    typedef StringBase<wchar_t> WString;
    
    template<typename CharT, typename Allocator>
    std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& lhs, const StringBase<CharT, Allocator>& rhs) {
        return lhs << rhs.data();
    }
}
