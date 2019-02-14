////////////////////////////////////////////////////////////////////////////////
// A stack-allocated wrapper around the various types of primitives or an object
// pointer.
// -----
// NOTE:
// There's a lot of unnecessary code here because VS15 is still not standard
// conform. Hopefully this changes with VS16.
// 
// In particular, all code handling the various integer types (except assignment
// operators) are redundant, as a standard compliant compiler should be able to
// resolve integer types correctly to the long-overload, and not simply complain
// about an ambiguous call. Tsk.
// 
// A template doesn't work either in many instances below because templates are
// weird when it comes to operators.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <limits>
#include <type_traits>

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
        using Pointer = Runtime::ManagedMemoryPointer<Runtime::Object>;
        
        neuroValueType m_type;
        bool m_unsigned;
        union {
            int64 m_longValue;
            double m_doubleValue;
            Pointer m_objectValue;
            void* m_ptrValue;
        };
        
    public:
        Value() : m_type(NVT_Undefined) {}
        Value(bool value) : m_type(NVT_Bool), m_longValue(value) {}
        Value(uint8 value) : m_type(NVT_Byte), m_unsigned(true), m_longValue(value) {}
        Value(int8 value) : m_type(NVT_Byte), m_unsigned(false), m_longValue(value) {}
        Value(uint16 value) : m_type(NVT_Short), m_unsigned(true), m_longValue(value) {}
        Value(int16 value) : m_type(NVT_Short), m_unsigned(false), m_longValue(value) {}
        Value(uint32 value) : m_type(NVT_Integer), m_unsigned(true), m_longValue(value) {}
        Value(int32 value) : m_type(NVT_Integer), m_unsigned(false), m_longValue(value) {}
        Value(uint64 value) : m_type(NVT_Long), m_unsigned(true) {
            *reinterpret_cast<uint64*>(m_longValue) = value;
        }
        Value(int64 value) : m_type(NVT_Long), m_unsigned(false), m_longValue(value) {}
        Value(float value) : m_type(NVT_Float), m_doubleValue(value) {}
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
            m_longValue = value;
            return *this;
        }
        Value& operator=(uint8 value) {
            clearManagedPointer();
            m_type = NVT_Byte;
            m_longValue = value;
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int8 value) {
            clearManagedPointer();
            m_type = NVT_Byte;
            m_longValue = value;
            m_unsigned = false;
            return *this;
        }
        Value& operator=(uint16 value) {
            clearManagedPointer();
            m_type = NVT_Short;
            m_longValue = value;
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int16 value) {
            clearManagedPointer();
            m_type = NVT_Short;
            m_longValue = value;
            m_unsigned = false;
            return *this;
        }
        Value& operator=(uint32 value) {
            clearManagedPointer();
            m_type = NVT_Integer;
            m_longValue = value;
            m_unsigned = true;
            return *this;
        }
        Value& operator=(int32 value) {
            clearManagedPointer();
            m_type = NVT_Integer;
            m_longValue = value;
            m_unsigned = false;
            return *this;
        }
        Value& operator=(uint64 value) {
            clearManagedPointer();
            m_type = NVT_Long;
            *reinterpret_cast<uint64*>(m_longValue) = value;
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
            m_doubleValue = value;
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

			m_type = other.type();
			m_unsigned = other.isUnsigned();
            
			switch (other.m_type) {
            default:
                m_type = NVT_Undefined;
                break;
            case NVT_Bool:
                m_longValue = other.getBool();
                break;
            case NVT_Byte:
                m_longValue = other.isUnsigned() ? other.getUByte() : other.getByte();
                break;
            case NVT_Short:
                m_longValue = other.isUnsigned() ? other.getUShort() : other.getShort();
                break;
            case NVT_Integer:
                m_longValue = other.isUnsigned() ? other.getUInt() : other.getInt();
                break;
            case NVT_Long:
                m_longValue = other.isUnsigned() ? other.getULong() : other.getLong();
                break;
            case NVT_Float:
                m_doubleValue = other.getFloat();
                break;
            case NVT_Double:
                m_doubleValue = other.getDouble();
                break;
            case NVT_NativeObject:
                m_objectValue = other.getManagedObject();
                break;
            case NVT_Object:
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
        
        bool getBool() const { return !!m_longValue; }
        uint8 getUByte() const { return (uint8)m_longValue; }
        int8 getByte() const { return (int8)m_longValue; }
        uint16 getUShort() const { return (uint16)m_longValue; }
        int16 getShort() const { return (int16)m_longValue; }
        uint32 getUInt() const { return (uint32)m_longValue; }
        int32 getInt() const { return (int32)m_longValue; }
        uint64 getULong() const { return *reinterpret_cast<uint64*>(m_longValue); }
        int64 getLong() const { return m_longValue; }
        float getFloat() const { return static_cast<float>(m_doubleValue); }
        double getDouble() const { return m_doubleValue; }
        Pointer getManagedObject() const { return m_objectValue; }
        void* getNativeObject() const { return m_ptrValue; }
        
		operator bool() const {
			if (isNumeric()) {
				if (isInteger()) {
					return getLong();
				}
				return getDouble() != 0;
			}
			if (isManagedObject()) {
				return getManagedObject();
			}
			if (isNativeObject()) {
				return getNativeObject();
			}
			return false;
		}
		bool operator==(bool value) const {
			return (bool)*this == value;
		}
		bool operator==(uint8 value) const {
            return isInteger() && isUnsigned() && getULong() == value;
		}
        bool operator==(int8 value) const {
            return isInteger() && !isUnsigned() && getLong() == value;
        }
        bool operator==(uint16 value) const {
            return isInteger() && isUnsigned() && getULong() == value;
        }
        bool operator==(int16 value) const {
            return isInteger() && !isUnsigned() && getLong() == value;
        }
        bool operator==(uint32 value) const {
            return isInteger() && isUnsigned() && getULong() == value;
        }
        bool operator==(int32 value) const {
            return isInteger() && !isUnsigned() && getLong() == value;
        }
        bool operator==(uint64 value) const {
            return isInteger() && isUnsigned() && getULong() == value;
        }
        bool operator==(int64 value) const {
            return isInteger() && !isUnsigned() && getLong() == value;
        }
        bool operator==(double value) const {
            return isDecimal() && getDouble() == value;
        }
        bool operator==(Pointer value) const {
            return isManagedObject() && getManagedObject() == value;
        }
        bool operator==(void* value) const {
            return isNativeObject() && getNativeObject() == value;
        }
        template<typename T>
        bool operator!=(T value) const {
            return !(*this == value);
        }
        
        Value& clear() {
            clearManagedPointer();
            m_type = NVT_Undefined;
            return *this;
        }
        
    protected:
        void clearManagedPointer() {
            // This used to be relevant
            // if (m_type == NVT_Object) {
            //     m_objectValue.clear();
            // }
        }
        
    public:
        static const Value undefined;
    };
}
