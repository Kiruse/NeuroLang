////////////////////////////////////////////////////////////////////////////////
// Managed, classless object of the Neuro Runtime.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <atomic>
#include <utility>

#include "DLLDecl.h"
#include "Delegate.hpp"
#include "NeuroTypes.h"
#include "NeuroIdentifier.hpp"
#include "NeuroValue.hpp"

#include "GC/ManagedMemoryPointer.hpp"

namespace Neuro {
    namespace Runtime
    {
        /**
         * A property is a simple hash map pair of Identifiers and Values.
         */
        using Property = std::pair<std::atomic<decltype(Identifier::number)>, Value>;
        
        class Object;
        
        /**
         * Since we have Neuro Objects, we may as well have Neuro Pointers. ;)
         */
        using Pointer = ManagedMemoryPointer<Object>;
        
        
        /**
         * Managed, classless, generic object.
         */
        class NEURO_API Object {
            friend class ::Neuro::Runtime::GC;
            
        public: // Types
            using CreateObjectDelegate = Delegate<Pointer, uint32>;
            using RecreateObjectDelegate = Delegate<Pointer, Pointer, uint32>;
            using RootObjectDelegate = Delegate<void, Object*>;
            using UnrootObjectDelegate = Delegate<void, Object*>;
            using AllocationDelegate = Delegate<ManagedMemoryPointerBase, uint32>;
            
            template<bool Mutable>
            class PropertyIterator {
            public:
                using object_type = toggle_const_t<!Mutable, Object>;
                using difference_type = uint32;
                using value_type = toggle_const_t<!Mutable, Property>;
                using pointer = value_type*;
                using reference = value_type&;
                using iterator_category = std::bidirectional_iterator_tag;
                
            private:
                object_type* inst;
                uint32 index;
                
            public:
                PropertyIterator(object_type* object = nullptr, uint32 index = 0) : inst(object), index(index) {}
                
                bool operator==(const PropertyIterator& other) const { return inst == other.inst && (index == other.index || (index >= inst->capacity() && other.index >= inst->capacity())); }
                bool operator!=(const PropertyIterator& other) const { return !(*this == other); }
                operator bool() const { return inst && index < inst.capacity(); }
                
                PropertyIterator& operator++() {
                    while (!inst->props[++index].first && index < inst->capacity());
                    return *this;
                }
                PropertyIterator operator++(int) {
                    PropertyIterator copy(*this);
                    ++*this;
                    return *this;
                }
                PropertyIterator& operator--() {
                    while (!inst->props[--index].first && index < inst->capacity());
                    return *this;
                }
                PropertyIterator operator--(int) {
                    PropertyIterator copy(*this);
                    --*this;
                    return *this;
                }
                
                reference operator*() const { return inst->props[index]; }
                pointer operator->() const { return inst->props + index; }
            };
            
            using MutablePropertyIterator = PropertyIterator<true>;
            using ImmutablePropertyIterator = PropertyIterator<false>;
            
        private: // Properties
            /**
             * Permanent managed memory pointer to this instance.
             */
            Pointer self;
            
            /**
             * Pointer to the beginning of this object's property map.
             */
            Property* props;
            
            /**
             * Maximum number of properties this object can hold.
             */
            uint32 propCount;
            
            
        private: // RAII
            // Constructor letting us know how many properties we have.
            // We already know where our properties are: right behind us.
            Object(Pointer self, uint32 propCount);
            
            // Special move constructor receiving self pointer and pointer to object to copy from.
            Object(Pointer self, Pointer other);
            
            // Special move constructor with different property map size.
            Object(Pointer self, Pointer other, uint32 newPropCount);
            
            // Objects must receive their self pointer, hence we cannot use copy and move constructors...
            Object(const Object&) = delete;
            Object(Object&&) = delete;
            
            // nor copy and move assignments.
            Object& operator=(const Object&) = delete;
            Object& operator=(Object&&) = delete;
            
            // Destruction is reserved to the Garbage Collector!
            ~Object();
            
        public: // Methods
            /**
             * Get the (mutable) value of this object's property identified by
             * the Identifier.
             * If the property has not been found, it will be created and assume
             * the default value "undefined".
             */
            Value& getProperty(Identifier id);
            
            /**
             * Get the (mutable) value of this object's property by name.
             * If the property has not been found, it will be created and assume
             * the default value "undefined".
             * 
             * Because this uses Identifier::lookup every time, it's likely more
             * efficient to use getProperty(Identifier) directly.
             */
            Value& getProperty(const String& name) {
                return getProperty(Identifier::lookup(name));
            }
            
            /**
             * Get the (immutable) value of this object's property identified by
             * the Identifier.
             * Unlike the non-const version of this function, if the property
             * was not found, it will not be created. Instead, a reference to
             * Value::undefined is returned.
             */
            const Value& getProperty(Identifier id) const;
            
            /**
             * Get the (immutable) value of this object's property identified by
             * the Identifier.
             * Unlike the non-const version of this function, if the property
             * was not found, it will not be created. Instead, a reference to
             * Value::undefined is returned.
             * 
             * Because this uses Identifier::lookup every time, it's likely more
             * efficient to use getProperty(Identifier) directly.
             */
            const Value& getProperty(const String& name) const {
                return getProperty(Identifier::lookup(name));
            }
            
            /**
             * Admittingly rather useless alias for `getProperty()`.
             */
            Value& operator[](Identifier id) { return getProperty(id); }
            
            /**
             * Admittingly rather useless alias for `getProperty() const`.
             */
            const Value& operator[](Identifier id) const { return getProperty(id); }
            
            /**
             * Add this object to the GC root.
             */
            void root(); // See NeuroGC.cpp
            
            /**
             * Remove this object from the GC root.
             */
            void unroot(); // See NeuroGC.cpp
            
            
            /**
             * Counts all existing properties.
             */
            uint32 length() const;
            
            /**
             * Retrieves the maximum number of properties this object can hold.
             */
            uint32 capacity() const { return propCount; }
            
            /**
             * Gets managed pointer to self.
             */
            Pointer getPointer() const { return self; }
            
        private: // Internal helpers
            /**
             * Simple case of special move construction where the number of
             * properties is the same. Properties are copied over bytewise.
             */
            void copyProps(Pointer other);
            
            /**
             * Copies the properties of the other object whilst rehashing them.
             * Usually conducted during special move construction where the
             * number of properties of both objects differs.
             */
            void copyRehashProps(Pointer other);
            
            
        public: // Iterators
            ImmutablePropertyIterator cbegin() const { return ImmutablePropertyIterator(this, 0); }
            MutablePropertyIterator begin() { return MutablePropertyIterator(this, 0); }
            auto begin() const { return cbegin(); }
            ImmutablePropertyIterator cend() const { return ImmutablePropertyIterator(this, propCount); }
            MutablePropertyIterator end() { return MutablePropertyIterator(this, propCount); }
            auto end() const { return cend(); }
            
        public: // Events
            /**
             * Triggered when the Garbage Collector moves this object to a new
             * location. Designed to let native code react if necessary.
             */
            EventDelegate<Object*> onMove;
            
            /**
             * Triggered when the Garbage Collector destroys this object. The
             * object still exists at the time the delegates are called. Designed
             * to let native code react if necessary.
             */
            EventDelegate<> onDestroy;
            
        public: // External dependencies (delegates & statics)
            
            /**
             * Resets the delegate used by the createObject static method to its
             * default.
             */
            static void useDefaultCreateObjectDelegate();
            
            /**
             * Resets the delegate used by the recreateObject static method to
             * its default.
             */
            static void useDefaultRecreateObjectDelegate();
            
            /**
             * Resets the delegate used by the root method to its default.
             */
            static void useDefaultRootObjectDelegate();
            
            /**
             * Resets the delegate used by the unroot method to its default.
             */
            static void useDefaultUnrootObjectDelegate();
            
            /**
             * Sets the delegate used by the createObject static method.
             */
            static void setCreateObjectDelegate(const CreateObjectDelegate& deleg);
            
            /**
             * Sets the delegate used by the recreateObject static method.
             */
            static void setRecreateObjectDelegate(const RecreateObjectDelegate& deleg);
            
            /**
             * Sets the delegate used by the root method.
             */
            static void setRootDelegate(const RootObjectDelegate& deleg);
            
            /**
             * Sets the delegate used by the unroot method.
             */
            static void setUnrootDelegate(const UnrootObjectDelegate& deleg);
            
            /**
             * Create an entirely new object using an arbitrary algorithm
             * determined by `useCreateObjectDelegate`.
             */
            static Pointer createObject(uint32 knownPropsCount);
            
            /**
             * Create a new managed, generic object. Manipulate the object
             * through the class directly.
             */
            static Pointer defaultCreateObject(uint32 knownPropsCount);
            
            /**
             * Create a new generic object in a custom memory region as
             * determined by the allocation delegate.
             * 
             * Useful to slightly alter the behavior of the defaultCreateObject
             * method.
             */
            static Pointer genericCreateObject(const AllocationDelegate& allocDeleg, uint32 knownPropsCount);
            
            /**
             * Recreate an object based on another, resizing its property map
             * in the process. Uses an arbitrary algorithm determined by
             * `useRecreateObjectDelegate`.
             */
            static Pointer recreateObject(Pointer object, uint32 knownPropsCount);
            
            /**
             * Create a new managed, generic object from the given object, down-
             * or upsizing its property map.
             */
            static Pointer defaultRecreateObject(Pointer object, uint32 knownPropsCount);
            
            /**
             * Creates a new object from the given object in a custom memory
             * region as determined by the allocation delegate.
             * 
             * Useful to slightly alter the behavior of the defaultRecreateObject
             * method.
             */
            static Pointer genericRecreateObject(const AllocationDelegate& allocDeleg, Pointer object, uint32 knownPropsCount);
        };
    }
}
