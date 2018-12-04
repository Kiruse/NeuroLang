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
    
    
    DEFINE_NEURO_ERROR(No,                 0, "No error occurred. Everything is fine.")
    DEFINE_NEURO_ERROR(Generic,            1, "Some error occurred. Nothing is alright!")
    DEFINE_NEURO_ERROR(NotImplemented,     2, "Operation not implemented.")
    DEFINE_NEURO_ERROR(NotSupported,       3, "Operation currently not supported!")
    DEFINE_NEURO_ERROR(NotEnoughMemory,    4, "The application drained all available memory!")
    DEFINE_NEURO_ERROR(IllegalDuplicate,   5, "An item of this kind already exists and may not be added anew.")
    DEFINE_NEURO_ERROR(InvalidState,       6, "Invalid state for the requested operation.")
    DEFINE_NEURO_ERROR(InvalidArgument,    7, "Invalid argument passed.")
    DEFINE_NEURO_ERROR(NullPointer,        8, "Unexpected null pointer encountered.")
    DEFINE_NEURO_ERROR(DataSetNotFound,    9, "Data set not found.")
    DEFINE_NEURO_ERROR(UncaughtException, 10, "Uncaught exception")
}
