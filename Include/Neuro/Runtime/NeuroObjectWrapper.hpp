////////////////////////////////////////////////////////////////////////////////
// Wrapper around any type of object, managed or not.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include "Assert.hpp"
#include "NeuroTypes.h"
#include "NeuroObject.hpp"
#include "Numeric.hpp"

namespace Neuro
{
    /**
     * Wrapper around any type of object, be it a managed Neuro Object or any
     * other native object.
     */
    class NEURO_API ObjectWrapper {
        friend class Runtime;
        
    protected:
        uint32 m_managed : 1;
        uint32 m_marked : 1;
        void* m_data;
        
    public:
        ObjectWrapper(void* pointer = nullptr) : m_managed(false), m_marked(false), m_data(pointer) {}
        ObjectWrapper(Object* neuroObject) : m_managed(true), m_data(neuroObject) {}
        virtual ~ObjectWrapper() = default;
        
    public:
        void* get() { return m_data; }
        const void* get() const { return m_data; }
        Object* getManaged() { Assert::Value(m_managed) == true; return reinterpret_cast<Object*>(m_data); }
        const Object* getManaged() const { Assert::Value(m_managed) == true; return reinterpret_cast<const Object*>(m_data); }
        bool isManaged() const { return m_managed; }
        bool isMarked() const { return m_marked; }
        
        Object* operator->() { return getManaged(); }
        const Object* operator->() const { return getManaged(); }
        
        operator bool() const { return m_data; }
    };
}
