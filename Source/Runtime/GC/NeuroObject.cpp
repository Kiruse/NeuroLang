////////////////////////////////////////////////////////////////////////////////
// Definition of our more complex types.
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
        // Imported GC globals
        
        namespace GCGlobals
        {
            extern std::mutex rootsMutex;
            extern Buffer<Pointer> roots;
        }
        
        
        ////////////////////////////////////////////////////////////////////////
        // Local forward declarations
        
        void rootObjectInternal(Object* inst);
        void unrootObjectInternal(Object* inst);
        
        
        ////////////////////////////////////////////////////////////////////////
        // Delegates
        
        SinglecastDelegate<Pointer, uint32>          createObjectDeleg   = Object::CreateObjectDelegate::FunctionDelegate<Object::defaultCreateObject>();
        SinglecastDelegate<Pointer, Pointer, uint32> recreateObjectDeleg = Object::RecreateObjectDelegate::FunctionDelegate<Object::defaultRecreateObject>();
        SinglecastDelegate<void, Object*>            rootDeleg           = Object::RootObjectDelegate::FunctionDelegate<rootObjectInternal>();
        SinglecastDelegate<void, Object*>            unrootDeleg         = Object::UnrootObjectDelegate::FunctionDelegate<unrootObjectInternal>();
        
        
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
        
        
        Object::Object(Pointer self, uint32 propCount) : self(self), props(getPropertyMapAddress(this)), propCount(propCount) {
            // Guaranteed to work because neuroValueType::Undefined == 0.
            std::memset(props, 0, sizeof(Property) * propCount);
        }
        
        Object::Object(Pointer self, Pointer other) : self(self), props(getPropertyMapAddress(this)), propCount(other->propCount) {
            copyProps(other);
        }
        
        Object::Object(Pointer self, Pointer other, uint32 newPropCount) : self(self), props(getPropertyMapAddress(this)), propCount(newPropCount) {
            copyRehashProps(other);
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
            Pointer newObj = recreateObject(self, propCount + 1);
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
        
        void Object::copyProps(Pointer other) {
            std::memcpy(props, other->props, propCount * sizeof(Property));
        }
        
        void Object::copyRehashProps(Pointer other) {
            const uint32 count = std::max<uint32>(other->length(), propCount);
            uint32 added = 0;
            for (auto& prop : *other) {
                if (added++ >= count) break;
                getProperty(Identifier::fromUID(prop.first)) = prop.second;
            }
        }
        
        
        void Object::root() {
            rootDeleg(this);
        }
        
        void rootObjectInternal(Object* inst) {
            std::scoped_lock{GCGlobals::rootsMutex};
            GCGlobals::roots.add(inst->getPointer());
        }
        
        void Object::unroot() {
            unrootDeleg(this);
        }
        
        void unrootObjectInternal(Object* inst) {
            std::scoped_lock{GCGlobals::rootsMutex};
            GCGlobals::roots.remove(inst->getPointer());
        }
        
        void Object::useDefaultCreateObjectDelegate() {
            createObjectDeleg = CreateObjectDelegate::FunctionDelegate<Object::defaultCreateObject>();
        }
        
        void Object::useDefaultRecreateObjectDelegate() {
            recreateObjectDeleg = RecreateObjectDelegate::FunctionDelegate<Object::defaultRecreateObject>();
        }
        
        void Object::useDefaultRootObjectDelegate() {
            rootDeleg = RootObjectDelegate::FunctionDelegate<rootObjectInternal>();
        }
        
        void Object::useDefaultUnrootObjectDelegate() {
            unrootDeleg = UnrootObjectDelegate::FunctionDelegate<unrootObjectInternal>();
        }
        
        void Object::setCreateObjectDelegate(const Object::CreateObjectDelegate& deleg) {
            createObjectDeleg = deleg;
        }
        
        void Object::setRecreateObjectDelegate(const Object::RecreateObjectDelegate& deleg) {
            recreateObjectDeleg = deleg;
        }
        
        void Object::setRootDelegate(const Object::RootObjectDelegate& deleg) {
            rootDeleg = deleg;
        }
        
        void Object::setUnrootDelegate(const Object::UnrootObjectDelegate& deleg) {
            unrootDeleg = deleg;
        }
        
        
        Pointer Object::createObject(uint32 knownPropsCount) {
            return createObjectDeleg(knownPropsCount);
        }
        
        Pointer Object::defaultCreateObject(uint32 knownPropsCount) {
            return genericCreateObject(AllocationDelegate::FunctionDelegate<GC::allocateTrivial>(), knownPropsCount);
        }
        
        Pointer Object::genericCreateObject(const AllocationDelegate& alloc, uint32 knownPropsCount) {
            // TODO: Determine algorithm for prop buffer count
            const uint32 totalPropsCount = knownPropsCount + 10;
            
            auto rawptr = alloc(sizeof(Object) + sizeof(Property) * totalPropsCount);
            if (!rawptr) return Pointer();
            
            Pointer self(rawptr);
            new (self.get()) Object(self, totalPropsCount);
            return self;
        }
        
        Pointer Object::recreateObject(Pointer object, uint32 knownPropsCount) {
            return recreateObjectDeleg(object, knownPropsCount);
        }
        
        Pointer Object::defaultRecreateObject(Pointer object, uint32 knownPropsCount) {
            return genericRecreateObject(AllocationDelegate::FunctionDelegate<GC::allocateTrivial>(), object, knownPropsCount);
        }
        
        Pointer Object::genericRecreateObject(const AllocationDelegate& alloc, Pointer object, uint32 knownPropsCount) {
            // TODO: Determine algorithm for prop buffer count
            const uint32 totalPropsCount = knownPropsCount + 10;
            
            // Only recreate if we're actually resizing!
            if (totalPropsCount == object->propCount) return object;
            
            auto rawptr = alloc(sizeof(Object) + sizeof(Property) * totalPropsCount);
            if (!rawptr) return Pointer();
            
            Pointer self(rawptr);
            new (self.get()) Object(self, object, totalPropsCount);
            return self;
        }
    }
}
