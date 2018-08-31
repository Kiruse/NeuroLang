////////////////////////////////////////////////////////////////////////////////
// Definition of the Error class, its lookup registry, and the lookup method
// itself.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include <map>
#include "Assert.hpp"
#include "Error.hpp"

namespace Neuro
{
    static std::map<uint32, Error> g_neuroErrorRegistry;
    
    
    Error::Error(int32 code, const String& name, const String& message) : m_code(code), m_name(name), m_message(message) {
        // There can only be one.
        Assert::Value(g_neuroErrorRegistry.count(code)) == 0;
        g_neuroErrorRegistry[code] = *this;
    }
    
    Error Error::lookup(int32 code) {
        auto result = g_neuroErrorRegistry.find(code);
        if (result != g_neuroErrorRegistry.end()) {
            return (*result).second;
        }
        return NoError::instance();
    }
    
    
    DEFINE_NEURO_ERROR(No, 0, "No error occurred. Everything is fine.")
}
