////////////////////////////////////////////////////////////////////////////////
// A special type wrapper that keeps track of whether the stored data type is
// actually valid to access.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <type_traits>
#include <utility>

#include "DLLDecl.h"

namespace Neuro
{
    constexpr struct FMaybeForceInitTag {} MaybeForceInitTag {};
    
    /**
     * The Maybe class uses some black magic / reinterpretation_cast magic and
     * some meta data to keep track of whether the internally stored data type
     * is valid for access. The class wraps around the contained data type to
     * completely avoid heap allocation.
     * 
     * If the contained data type is trivially copyable, it is safe to trivially
     * copy this data type as well, even though the type itself is not trivial.
     */
    template<typename T>
    class Maybe {
        bool m_valid;
        /** This is where we cast magic. */
        unsigned char m_buffer[sizeof(T)];
        
    public:
        Maybe() : m_valid(false) {}
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, const U&>>>
        Maybe(const Maybe<U>& other) : m_valid(false) { set(other); }
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, U&&>>>
        Maybe(Maybe<U>&& other) : m_valid(false) { set(std::move(other)); }
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, const U&>>>
        Maybe(const U& value) : m_valid(false) { set(value); }
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, U&&>>>
        Maybe(U&& value) : m_valid(false) { set(std::move(value)); }
        template<typename... Args>
        Maybe(Args... args) : m_valid(false) { create(std::forward<Args>(args)...); }
        ~Maybe() { clear(); }
        
        
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, const U&>>>
        Maybe& operator=(const Maybe<U>& other) { return set(other); }
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, U&&>>>
        Maybe& operator=(Maybe<U>&& other) { return set(std::move(other)); }
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, const U&>>>
        Maybe& operator=(const U& value) { return set(value); }
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, U&&>>>
        Maybe& operator=(U&& value) { return set(std::move(value)); }
        
        
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, const U&>>>
        Maybe& set(const Maybe<U>& other) {
            clear();
            if (other) {
                new (m_buffer) T(*other);
                m_valid = true;
            }
            return *this;
        }
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, U&&>>>
        Maybe& set(Maybe<U>&& other) {
            clear();
            if (other) {
                new (m_buffer) T(std::move(*other));
                m_valid = true;
            }
            return *this;
        }
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, const U&>>>
        Maybe& set(const U& value) {
            clear();
            new (m_buffer) T(value);
            m_valid = true;
            return *this;
        }
        template<typename U = T, typename = std::enable_if_t<std::is_constructible_v<T, U&&>>>
        Maybe& set(U&& value) {
            clear();
            new (m_buffer) T(std::move(value));
            m_valid = true;
            return *this;
        }
        
        template<typename... Args>
        Maybe& create(Args... args) {
            clear();
            new (m_buffer) T(std::forward<Args>(args)...);
            m_valid = true;
            return *this;
        }
        
        Maybe& clear() {
            if (m_valid) {
                reinterpret_cast<T&>(m_buffer).~T();
                m_valid = false;
            }
            return *this;
        }
        
        T& get() { return reinterpret_cast<T&>(m_buffer); }
        const T& get() const { return reinterpret_cast<const T&>(m_buffer); }
        
        bool valid() const { return m_valid; }
        
        operator bool() const { return m_valid; }
        T& operator*() { return get(); }
        const T& operator*() const { return get(); }
        T* operator->() { return &get(); }
        const T* operator->() const { return &get(); }
    };
}
