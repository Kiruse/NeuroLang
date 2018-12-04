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
// TODO: Document our defined methods and constructors, lol.
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
        
    protected:
        RawHeapAllocator(uint32 size, T* data) : m_size(size), m_data(data) {}
        
    public:
        RawHeapAllocator() : m_size(0), m_data(nullptr) {}
        RawHeapAllocator(uint32 desiredSize) : m_size(desiredSize), m_data(alloc(desiredSize)) {}
        RawHeapAllocator(const RawHeapAllocator& other) : m_size(other.m_size), m_data(alloc(other.m_size)) {
            // This may be nullptr if other.size() == 0.
            if (m_data) {
                std::memcpy(m_data, other.m_data, m_size * sizeof(T));
            }
        }
        RawHeapAllocator(RawHeapAllocator&& other) : m_size(other.m_size), m_data(other.m_data) {
            other.m_data = nullptr;
            other.m_size = 0;
        }
        RawHeapAllocator& operator=(const RawHeapAllocator& other) {
            if (!m_data) {
                m_data = alloc(other.m_size);
            }
            else {
                resize(other.m_size);
            }
            
            if (m_data) {
                std::memcpy(m_data, other.m_data, other.m_size * sizeof(T));
            }
            
            return *this;
        }
        RawHeapAllocator& operator=(RawHeapAllocator&& other) {
            if (m_data) std::free(m_data);
            m_data = other.m_data;
            m_size = other.m_size;
            other.m_data = nullptr;
            other.m_size = 0;
            return *this;
        }
        ~RawHeapAllocator() {
            if (m_data) std::free(m_data);
            m_data = nullptr;
            m_size = 0;
        }
        
        void resize(uint32 desiredSize) {
            // Must not resize non-existent data.
            if (m_data && desiredSize != m_size) {
                m_data = reinterpret_cast<T*>(std::realloc(m_data, desiredSize * sizeof(T)));
                m_size = desiredSize;
            }
        }
        
        template<typename... Args>
        void create(uint32 index, uint32 count, Args... args) {
            if (m_data) {
                for (uint32 i = index; i < index + count; ++i) {
                    new (m_data + i) T(std::forward<Args>(args)...);
                }
            }
        }
        void copy(uint32 index, const T* source, uint32 count) {
            if (m_data) std::memcpy(m_data + index, source, std::min(count, m_size - index) * sizeof(T));
        }
        void copy(uint32 toIndex, uint32 fromIndex, uint32 count) {
            copy(toIndex, m_data + fromIndex, count);
        }
        void move(uint32 index, T* source, uint32 count) {
            if (m_data) std::memmove(m_data + index, source, std::min(count, m_size - index) * sizeof(T));
        }
        void move(uint32 toIndex, uint32 fromIndex, uint32 count) {
            move(toIndex, m_data + fromIndex, count);
        }
        void destroy(uint32 index, uint32 count) {
            if (m_data) {
                for (uint32 i = index; i < std::min(m_size, index + count); ++i) {
                    m_data[i].~T();
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
        
    protected:
        static T* alloc(uint32 desiredSize) {
            if (!desiredSize) return nullptr;
            return reinterpret_cast<T*>(std::malloc(sizeof(T) * desiredSize));
        }
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
        
    protected:
        RAIIHeapAllocator(uint32 size, T* data) : m_size(size), m_data(data) {}
        
    public:
        RAIIHeapAllocator() : m_size(0), m_data(nullptr) {}
        RAIIHeapAllocator(uint32 desiredSize) : m_size(desiredSize), m_data(alloc(desiredSize)) {}
        template<typename U = T, typename = std::enable_if_t<std::is_assignable_v<T, const U&>>>
        RAIIHeapAllocator(const RAIIHeapAllocator<U>& other) : m_size(other.m_size), m_data(alloc(other.m_size)) {
            // This may be nullptr if other.size() == 0.
            if (m_data) {
                copy(0, other.m_data, m_size);
            }
        }
        RAIIHeapAllocator(RAIIHeapAllocator&& other) : m_size(other.m_size), m_data(other.m_data) {
            other.m_size = 0;
            other.m_data = nullptr;
        }
        template<typename U = T, typename = std::enable_if_t<std::is_assignable_v<T, const U&>>>
        RAIIHeapAllocator& operator=(const RAIIHeapAllocator<U>& other) {
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
            if (m_data) delete[] m_data;
            m_size = 0;
            m_data = nullptr;
        }
        
        void resize(uint32 desiredSize) {
            // Must not resize non-existent data.
            if (m_data && desiredSize != m_size) {
                T* tmp = m_data;
                m_data = new T[desiredSize];
                resize_restore(tmp, std::min(desiredSize, m_size));
                delete[] tmp;
				m_size = desiredSize;
            }
        }
        
        template<typename U = T>
        std::enable_if_t<std::is_assignable_v<T, const U&>> copy(uint32 index, const U* source, uint32 count) {
            if (m_data) {
                count = std::min(count, m_size - index);
                for (uint32 i = 0; i < count; ++i) {
                    m_data[index + i] = source[i];
                }
            }
        }
        template<typename U = T>
        std::enable_if_t<std::is_assignable_v<T, const U&>> copy(uint32 to, uint32 from, uint32 count) {
            if (m_data) {
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
        }
        template<typename U = T>
        std::enable_if_t<std::is_assignable_v<T, U&&>> move(uint32 index, U* source, uint32 count) {
            if (m_data) {
                count = std::min(count, m_size - index);

                // Move backwards!
                if (source < m_data + index && m_data + index < source + count) {
                    for (uint32 i = count - 1; i != -1; --i) {
                        m_data[index + i] = std::move(source[i]);
                    }
                }
                // Move forwards!
                else {
                    for (uint32 i = 0; i < count; ++i) {
                        m_data[index + i] = std::move(source[i]);
                    }
                }
            }
        }
        template<typename U = T>
        std::enable_if_t<std::is_assignable_v<T, U&&>> move(uint32 to, uint32 from, uint32 count) {
            if (m_data) {
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
        }
        
        T* get(uint32 index) { return m_data + index; }
        const T* get(uint32 index) const { return m_data + index; }
        
        uint32 size() const { return m_size; }
        uint32 actual_size() const { return m_size; }
        uint32 numBytes() const { return m_size * sizeof(T); }
        
        T* data() { return m_data; }
        const T* data() const { return m_data; }

	protected:
        static T* alloc(uint32 desiredSize) {
            if (!desiredSize) return nullptr;
            return new T[desiredSize];
        }
        
		// The kewl thing about these fallbacks is it will first try and move,
		// then try to copy, and finally do nothing if none of those two exist.
		// That is all thanks to type coercion and function overload resolution.
		template<typename U = T>
		auto resize_restore(U* oldData, uint32 count)
			-> decltype(move(0, oldData, count))
		{
			move(0, oldData, count);
		}
		template<typename U = T>
		auto resize_restore(const U* oldData, uint32 count)
			-> decltype(copy(0, oldData, count))
		{
			copy(0, oldData, count);
		}
		void resize_restore(...) {}
    };
    
    /**
     * The utility default heap allocator for most dependent implementations
     * chooses the heap allocator based on whether the underlying data type is
     * trivial.
     * For optimization, you might want to consider using a NonTrivialHeapAllocator
     * with forced trivial copying and moving.
     */
    template<typename T> using AutoHeapAllocator = typename toggle_type<std::is_trivial_v<T>, RawHeapAllocator<T>, RAIIHeapAllocator<T>>::type;
    
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
