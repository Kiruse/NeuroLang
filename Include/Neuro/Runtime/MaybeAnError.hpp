////////////////////////////////////////////////////////////////////////////////
// Maybe meets Error. Only if Error is NoError (operator bool()) is it valid
// to read the value. Otherwise behavior is undefined.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <utility>

#include "Maybe.hpp"
#include "Error.hpp"

namespace Neuro
{
    template<typename T>
    class NEURO_API MaybeAnError {
        Maybe<T> m_maybe;
        Error m_err;
        
    public:
        MaybeAnError(const Error& err) : m_maybe(), m_err(err) {}
        MaybeAnError(const T& value) : m_maybe(value), m_err(NoError::instance()) {}
        
        Error error() const { return m_err; }
        T& value() { return *m_maybe; }
        const T& value() const { return *m_maybe; }
        
        bool valid() const { return (bool)m_err; }
        operator bool() const { return valid(); }
    };
}
