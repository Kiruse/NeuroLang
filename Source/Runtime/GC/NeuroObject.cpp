////////////////////////////////////////////////////////////////////////////////
// Definition of our more complex types.
// -----
// TODO: Read n properties per thread for large property maps
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include <algorithm>
#include <cstring>
#include <mutex>

#include "NeuroObject.hpp"
#include "GC/NeuroGC.hpp"

namespace Neuro {
    namespace Runtime
    {
        ////////////////////////////////////////////////////////////////////////
        // Local forward declarations
        ////////////////////////////////////////////////////////////////////////
        
        void rootObjectInternal(Object* inst);
        void unrootObjectInternal(Object* inst);
        
        
        ////////////////////////////////////////////////////////////////////////
        // Helpers
        ////////////////////////////////////////////////////////////////////////
        
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
        
        
        ////////////////////////////////////////////////////////////////////////
        // RAII
        ////////////////////////////////////////////////////////////////////////
        
        Object::Object(Pointer self, uint32 propCount) : propertyWriteMutex(), self(self), props(getPropertyMapAddress(this)), propCount(propCount) {
            initProps();
        }
        
        Object::Object(Pointer self, Object* other) : propertyWriteMutex(), self(self), props(getPropertyMapAddress(this)), propCount(other->propCount) {
            copyProps(other);
            onMove(other);
        }
        
        Object::Object(Pointer self, Object* other, uint32 newPropCount) : propertyWriteMutex(), self(self), props(getPropertyMapAddress(this)), propCount(newPropCount) {
            initProps();
            copyRehashProps(other);
            onMove(other);
        }
        
        Object::~Object() {
            onDestroy();
            
            // Clearing every single property triggers heuristics for gc scans
            for (uint32 i = 0; i < propCount; ++i) {
                props[i].value.clear();
            }
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Methods
        ////////////////////////////////////////////////////////////////////////
        
        Value& Object::getProperty(Identifier id) {
            Property* prop = getConstProp(id);
            if (prop) return prop->value;
            decltype(id.getUID()) number = id.getUID();
            
            std::lock_guard{propertyWriteMutex};
            const uint32 cap = capacity();
            
            // Find property based on hash index
            for (uint8 i = 0; i < 8; ++i) {
                prop = props + (cycleBits(number, propCount * i) % cap);
                if (prop->id == -1) {
                    prop->id = number;
                    return prop->value;
                }
            }
            
            // Find property by iterating linearly
            for (uint32 i = 0; i < length(); ++i) {
                prop = props + i;
                if (prop->id == -1) {
                    prop->id = number;
                    return prop->value;
                }
            }
            
            // Expand property map
            Pointer newObj = recreateObject(self, propCount + 1);
            return newObj->getProperty(id);
        }
        
        const Value& Object::getProperty(Identifier id) const {
            Property* prop = getConstProp(id);
            if (!prop) return Value::undefined;
            return prop->value;
        }
        
        uint32 Object::length() const {
            uint32 count = 0;
            for (uint32 i = 0; i < propCount; ++i) {
                if (props[i].id != -1) ++count;
            }
            return count;
        }
        
        void Object::initProps() {
            for (uint32 i = 0; i < propCount; ++i) {
                props[i].id = -1;
                props[i].value = Value::undefined;
            }
        }
        
        void Object::copyProps(Object* other) {
            std::memcpy(props, other->props, propCount * sizeof(Property));
        }
        
        void Object::copyRehashProps(Object* other) {
			const uint32 max = capacity();
            uint32 added = 0;
            for (auto& prop : *other) {
                if (added++ >= max) break;
                getProperty(Identifier::fromUID(prop.id)) = prop.value;
            }
        }
        
        Property* Object::getConstProp(Identifier id) const {
            const auto number = id.getUID();
            const uint32 cap = capacity();
            
            // Try to find 8 distinct positions based on ID
            for (uint8 i = 0; i < 8; ++i) {
                auto& prop = props[cycleBits(number, propCount * i) % cap];
                if (prop.id == number) return &prop;
            }
            
            // Try to find the property by linearly iterating over the map!
            for (uint32 start = number % propCount, index = start + 1; index != start; index = (index + 1) % propCount) {
                if (props[index].id == number) return props + index;
            }
            
            return nullptr;
        }
        
        
        void Object::root() {
            GC::instance()->root(self);
        }
        
        void Object::unroot() {
            GC::instance()->unroot(self);
        }
        
        
        Pointer Object::createObject(uint32 propsCount, uint32 propsSlack) {
            // TODO: Determine algorithm for prop buffer count
            const uint32 totalPropsCount = propsCount + propsSlack;
            
            auto rawptr = GC::instance()->allocateTrivial(sizeof(Object) + sizeof(Property) * totalPropsCount, 1);
            if (!rawptr) return Pointer();
            
            Pointer self(rawptr);
            new (self.get()) Object(self, totalPropsCount);
            return self;
        }
        
        Pointer Object::recreateObject(Pointer object, uint32 propsCount, uint32 propsSlack) {
            // TODO: More dynamic algorithm for property map upsizing
            const uint32 totalPropsCount = propsCount + propsSlack;
			auto oldptr = object.get();
            
            // Only recreate if we're actually resizing!
            if (totalPropsCount == object->propCount) return object;
            
            Error err = GC::instance()->reallocate(object, sizeof(Object) + sizeof(Property) * totalPropsCount, 1, false);
            if (err) return Pointer();
            
			auto newptr = object.get();
            
            new (object.get()) Object(object, oldptr, totalPropsCount);
            return object;
        }
    }
}
