////////////////////////////////////////////////////////////////////////////////
// A stack-allocated wrapper around the various types of primitives or an object
// pointer.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include "Assert.hpp"
#include "DLLDecl.h"
#include "Error.hpp"
#include "NeuroTypes.h"

#include "GC/ManagedMemoryPointer.hpp"

namespace Neuro
{
    namespace Runtime {
        // Forward-declare the Object class as we're using only a pointer to it here,
        // but the object class uses the Value class directly.
        class Object;
    }
    
    /**
     * A "typeless" wrapper around an arbitrary value in the Neuro Runtime.
     * The value may be any of the primitives (boolean, integer, float / double)
     * or an object wrapper, which in turn wraps around a managed Neuro Object,
     * or an arbitrary native object.
     */
    class NEURO_API Value {
        using Pointer = Runtime::GC::ManagedMemoryPointer<Runtime::Object>;
        
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
            Pointer m_objectValue;
            void* m_ptrValue;
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
        Value(Pointer obj) : m_type(NVT_Object), m_objectValue(obj) {}
        Value(void* ptr) : m_type(NVT_NativeObject), m_ptrValue(ptr) {}
        Value(const Value& other) { *this = other; }
        ~Value() {
            clearManagedPointer();
        }
        
        Value& operator=(bool value) {
            clearManagedPointer();
            m_type = NVT_Bool;
            m_boolValue = value;
            return *this;
        }
        Value& operator=(uint8 value) {
            clearManagedPointer();
            m_type = NVT_Byte;
            m_byteValue = value;
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int8 value) {
            clearManagedPointer();
            m_type = NVT_Byte;
            m_byteValue = static_cast<uint8>(value);
            m_unsigned = false;
            return *this;
        }
        Value& operator=(uint16 value) {
            clearManagedPointer();
            m_type = NVT_Short;
            m_shortValue = static_cast<int16>(value);
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int16 value) {
            clearManagedPointer();
            m_type = NVT_Short;
            m_shortValue = value;
            m_unsigned = false;
            return *this;
        }
        Value& operator=(uint32 value) {
            clearManagedPointer();
            m_type = NVT_Integer;
            m_intValue = static_cast<int32>(value);
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int32 value) {
            clearManagedPointer();
            m_type = NVT_Integer;
            m_intValue = value;
            m_unsigned = false;
            return *this;
        }
        Value& operator=(uint64 value) {
            clearManagedPointer();
            m_type = NVT_Long;
            m_longValue = static_cast<int64>(value);
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int64 value) {
            clearManagedPointer();
            m_type = NVT_Long;
            m_longValue = value;
            m_unsigned = false;
            return *this;
        }
        Value& operator=(float value) {
            clearManagedPointer();
            m_type = NVT_Float;
            m_floatValue = value;
            return *this;
        }
        Value& operator=(double value) {
            clearManagedPointer();
            m_type = NVT_Double;
            m_doubleValue = value;
            return *this;
        }
        Value& operator=(Pointer obj) {
            clearManagedPointer();
            m_type = NVT_Object;
            m_objectValue = obj;
            return *this;
        }
        Value& operator=(void* ptr) {
            clearManagedPointer();
            throw NotSupportedError::instance();
            m_type = NVT_NativeObject;
            m_ptrValue = ptr;
            return *this;
        }
        Value& operator=(const Value& other) {
            clearManagedPointer();
            switch (other.m_type) {
            default:
                m_type = NVT_Undefined;
                Assert::fail("Unknown value type");
                break;
            case NVT_Bool:
                m_type = NVT_Bool;
                m_boolValue = other.getBool();
                break;
            case NVT_Byte:
                m_type = NVT_Byte;
                m_byteValue = other.isUnsigned() ? other.getUByte() : other.getByte();
                break;
            case NVT_Short:
                m_type = NVT_Short;
                m_shortValue = other.isUnsigned() ? other.getUShort() : other.getShort();
                break;
            case NVT_Integer:
                m_type = NVT_Integer;
                m_intValue = other.isUnsigned() ? other.getUInt() : other.getInt();
                break;
            case NVT_Long:
                m_type = NVT_Long;
                m_longValue = other.isUnsigned() ? other.getULong() : other.getLong();
                break;
            case NVT_Float:
                m_type = NVT_Float;
                m_floatValue = other.getFloat();
                break;
            case NVT_Double:
                m_type = NVT_Double;
                m_doubleValue = other.getDouble();
                break;
            case NVT_NativeObject:
                m_type = NVT_NativeObject;
                m_objectValue = other.getManagedObject();
                break;
            case NVT_Object:
                m_type = NVT_Object;
                m_ptrValue = other.getNativeObject();
                break;
            }
            return *this;
        }
        
        neuroValueType type() const { return m_type; }
        bool isUndefined() const { return m_type == NVT_Undefined; }
        bool isNumeric() const { return m_type != NVT_Object && m_type != NVT_Undefined; }
        bool isInteger() const { return m_type >= NVT_Bool && m_type <= NVT_Long; }
        bool isDecimal() const { return m_type == NVT_Float || m_type == NVT_Double; }
        bool isUnsigned() const { return m_unsigned; }
        bool isObject() const { return m_type == NVT_Object || m_type == NVT_NativeObject; }
        bool isManagedObject() const { return m_type == NVT_Object; }
        bool isNativeObject() const { return m_type == NVT_NativeObject; }
        
        bool getBool() const { return m_boolValue; }
        uint8 getUByte() const { return (uint8)m_byteValue; }
        int8 getByte() const { return (int8)m_byteValue; }
        uint16 getUShort() const { return (uint16)m_shortValue; }
        int16 getShort() const { return (int16)m_shortValue; }
        uint32 getUInt() const { return (uint32)m_intValue; }
        int32 getInt() const { return (int32)m_intValue; }
        uint64 getULong() const { return (uint64)m_longValue; }
        int64 getLong() const { return (int64)m_longValue; }
        float getFloat() const { return (float)m_floatValue; }
        double getDouble() const { return (double)m_doubleValue; }
        Pointer getManagedObject() const { return m_objectValue; }
        void* getNativeObject() const { return m_ptrValue; }
        
        Value& clear() {
            clearManagedPointer();
            m_type = NVT_Undefined;
            return *this;
        }
        
    protected:
        void clearManagedPointer() {
            if (m_type == NVT_Object) {
                m_objectValue.clear();
            }
        }
        
    public:
        static const Value undefined;
    };
}
