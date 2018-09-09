////////////////////////////////////////////////////////////////////////////////
// A STL-vector-like continuous dynamic array. The primary reason for providing
// our own implementation is to allow for easier cross-compiler support. We know
// exactly when the layout of the Neuro::Buffer changes, hence most changes to
// compiler specific STL implementations are irrelevant to us.
// 
// # Allocator Requirements
// 
// ## Required
// 
// - `void resize(uint32 desiredSize)`
// - `void copy(uint32, const T*, uint32)`
// - `T* get(uint32)`
// - `const T* get(uint32) const`
// - `T* data()`
// - `const T* data() const`
// - `uint32 size() const`
// - `uint32 actual_size()`
// - `uint32 numBytes()`
// 
// ## Optional
// 
// - `void move(uint32, const T*, uint32)` (preferred)
// - `void create(uint32, uint32, ...)`
// - `void destroy(uint32, uint32)`
// 
// # Trivia
// To be honest, I had started writing this buffer as a C implementation using
// a generic unsigned char buffer, where the Buffer sits on top of that utilizing
// reinterpretations of said buffer, and managing it through the C interface.
// That quickly became redundant, and I just moved the entire implementation into
// a set of header files.
// -----
// Copyright (c) Kiruse.
// License: GPL 3.0
#pragma once

#include <iostream>
#include <initializer_list>
#include <utility>
#include "Allocator.hpp"
#include "Misc.hpp"
#include "Numeric.hpp"


namespace Neuro
{
    template<typename T, typename Allocator = AutoHeapAllocator<T>>
    class NEURO_API Buffer {
        template<typename U, typename OtherAlloc> friend class Buffer;
        
    protected:
        Allocator alloc;
        uint32 m_length;
        uint32 m_expand;
        
    protected:
        /**
         * Special constructor for subclasses, allowing them to pass in a custom
         * constructed allocator. Used in the Garbage Collector.
         */
        Buffer(Allocator&& alloc, uint32 expand = 8) : alloc(std::move(alloc)), m_length(0), m_expand(expand) {}
        
    public:
        Buffer(uint32 size = 8, uint32 expand = 8) : alloc(size), m_length(0), m_expand(expand) {}
        Buffer(const std::initializer_list<T>& init, uint32 expand = 8) : alloc(init.size()), m_length(0), m_expand(expand) {
            add(init);
        }
        Buffer(const Buffer& other) : alloc(other.size()), m_length(other.m_length), m_expand(other.m_expand) {
            alloc.copy(0, other.alloc.data(), other.m_length);
        }
        Buffer(Buffer&& other) : alloc(std::move(other.alloc)), m_length(other.m_length), m_expand(other.m_expand) {}
        Buffer& operator=(const std::initializer_list<T>& values) {
            clear();
            resize(values.size());
            // This might, but should never change in future C++ standards...
            // Currently std::initializer_list::begin() is defined to return an iterator, which is defined as const T*.
            alloc.copy(0, values.begin(), values.size());
            m_length = values.size();
            return *this;
        }
        Buffer& operator=(const Buffer& other) {
            clear();
            alloc.resize(other.size());
            alloc.copy(0, other.alloc.data(), other.m_length);
            m_length = other.m_length;
            m_expand = other.m_expand;
            return *this;
        }
        Buffer& operator=(Buffer&& other) {
            clear();
            alloc = std::move(other.alloc);
            m_length = other.m_length;
            m_expand = other.m_expand;
            return *this;
        }
        virtual ~Buffer() {
            clear();
        }
        
        /**
         * @brief Resizes the buffer to hold exactly the specified number of elements.
         * 
         * @param numberOfElements 
         * @return Buffer& 
         */
        virtual Buffer& resize(uint32 numberOfElements) {
            alloc.resize(numberOfElements);
            return *this;
        }
        
        /**
         * @brief Resizes the buffer to hold at least the specified number of elements.
         * 
         * @param targetSize 
         * @return Buffer& 
         */
        virtual Buffer& fit(uint32 numberOfElements) {
            resize((numberOfElements / m_expand + 1) * m_expand);
            return *this;
        }
        
        /**
         * @brief Shrinks the buffer down to its absolutely required space.
         * 
         * @return Buffer& 
         */
        virtual Buffer& shrink() {
            resize(m_length);
            return *this;
        }
        
        /**
         * Create a new instance in-place. The underlying allocator must provide
         * the `create` method, the standard of which calling the respective
         * constructor in-place.
         * 
         * The RAIIHeapAllocator does not provide the `create` method, whilst the
         * RawHeapAllocator does.
         */
        template<typename... Args>
        auto addNew(Args... args)
         -> decltype(alloc.create(m_length, 1, args...), *this)
        {
            if (m_length + 1 > size()) {
                alloc.resize(size() + m_expand);
            }
            alloc.create(m_length, 1, std::forward<Args>(args)...);
			++m_length;
            return *this;
        }
        
        virtual Buffer& add(const T& elem) {
            if (m_length + 1 > size()) {
                alloc.resize(size() + m_expand);
            }
            alloc.copy(m_length, &elem, 1);
            ++m_length;
            return *this;
        }
        
        virtual Buffer& add(const Buffer& other) {
            return merge(other);
        }
        
        virtual Buffer& add(const std::initializer_list<T>& elems) {
            return add(elems.begin(), elems.end());
        }
        
        template<typename Iterator>
        Buffer& add(const Iterator& first, const Iterator& last) {
            if (first && last) {
                auto dist = std::distance(first, last);
                if (m_length + dist > size()) {
                    fit(m_length + dist);
                }
                alloc.copy(m_length, first, dist);
				m_length += dist;
            }
            return *this;
        }
        
        template<typename... Args>
        auto insertNew(uint32 before, Args... args)
         -> decltype(alloc.create(0, 1, args...), *this)
        {
            if (m_length + 1 > size()) {
                alloc.resize(size() + m_expand);
            }
            internal_move(before, before + 1, WorkaroundTag);
            alloc.create(before, 1, args...);
			++m_length;
            return *this;
        }
        
        virtual Buffer& insert(uint32 before, const T& elem) {
            if (m_length + 1 > size()) {
                resize(m_length + m_expand);
            }
            internal_move(before, before + 1, WorkaroundTag);
            alloc.copy(before, &elem, 1);
            ++m_length;
            return *this;
        }
        
        virtual Buffer& insert(uint32 before, const Buffer& other) {
            if (m_length + other.m_length > size()) {
                fit(m_length + other.m_length);
            }
            internal_move(before, before + other.m_length, WorkaroundTag);
            alloc.copy(before, other.alloc.data(), other.m_length);
			m_length += other.m_length;
            return *this;
        }
        
        virtual Buffer& insert(uint32 before, const std::initializer_list<T>& elems) {
            return insert(before, elems.begin(), elems.end());
        }
        
        template<typename Iterator>
        Buffer& insert(uint32 before, const Iterator& first, const Iterator& last) {
            if (first && last) {
                auto dist = std::distance(first, last);
                if (m_length + dist > size()) {
                    fit(m_length + dist);
                }
                internal_move(before, before + dist, WorkaroundTag);
                alloc.copy(before, first, dist);
				m_length += dist;
            }
            return *this;
        }
        
        virtual Buffer& merge(const Buffer& other) {
            if (m_length + other.m_length > size()) {
                fit(m_length + other.m_length);
            }
            alloc.copy(m_length, other.alloc.data(), other.m_length);
            m_length += other.m_length;
            return *this;
        }
        
        virtual Buffer& drop(uint32 numberOfElements = 1) {
            internal_destroy(m_length, numberOfElements);
            m_length -= numberOfElements;
            return *this;
        }
        
        /**
         * @brief Splices the buffer / cuts out elements from anywhere in the buffer.
         * 
         * Elements in the range [index,index+numberOfElements) will be removed
         * and subsequent elements moved up to fill in the gap.
         * 
         * @param index 
         * @param numberOfElements 
         * @return Buffer& 
         */
        virtual Buffer& splice(uint32 index, uint32 numberOfElements = 1) {
            numberOfElements = std::min(numberOfElements, m_length - index);
            if (numberOfElements) {
                // Allow the elements to properly deconstruct.
                internal_destroy(index, numberOfElements);
                
                // Are there even any elements left to move up?
                if (index + numberOfElements < m_length) {
                    internal_move(index + numberOfElements, index, WorkaroundTag);
                }
                
                m_length -= numberOfElements;
            }
            return *this;
        }
        
        /**
         * Removes the first instance of the given element within the specified
         * range.
         */
        template<bool (*Comparator)(const T&, const T&) = is::equal<T>>
        Buffer& remove(const T& elem, uint32 leftOffset = 0, uint32 rightOffset = 0) {
            uint32 index = find<Comparator>(elem, leftOffset, rightOffset);
            if (index != npos) {
                splice(index);
            }
            return *this;
        }
        
        /**
         * Removes all instances of the given element within the specified
         * range.
         */
        template<bool (*Comparator)(const T&, const T&) = is::equal<T>>
        Buffer& removeAll(const T& elem, uint32 leftOffset = 0, uint32 rightOffset = 0) {
            uint32 index = find<Comparator>(elem, leftOffset, rightOffset);
            while (index != npos) {
                splice(index);
                index = find<Comparator>(elem, leftOffset + index, rightOffset);
            }
            return *this;
        }
        
        virtual Buffer& clear() {
            // Destory everything and reset length to 0.
            internal_destroy(0, m_length);
            m_length = 0;
            return *this;
        }
        
        /**
         * Attempts to find the first instance of the given element within the
         * specified range and returns its index. If none found, returns npos.
         */
        template<bool (*Comparator)(const T&, const T&) = is::equal<T>>
        uint32 find(const T& elem, uint32 leftOffset = 0, uint32 rightOffset = 0) const {
            convertOffsets(leftOffset, rightOffset);
            
            for (uint32 i = leftOffset; i < rightOffset; ++i) {
                if (Comparator(get(i), elem)) return i;
            }
            return npos;
        }
        
        /**
         * Finds the last instance of the given element in the specified range
         * and returns its index. If none found, returns npos.
         */
        template<bool (*Comparator)(const T&, const T&) = is::equal<T>>
        uint32 findLast(const T& elem, uint32 leftOffset = 0, uint32 rightOffset = 0) const {
            convertOffsets(leftOffset, rightOffset);
            
            for (uint32 i = rightOffset - 1; i >= leftOffset && i != npos; --i) {
                if (Comparator(get(i), elem)) return i;
            }
            return npos;
        }
        
        /**
         * Attempts to find the first element that matches the specified predicate
         * and returns its index. If none found, returns npos.
         */
        template<typename Predicate>
        uint32 findByPredicate(const Predicate& pred, uint32 leftOffset = 0, uint32 rightOffset = 0) const {
            convertOffsets(leftOffset, rightOffset);
            
            for (uint32 i = leftOffset; i < rightOffset; ++i) {
                if (pred(get(i))) return i;
            }
            return npos;
        }
        
        /**
         * Attempts to find the last element that matches the specified predicate
         * and returns its index. If none found, returns npos.
         */
        template<typename Predicate>
        uint32 findLastByPredicate(const Predicate& pred, uint32 leftOffset = 0, uint32 rightOffset = 0) const {
            convertOffsets(leftOffset, rightOffset);
            
            for (uint32 i = rightOffset - 1; i >= leftOffset && i != npos; --i) {
                if (pred(get(i))) return i;
            }
            return npos;
        }
        
        virtual uint32 length() const { return m_length; }
        virtual uint32 size() const { return alloc.size(); }
        virtual uint32 actual_size() const { return alloc.actual_size(); }
        virtual uint32 numBytes() const { return alloc.numBytes(); }
        
        virtual bool empty() { return m_length == 0; }
        
        virtual T* data() { return alloc.data(); }
        virtual const T* data() const { return alloc.data(); }
        
        virtual T& get(uint32 index) { return *alloc.get(index); }
        virtual const T& get(uint32 index) const { return *alloc.get(index); }
        
        virtual T& operator[](uint32 index) { return get(index); }
        virtual const T& operator[](uint32 index) const { return get(index); }
        
        virtual T& first() { return get(0); }
        virtual const T& first() const { return get(0); }
        virtual T& last() { return get(m_length - 1); }
        virtual const T& last() const { return get(m_length - 1); }
        
        virtual T* begin() { return alloc.data(); }
        virtual const T* begin() const { return cbegin(); }
        virtual const T* cbegin() const { return alloc.data(); }
        virtual T* end() { return alloc.data() + m_length; }
        virtual const T* end() const { return cend(); }
        virtual const T* cend() const { return alloc.data() + m_length; }
        
    protected:
        /**
         * Ensures the specified left and right offsets are valid within the
         * valid range of the underlying memory buffer.
         */
        void sanitizeOffsets(uint32& leftOffset, uint32& rightOffset) const {
            leftOffset = std::min(leftOffset, m_length);
            rightOffset = std::min(rightOffset, m_length);
        }
        
        /**
         * Ensures the specified left and right offsets are valid within the
         * valid range of the underlying memory buffer and converts them into
         * absolute indices with respect to the beginning of the buffer.
         */
        void convertOffsets(uint32& leftOffset, uint32& rightOffset) const {
            sanitizeOffsets(leftOffset, rightOffset);
            rightOffset = m_length - rightOffset;
        }
        
    private:
        // A dummy type used so we can avoid additional meta types for every
        // "optional" allocator method.
        constexpr static struct NEURO_API FWorkaroundTag {} WorkaroundTag = {};
        
        template<typename U = T>
        auto internal_destroy(uint32 index, uint32 count)
         -> decltype(alloc.destroy(0, m_length), void())
        {
            alloc.destroy(index, count);
        }
        
        // Do nothing if the allocator does not provide destroy.
        void internal_destroy(...) {}
        
        /**
         * Moves elements [from,length()) around within the buffer itself.
         * Excessive elements are destroyed and overridden where applicable.
         * If move and destroy are not available, artifact duplicates may
         * arise. The calling code should strive to never leave "gaps" in the
         * buffer.
         */
        template<typename U = T>
        auto internal_move(uint32 from, uint32 to, FWorkaroundTag)
         -> decltype(alloc.move(to, from, (uint32)1), void()) // We don't care about the exact number, just the fact it's uint32.
        {
            // No need to consider to, as alloc.move should already restrict that to prevent the buffer from overflowing.
            alloc.move(to, from, m_length - from);
        }
        
        void internal_move(uint32 from, uint32 to, ...) {
            uint32 count = m_length - from;
            // No need to consider to, as alloc.copy should already restrict that to prevent the buffer from overflowing.
            alloc.copy(to, from, count);
            
            // Moving back. Destroy "empty" elements up front.
            // No need to destroy elements at the back, because they're valid anway.
            // However, internal_destroy may be a no-op if the allocator does not support it (e.g. RAIIHeapAllocator).
            if (from < to && to < from + count) {
                internal_destroy(from, to - from);
            }
        }
    };
    
    typedef Buffer<uint8> ByteBuffer;
}
