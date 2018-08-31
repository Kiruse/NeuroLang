////////////////////////////////////////////////////////////////////////////////
// Various general purpose allocators that can be used to operate on ranges of
// memory in arbitrary segments (e.g. heap, stack, managed). Not bound to any
// specific use case, but used for example in the Neuro::Buffer interface.
// 
// Classes that use allocators should be templated, such that it's not necessary
// for the allocators to have virtual methods, slightly improving performance.
// 
// **Allocators are not standardized!**
// 
// Allocators are designed to be general purpose. If your container does not
// support an allocator, then it simply needs a different allocator, potentially
// a custom one. On the other hand, allocators are designed to allow inheritance.
// This makes it easier to customize an allocator based on an existing allocator,
// as can be seen with the StringAllocator.
// 
// Allocators should be as generic as possible. Sometimes it's better to well-
// define the interface required by a container, and leave the specifics to the
// container itself. The most generic allocator is the RawHeapAllocator which
// does not even initialize the data. A more specialized case is the
// RAIIHeapAllocator, which initializes the data as soon as memory is acquired.
// Both have different advantages and disadvantages. For instance, the RAII
// allocator only supports default-constructible data types and types cannot be
// manually destroyed. On the other hand, the raw allocator requires manually
// tracking which elements are valid, but supports any data type and avoids
// unneccesarily initializing unused data, resulting in an overall speed increase.
// 
// Another noteworthy mention is the future ManagedAllocator, which consults
// Neuro's GarbageCollector and should allow all of Neuro's container types to
// be immediately compatible with the GarbageCollector as soon as it is implemented.
// 
// A container should document which methods it requires from an allocator,
// and a centralized documentation for various allocator methods should be
// maintained to avoid confusion or redundancy.
// 
// # Standard Allocator Methods
// 
// The following standard allocator methods exist in the basic runtime:
// 
// ## template<typename... Args> void create(uint32 index, uint32 count, Args... args);
// 
// Initialize a specific memory range, perfectly forwarding the specified
// arguments to the constructor.
// 
// ## void copy(uint32 index, const T* source, uint32 count);
// 
// Copy `count` values from the source pointer into this array at the specified
// index.
// 
// ## void copy(uint32 to, uint32 from, uint32 count);
// 
// Copy *at most* `count` values from within this buffer to another location
// in this buffer. `count` may be adjusted as to prevent writing to or reading
// from outer bounds.
// 
// ## void move(uint32 index, T* source, uint32 count);
// 
// Analogous to copy(uint32, T*, uint32);
// 
// ## void move(uint32 to, uint32 from, uint32 count);
// 
// Analogous to copy(uint32, uint32, uint32);
// 
// ## void destroy(uint32 index, uint32 count);
// 
// Destroys the specified memory range within this buffer, calling the destructor.
// 
// ## T* get(uint32 index), const T* get(uint32 index) const;
// 
// Gets the element at the specified index. May or may not verify the element.
// May or may not return NULL.
// 
// ## uint32 size() const, uint32 actual_size() const, uint32 numBytes() const;
// 
// Gets the safely usable max number of elements, the actual max number of
// elements, and the number of bytes contained in the buffer respectively.
// 
// ## T* data(), const T* data() const;
// 
// Gets the pointer to the buffer. May or may not be NULL. If an allocator
// wraps its elements to track additional data, it is strongly discouraged to
// provide this method.
// -----
// Copyright (c) Kiruse 2018
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>

#include "DLLDecl.h"
#include "Numeric.hpp"
#include "Misc.hpp"

namespace Neuro
{
    /**
     * A specialized allocator for uninitialized, heap allocated memory suitable
     * for trivial or compatible data types. All operations upon the memory
     * segment require explicit manual initialization and deconstruction.
     * 
     * For a RAII variant, @see {RAIIHeapAllocator}.
     * 
     * The allocated memory can be resized at runtime.
     * @tparam T 
     */
    template<typename T>
    struct NEURO_API RawHeapAllocator {
        uint32 m_size;
        T* m_data;
        
        RawHeapAllocator() = default;
        RawHeapAllocator(uint32 desiredSize) : m_size(desiredSize), m_data(reinterpret_cast<T*>(std::malloc(desiredSize * sizeof(T)))) {}
        RawHeapAllocator(const RawHeapAllocator& copy) : m_size(copy.m_size), m_data(reinterpret_cast<T*>(std::malloc(copy.m_size * sizeof(T)))) {
            std::memcpy(m_data, copy.m_data, m_size * sizeof(T));
        }
        RawHeapAllocator(RawHeapAllocator&& move) : m_size(move.m_size), m_data(move.m_data) {
            move.m_data = nullptr;
            move.m_size = 0;
        }
        RawHeapAllocator& operator=(const RawHeapAllocator& copy) {
            if (m_data) {
                m_data = reinterpret_cast<T*>(std::realloc(m_data, copy.m_size * sizeof(T)));
            }
            else {
                m_data = reinterpret_cast<T*>(std::malloc(copy.m_size * sizeof(T)));
            }
            if (m_data) {
                std::memcpy(m_data, copy.m_data, copy.m_size * sizeof(T));
                m_size = copy.m_size;
            }
            return *this;
        }
        RawHeapAllocator& operator=(RawHeapAllocator&& move) {
            if (m_data) {
                std::free(m_data);
            }
            m_data = move.m_data;
            m_size = move.m_size;
            move.m_data = nullptr;
            move.m_size = 0;
            return *this;
        }
        ~RawHeapAllocator() {
            if (m_data) {
                std::free(m_data);
                m_data = nullptr;
            }
        }
        
        void resize(uint32 desiredSize) {
            if (desiredSize != m_size) {
                m_data = reinterpret_cast<T*>(std::realloc(m_data, desiredSize * sizeof(T)));
                m_size = desiredSize;
            }
        }
        
        template<typename... Args>
        void create(uint32 index, uint32 count, Args... args) {
            for (uint32 i = index; i < index + count; ++i) {
                new (m_data + i) T(std::forward<Args>(args)...);
            }
        }
        void copy(uint32 index, const T* source, uint32 count) {
            std::memcpy(m_data + index, source, std::min(count, m_size - index) * sizeof(T));
        }
        void copy(uint32 toIndex, uint32 fromIndex, uint32 count) {
            copy(toIndex, m_data + fromIndex, count);
        }
        void move(uint32 index, T* source, uint32 count) {
            std::memmove(m_data + index, source, std::min(count, m_size - index) * sizeof(T));
        }
        void move(uint32 toIndex, uint32 fromIndex, uint32 count) {
            move(toIndex, m_data + fromIndex, count);
        }
        void destroy(uint32 index, uint32 count) {
            for (uint32 i = index; i < std::min(m_size, index + count); ++i) {
                m_data[i].~T();
            }
        }
        
        T* get(uint32 index) { return m_data + index; }
        const T* get(uint32 index) const { return m_data + index; }
        
        uint32 size() const { return m_size; }
        uint32 actual_size() const { return m_size; }
        uint32 numBytes() const { return m_size * sizeof(T); }
        
        T* data() { return m_data; }
        const T* data() const { return m_data; }
    };
    
    /**
     * A RAII-based heap allocator suitable for most non-trivial data types.
     * The memory should be valid at any given time.
     * 
     * Copy and move operations SFINAE, i.e. it's possible the allocator does not
     * support copy and move, rendering this 
     */
    template<typename T>
    struct NEURO_API RAIIHeapAllocator {
        uint32 m_size;
        T* m_data;
        
        RAIIHeapAllocator() = default;
        RAIIHeapAllocator(uint32 desiredSize) : m_size(desiredSize), m_data(new T[desiredSize]) {}
        RAIIHeapAllocator(const RAIIHeapAllocator& other) : m_size(other.m_size), m_data(new T[other.m_size]) {
            copy(0, other.m_data, m_size);
        }
        RAIIHeapAllocator(RAIIHeapAllocator&& other) : m_size(other.m_size), m_data(other.m_data) {
            other.m_size = 0;
            other.m_data = nullptr;
        }
        RAIIHeapAllocator& operator=(const RAIIHeapAllocator& other) {
            delete[] m_data;
            m_size = other.m_size;
            m_data = new T[other.m_size];
            copy(0, other.m_data, m_size);
            return *this;
        }
        RAIIHeapAllocator& operator=(RAIIHeapAllocator&& other) {
            delete[] m_data;
            m_size = other.m_size;
            m_data = other.m_data;
            other.m_size = 0;
            other.m_data = nullptr;
            return *this;
        }
        ~RAIIHeapAllocator() {
            delete[] m_data;
            m_size = 0;
            m_data = nullptr;
        }
        
        void resize(uint32 desiredSize) {
            if (desiredSize != m_size) {
                T* tmp = m_data;
                m_data = new T[desiredSize];
                copy(0, tmp, std::min(desiredSize, m_size));
                delete[] tmp;
				m_size = desiredSize;
            }
        }
        
        template<typename U = T>
        std::enable_if_t<std::is_copy_assignable_v<U>> copy(uint32 index, const T* source, uint32 count) {
            count = std::min(count, m_size - index);
            for (uint32 i = 0; i < count; ++i) {
                m_data[index + i] = source[i];
            }
        }
        template<typename U = T>
        std::enable_if_t<std::is_copy_assignable_v<U>> copy(uint32 to, uint32 from, uint32 count) {
            count = std::min(m_size - to, count);
            count = std::min(m_size - from, count);
            
            // Copy backwards!
            if (from < to && to < from + count) {
                for (uint32 i = count - 1; i != -1; --i) {
                    m_data[to + i] = m_data[from + i];
                }
            }
            // Copy forwards!
            else {
                for (uint32 i = 0; i < count; ++i) {
                    m_data[to + i] = m_data[from + i];
                }
            }
        }
        template<typename U = T>
        std::enable_if_t<std::is_move_assignable_v<U>> move(uint32 index, U* source, uint32 count) {
            count = std::min(count, m_size - index);

			// Move backwards!
			if (source < m_data + index && m_data + index < source + count) {
				for (uint32 i = count - 1; i != -1; --i) {
					m_data[index + i] = std::move(source[i]);
				}
			}
			else {
				for (uint32 i = 0; i < count; ++i) {
					m_data[index + i] = std::move(source[i]);
				}
			}
			// Move forwards!
        }
        template<typename U = T>
        std::enable_if_t<std::is_move_assignable_v<U>> move(uint32 to, uint32 from, uint32 count) {
            count = std::min(m_size - to, count);
            count = std::min(m_size - from, count);
            
            // Move backwards!
            if (from < to && to < from + count) {
                for (uint32 i = count - 1; i != -1; --i) {
                    m_data[to + i] = std::move(m_data[from + i]);
                }
            }
            // Move forwards!
            else {
                for (uint32 i = 0; i < count; ++i) {
                    m_data[to + i] = std::move(m_data[from + i]);
                }
            }
        }
        
        T* get(uint32 index) { return m_data + index; }
        const T* get(uint32 index) const { return m_data + index; }
        
        uint32 size() const { return m_size; }
        uint32 actual_size() const { return m_size; }
        uint32 numBytes() const { return m_size * sizeof(T); }
        
        T* data() { return m_data; }
        const T* data() const { return m_data; }
    };
    
    /**
     * The utility default heap allocator for most dependent implementations
     * chooses the heap allocator based on whether the underlying data type is
     * trivial.
     * For optimization, you might want to consider using a NonTrivialHeapAllocator
     * with forced trivial copying and moving.
     */
    template<typename T> using AutoHeapAllocator = typename toggle_type<RAIIHeapAllocator<T>, RawHeapAllocator<T>, std::is_trivial_v<T>>::type;
    
    /**
     * @brief Specialized heap allocator for Neuro::String.
     * 
     * Basically a regular heap allocator, except it allocates 1 more byte than
     * requested to store the c-string terminating 0 byte in.
     * 
     * @tparam CharT 
     */
    template<typename CharT, typename BaseAlloc = AutoHeapAllocator<CharT>>
    struct NEURO_API StringAllocator : public BaseAlloc {
        StringAllocator() = default;
        StringAllocator(uint32 desiredSize) : BaseAlloc(desiredSize + 1) {
            std::memset(m_data, 0, m_size);
        }
        
        void resize(uint32 desiredSize) {
            uint32 oldSize = m_size;
            BaseAlloc::resize(desiredSize + 1);
            if (m_size > oldSize) {
                std::memset(m_data + oldSize, 0, m_size - oldSize);
            }
            else {
                m_data[m_size - 1] = 0;
            }
        }
        uint32 size() const { return BaseAlloc::size() - 1; }
        uint32 actual_size() const { return BaseAlloc::size(); }
    };
}
