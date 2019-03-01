////////////////////////////////////////////////////////////////////////////////
// Managed, classless object of the Neuro Runtime.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#pragma warning(push)
#pragma warning(disable: 4251)

#include <mutex>
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
        struct Property {
            decltype(Identifier::number) id;
            Value value;
        };
        
        class Object;
        
        /**
         * Since we have Neuro Objects, we may as well have Neuro Pointers. ;)
         */
        using Pointer = ManagedMemoryPointer<Object>;
        
        
        /**
         * Managed, classless, generic object.
         */
        class NEURO_API Object {
            friend class ::Neuro::Runtime::GCInterface;
            friend class ::Neuro::Runtime::GC;
            
        public:  // Types
            using AllocationDelegate = Delegate<ManagedMemoryPointerBase, uint32>;
            
            template<bool Mutable>
            class PropertyIterator {
            public:  // Types
                using object_type = toggle_const_t<!Mutable, Object>;
                using difference_type = uint32;
                using value_type = toggle_const_t<!Mutable, Property>;
                using pointer = value_type*;
                using reference = value_type&;
                using iterator_category = std::bidirectional_iterator_tag;
                
            private: // Fields
                object_type* inst;
                uint32 index;
                
            public:  // RAII
                PropertyIterator(object_type* object = nullptr, uint32 index = 0) : inst(object), index(index) {}
                
            public:  // Operators
                bool operator==(const PropertyIterator& other) const { return inst == other.inst && (index == other.index || (index >= inst->capacity() && other.index >= inst->capacity())); }
                bool operator!=(const PropertyIterator& other) const { return !(*this == other); }
                operator bool() const { return inst && index < inst.capacity(); }
                
                PropertyIterator& operator++() {
                    while (inst->props[++index].id == -1 && index < inst->capacity());
                    return *this;
                }
                PropertyIterator operator++(int) {
                    PropertyIterator copy(*this);
                    ++*this;
                    return *this;
                }
                PropertyIterator& operator--() {
                    while (inst->props[--index].id == -1 && index < inst->capacity());
                    return *this;
                }
                PropertyIterator operator--(int) {
                    PropertyIterator copy(*this);
                    --*this;
                    return *this;
                }
                
                reference operator*() const { return inst->props[index]; }
                pointer operator->() const { return inst->props + index; }
                
            public:  // Static Methods
                static PropertyIterator fromFirst(object_type* obj) {
                    for (uint32 i = 0; i < obj->capacity(); ++i) {
                        if (obj->props[i].id != -1) return PropertyIterator(obj, i);
                    }
                    return PropertyIterator(obj, obj->capacity());
                }
                static PropertyIterator fromLast(object_type* obj) {
                    for (uint32 i = obj->capacity() - 1; i < obj->capacity(); --i) {
                        if (obj->props[i].id != -1) return PropertyIterator(obj, i);
                    }
                    return PropertyIterator(obj, obj->capacity());
                }
            };
            
            using MutablePropertyIterator = PropertyIterator<true>;
            using ImmutablePropertyIterator = PropertyIterator<false>;
            
        private: // Fields
            std::mutex propertyWriteMutex;
            
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
            
            
        public:  // Delegates
            MulticastDelegate<void, Pointer> onMove;
            MulticastDelegate<void> onDestroy;
            
            
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
            
        public:  // Methods
            bool hasProperty(Identifier id) const {
                return !!getConstProp(id);
            }
            
            bool hasProperty(const String& name) const {
                return hasProperty(Identifier::lookup(name));
            }
            
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
            void root();
            
            /**
             * Remove this object from the GC root.
             */
            void unroot();
            
            
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
             * Initializes the entire property map, which kinda sucks...
             */
            void initProps();
            
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
            
            /**
             * Only gets an existing property by ID.
             */
            Property* getConstProp(Identifier id) const;
            
            
        public:  // Iterators
            ImmutablePropertyIterator cbegin() const { return ImmutablePropertyIterator::fromFirst(this); }
            MutablePropertyIterator begin() { return MutablePropertyIterator::fromFirst(this); }
            auto begin() const { return cbegin(); }
            ImmutablePropertyIterator cend() const { return ImmutablePropertyIterator::fromLast(this); }
            MutablePropertyIterator end() { return MutablePropertyIterator::fromLast(this); }
            auto end() const { return cend(); }
            
        public:  // External dependencies (delegates & statics)
            /**
             * Create an entirely new object using an arbitrary algorithm
             * determined by `useCreateObjectDelegate`.
             */
            static Pointer createObject(uint32 propsCount, uint32 propsSlack = 10);
            
            /**
             * Recreate an object based on another, resizing its property map
             * in the process. Uses an arbitrary algorithm determined by
             * `useRecreateObjectDelegate`.
             */
            static Pointer recreateObject(Pointer object, uint32 propsCount, uint32 propsSlack = 10);
        };
    }
}

#pragma warning(pop)
