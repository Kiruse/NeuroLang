////////////////////////////////////////////////////////////////////////////////
// Definition of our more complex types.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include <algorithm>
#include <cstring>

#include "NeuroObject.hpp"

#include "GC/ManagedMemoryOverhead.hpp"

namespace Neuro {
    namespace Runtime
    {
        uint32 cycleBits(uint32 number, uint32 bits) {
            static constexpr uint32 uint32bits = sizeof(uint32) * 8;
            
            if (!bits) return number;
            
            // Skip full roundabouts.
            bits = bits % uint32bits;
            
            // Assemble the final number from two bit shifts.
            return (number << bits) | (number >> (uint32bits - bits));
        }
        
        
        Property* getPropertyMapAddress(Object* ptr) {
            return reinterpret_cast<Property*>(reinterpret_cast<uint8*>(ptr) + sizeof(Object));
        }
        
        
        Object::Object(uint32 propCount) : props(getPropertyMapAddress(this)), propCount(propCount) {
            // Works because neuroValueType::Undefined == 0.
            std::memset(props, 0, sizeof(Property) * propCount);
        }
        
        Object::Object(Pointer other, uint32 propCount) : props(getPropertyMapAddress(this)), propCount(propCount) {
            const uint32 count = std::max<uint32>(other->length(), propCount);
            uint32 added = 0;
            for (auto& prop : *other) {
                if (added++ >= count) break;
                getProperty(Identifier::fromUID(prop.first)) = prop.second;
            }
        }
        
        Object::Object(const Object& other) : props(getPropertyMapAddress(this)), propCount(other.propCount) {
            // Copying is not quite as trivial as just setting bytes, as pointers
            // to managed objects need to be resolved in case they have been
            // relocated.
            for (uint32 i = 0; i < propCount; ++i) {
                props[i].first.store(other.props[i].first.load());
                props[i].second = other.props[i].second;
            }
        }
        
        Object::Object(Object&& other) : props(getPropertyMapAddress(this)), propCount(other.propCount) {
            // Moving like copying is not quite as trivial as moving bytes,
            // because pointers to managed objects need to be resolved still.
            for (uint32 i = 0; i < propCount; ++i) {
                props[i].first.store(other.props[i].first.load());
                props[i].second = other.props[i].second;
            }
            
            // Call the move delegate for native code to react to.
            onMove(&other);
        }
        
        Object::~Object() {
            // Call the destruction delegate before clearing the properties so
            // native code may react to the state of the object prior to finalization.
            onDestroy();
            
            // Must clear each individual property to trigger scanning released
            // object references where applicable.
            for (uint32 i = 0; i < propCount; ++i) {
                props[i].second.clear();
            }
        }
        
        Value& Object::getProperty(Identifier id) {
            typedef decltype(Identifier::number) idnum;
            const idnum number = id.getUID();
            idnum cmp = 0;
            
            for (uint8 i = 0; i < 8; ++i) {
                auto& prop = props[cycleBits(number, propCount * i)];
                if (prop.first == number) return prop.second;
            }
            
            // First try to find the property, remembering the first unoccupied property we encounter for later...
            Property* available = nullptr;
            for (uint32 start = number % propCount, index = start + 1; index != start; index = (index + 1) % propCount) {
                if (props[index].first == number) {
                    // If we had preemptively claimed a property, release it again.
                    if (available) available->first = 0;
                    return props[index].second;
                }
                
                // Preemptively atomically claim first free property and remember its address.
                if (!available) {
                    if (props[index].first.compare_exchange_strong(cmp, number)) {
                        available = props + index;
                    }
                }
            }
            if (available) return available->second;
            
            // TODO: Somehow make upsizing object property maps more dynamic.
            Pointer newObj = recreateObject(Pointer(GC::ManagedMemoryPointerBase::fromObject(this)), propCount + 1);
            return newObj->getProperty(id);
        }
        
        const Value& Object::getProperty(Identifier id) const {
            const auto number = id.getUID();
            
            for (uint8 i = 0; i < 8; ++i) {
                auto& prop = props[cycleBits(number, propCount * i)];
                if (prop.first == number) return prop.second;
            }
            
            // Try to find the property!
            for (uint32 start = number % propCount, index = start + 1; index != start; index = (index + 1) % propCount) {
                if (props[index].first == number) return props[index].second;
            }
            
            return Value::undefined;
        }
        
        uint32 Object::length() const {
            uint32 count = 0;
            for (uint32 i = 0; i < propCount; ++i) {
                if (props[i].first) ++count;
            }
            return count;
        }
    }
}
