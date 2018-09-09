////////////////////////////////////////////////////////////////////////////////
// The Sister type of the ExceptionBase, an error comes with a human readable
// name, description and error code. Errors, unlike exceptions, are not
// instantiated. They are singletons, and the singleton instances are passed
// around by reference. Being singletons, they are instantiated at the start
// of a program, which we use hackishly to create a registry of all errors for
// error code lookup.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include "DLLDecl.h"
#include "NeuroString.hpp"
#include "Numeric.hpp"


#define DECLARE_NEURO_ERROR(ErrorName) \
    class ErrorName##Error : public Error { \
    private: \
        static const ErrorName##Error m_inst; \
        ErrorName##Error(); \
    public: \
        static inline const ErrorName##Error& instance() { return m_inst; } \
    };

#define DEFINE_NEURO_ERROR(ErrorName, ErrorCode, ErrorMessage) \
    const ErrorName##Error ErrorName##Error::m_inst; \
    ErrorName##Error::##ErrorName##Error() : Error(ErrorCode, #ErrorName, ErrorMessage) {}

namespace Neuro {
    class NEURO_API Error {
    private:
        int32 m_code;
        String m_name;
        String m_message;
        
    public:
        /**
         * @brief Default constructor exposed for STL container used internally.
         */
        Error() = default;
        
    protected:
        Error(int32 code, const String& name, const String& message);
        
    public:
        int32 code() const { return m_code; }
        const String& name() const { return m_name; }
        const String& message() const { return m_message; }
        
    public:
        /**
         * @brief Lookup an error by its code.
         * 
         * @param code 
         * @return Error* Pointer to the error instance if any, nullptr otherwise.
         */
        static Error lookup(int32 code);
    };
    
    
    ////////////////////////////////////////////////////////////////////////////
    // Following are various Error definitions.
    
    DECLARE_NEURO_ERROR(No)
    DECLARE_NEURO_ERROR(Generic)
    DECLARE_NEURO_ERROR(NotImplemented)
    DECLARE_NEURO_ERROR(NotEnoughMemory)
}
