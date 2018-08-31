////////////////////////////////////////////////////////////////////////////////
// An abstract variable in the Neuro Language. The Runtime does not use variables,
// only the values stored within them.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include "DLLDecl.h"
#include "NeuroTypes.h"
#include "NeuroString.hpp"
#include "NeuroValue.hpp"

namespace Neuro
{
    namespace Lang {
        /** Represents a variable on the stack. */
        class NEURO_API Variable {
            const String m_name;
            Value m_value;
            
        public:
            Variable(const String& name) : m_name(name), m_value() {}
            Variable(const String& name, const Value& value) : m_name(name), m_value(value) {}
            Variable(const String& name, Value&& value) : m_name(name), m_value(value) {}
            virtual ~Variable();
            
        public:
            const String& name() const { return m_name; }
            Value value() { return m_value; }
            const Value value() const { return m_value; }
        };
    }
}
