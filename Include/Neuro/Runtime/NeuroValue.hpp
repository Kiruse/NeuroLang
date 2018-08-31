////////////////////////////////////////////////////////////////////////////////
// A stack-allocated wrapper around the various types of primitives or an object
// pointer.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include "DLLDecl.h"
#include "NeuroObjectWrapper.hpp"
#include "NeuroTypes.h"

namespace Neuro
{
    /**
     * A "typeless" wrapper around an arbitrary value in the Neuro Runtime.
     * The value may be any of the primitives (boolean, integer, float / double)
     * or an object wrapper, which in turn wraps around a managed Neuro Object,
     * or an arbitrary native object.
     */
    class NEURO_API Value {
        neuroValueType m_type;
        bool m_unsigned;
        union {
            bool m_boolValue;
            uint8 m_byteValue;
            int16 m_shortValue;
            int32 m_intValue;
            int64 m_longValue;
            float m_floatValue;
            double m_doubleValue;
            ObjectWrapper m_objectValue;
        };
        
    public:
        Value() : m_type(NVT_Undefined) {}
        Value(bool value) : m_type(NVT_Bool), m_boolValue(value) {}
        Value(uint8 value) : m_type(NVT_Byte), m_unsigned(true), m_byteValue(value) {}
        Value(int8 value) : m_type(NVT_Byte), m_unsigned(false), m_byteValue(static_cast<uint8>(value)) {}
        Value(uint16 value) : m_type(NVT_Short), m_unsigned(true), m_shortValue(static_cast<int16>(value)) {}
        Value(int16 value) : m_type(NVT_Short), m_unsigned(false), m_shortValue(value) {}
        Value(uint32 value) : m_type(NVT_Integer), m_unsigned(true), m_intValue(static_cast<int32>(value)) {}
        Value(int32 value) : m_type(NVT_Integer), m_unsigned(false), m_intValue(value) {}
        Value(uint64 value) : m_type(NVT_Long), m_unsigned(true), m_longValue(static_cast<int64>(value)) {}
        Value(int64 value) : m_type(NVT_Long), m_unsigned(false), m_longValue(value) {}
        Value(float value) : m_type(NVT_Float), m_floatValue(value) {}
        Value(double value) : m_type(NVT_Double), m_doubleValue(value) {}
        
        Value& operator=(bool value) {
            m_type = NVT_Bool;
            m_boolValue = value;
            return *this;
        }
        Value& operator=(uint8 value) {
            m_type = NVT_Byte;
            m_byteValue = value;
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int8 value) {
            m_type = NVT_Byte;
            m_byteValue = reinterpret_cast<uint8>(value);
            m_unsigned = false;
            return *this;
        }
        Value& operator=(uint16 value) {
            m_type = NVT_Short;
            m_shortValue = reinterpret_cast<int16>(value);
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int16 value) {
            m_type = NVT_Short;
            m_shortValue = value;
            m_unsigned = false;
            return *this;
        }
        Value& operator=(uint32 value) {
            m_type = NVT_Integer;
            m_intValue = reinterpret_cast<int32>(value);
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int32 value) {
            m_type = NVT_Integer;
            m_intValue = value;
            m_unsigned = false;
            return *this;
        }
        Value& operator=(uint64 value) {
            m_type = NVT_Long;
            m_longValue = reinterpret_cast<int64>(value);
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int64 value) {
            m_type = NVT_Long;
            m_longValue = value;
            m_unsigned = false;
            return *this;
        }
        Value& operator=(float value) {
            m_type = NVT_Float;
            m_floatValue = value;
            return *this;
        }
        Value& operator=(double value) {
            m_type = NVT_Double;
            m_doubleValue = value;
            return *this;
        }
        
        neuroValueType type() const { return m_type; }
        bool isNumeric() const { return m_type != NVT_Object && m_type != NVT_Undefined; }
        bool isInteger() const { return m_type >= NVT_Bool && m_type <= NVT_Long; }
        bool isDecimal() const { return m_type == NVT_Float || m_type == NVT_Double; }
        bool isUnsigned() const { return m_unsigned; }
        bool isObject() const { return m_type == NVT_Object; }
        
        operator bool() const { return m_boolValue; }
        operator uint8() const { return (uint8)m_byteValue; }
        operator int8() const { return (int8)m_byteValue; }
        operator uint16() const { return (uint16)m_shortValue; }
        operator int16() const { return (int16)m_shortValue; }
        operator uint32() const { return (uint32)m_intValue; }
        operator int32() const { return (int32)m_intValue; }
        operator uint64() const { return (uint64)m_longValue; }
        operator int64() const { return (int64)m_longValue; }
        operator float() const { return (float)m_floatValue; }
        operator double() const { return (double)m_doubleValue; }
        operator ObjectWrapper() const { return m_objectValue; }
    };
}
