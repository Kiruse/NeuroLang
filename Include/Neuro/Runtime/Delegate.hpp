////////////////////////////////////////////////////////////////////////////////
// A brilliant little microlibrary recreating a special implementation of
// multicast delegates. The microlibrary has certain very strict rules, and
// custom extension is discouraged.
// 
//   Restriction #1)
// The Delegate specializations must fit into a fixed size for the Multicast
// Delegate container to function.
// 
//   Restriction #2)
// The Delegates use certain virtual functions, and thus must inherit from the
// base class.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <utility>
#include "Allocator.hpp"
#include "NeuroBuffer.hpp"

namespace Neuro
{
    /**
     * The very basis of the Delegates merely defining the max size of the
     * Delegates and the basic, untyped interface.
     * 
     * The interface must not be abstract as we create dummy objects based
     * directly off this class.
     */
    class NEURO_API DelegateBase {
        // Friended allocator to permit it constructing instances through the
        // default constructor.
        friend struct MulticastDelegateAllocator;
        friend struct RAIIHeapAllocator<DelegateBase>;
        
    protected:
        void* data;
        
    protected:
        DelegateBase() = default;
        
        // Creating a copy or moving data around requires explicit knowledge of
        // the exact specialization.
        DelegateBase(const DelegateBase&) = delete;
        DelegateBase(DelegateBase&&) = delete;
        DelegateBase& operator=(const DelegateBase&) = delete;
        DelegateBase& operator=(DelegateBase&&) = delete;
        
    public:
        virtual ~DelegateBase() = default;
        
        /**
         * Test whether this delegate instance is valid and callable.
         */
        virtual bool valid() const { return false; }
        operator bool() const { return valid(); }
        
        /**
         * Required for the Multicast Delegate to be able to remove a previously
         * added delegate from its invocation list.
         */
        virtual bool operator==(const DelegateBase& other) const { return false; }
        bool operator!=(const DelegateBase& other) const { return !(*this==other); }
        
        /**
         * Copy this delegate over to the target delegate. Used by the
         * MulticastDelegate in order to create appropriate copies.
         */
        virtual void copyTo(DelegateBase* target) const {}
        
        /**
         * Gets the data stored in this delegate. May not be valid!
         */
        void* getData() const { return data; }
    };
    
    /**
     * Defines the actual signature-related interface of the Delegates.
     */
    template<typename ReturnType, typename... Args>
    class NEURO_API Delegate : public DelegateBase {
    protected:
        Delegate() = default;
        
    public:
        /**
         * Call the delegate!
         */
        virtual ReturnType operator()(Args... args) const = 0;
        
        
        /**
         * The Delegate specialization for simple global function and static
         * method delegates.
         */
        template<ReturnType (*fp)(Args... args)>
        class NEURO_API FunctionDelegate : public Delegate {
        public:
            FunctionDelegate() = default;
            
            virtual ReturnType operator()(Args... args) const override {
                return fp(std::forward<Args>(args)...);
            }
            
            virtual bool valid() const override { return true; }
            
            virtual bool operator==(const DelegateBase& other) const override {
                // Equals if same runtime type ID.
                return typeid(other) == typeid(FunctionDelegate);
            }
            
            virtual void copyTo(DelegateBase* target) const override {
                new (target) FunctionDelegate();
            }
        };
        
        /**
         * The Delegate specialization for class methods.
         */
        template<typename T, ReturnType (T::*mp)(Args... args)>
        class NEURO_API MethodDelegate : public Delegate {
        public:
            MethodDelegate(T* obj) { data = obj; }
            MethodDelegate(const MethodDelegate& other) { data = other.data; }
            MethodDelegate& operator=(const MethodDelegate& other) { data = other.data; return *this; }
            
            virtual ReturnType operator()(Args... args) const override {
                return (reinterpret_cast<T*>(data)->*mp)(std::forward<Args>(args)...);
            }
            
            virtual bool operator==(const DelegateBase& other) const override {
                return typeid(other) == typeid(MethodDelegate) && data == other.getData();
            }
            
            virtual void copyTo(DelegateBase* target) const override {
                new (target) MethodDelegate(*this);
            }
        };
        
        /**
         * The Delegate specialization for 
         */
        template<typename Lambda>
        class NEURO_API LambdaDelegate : public Delegate {
        public:
            LambdaDelegate(Lambda& lambda) { data = reinterpret_cast<void*>(&lambda); }
            LambdaDelegate(const LambdaDelegate& other) { data = other.data; }
            LambdaDelegate& operator=(const LambdaDelegate& other) { data = other.data; return *this; }
            
            virtual ReturnType operator()(Args... args) const override {
                return (*reinterpret_cast<Lambda*>(data))(std::forward<Args>(args)...);
            }
            
            virtual bool operator==(const DelegateBase& other) const override {
                return typeid(other) == typeid(LambdaDelegate) && data == other.getData();
            }
            
            virtual void copyTo(DelegateBase* target) const override {
                new (target) LambdaDelegate(*this);
            }
        };
        
        /**
         * This is the way you really instantiate a LambdaDelegate, as there's
         * no way to tell the compiler what type the generated type is going to
         * be, duh.
         */
        template<typename Lambda>
        static LambdaDelegate<Lambda> fromLambda(Lambda& lambda) {
            return LambdaDelegate<Lambda>(lambda);
        }
    };
    
    /**
     * A wrapper around a single delegate for more convenient use.
     * 
     * The inconvenience about the regular Delegate is that the class has a pure
     * operator() which needs explicit knowledge of the exact implementation.
     * The MulticastDelegate circumvents this necessity through adequate amounts
     * of black magic to allow us to call delegates of any implementation. The
     * SinglecastDelegate makes use of the same black magic to allow us to call
     * and assign any specialization to a generic wrapper while using it like
     * any other delegate.
     */
    template<typename ReturnType, typename... Args>
    class NEURO_API SinglecastDelegate {
        uint8 buffer[sizeof(DelegateBase)];
        
    public:
        SinglecastDelegate(const Delegate<ReturnType, Args...>& deleg) {
            deleg.copyTo(reinterpret_cast<DelegateBase*>(&buffer));
        }
        SinglecastDelegate& operator=(const Delegate<ReturnType, Args...>& deleg) {
            deleg.copyTo(reinterpret_cast<DelegateBase*>(&buffer));
            return *this;
        }
        
        ReturnType operator()(Args... args) const {
            auto real = reinterpret_cast<const Delegate<ReturnType, Args...>*>(&buffer);
            return (*real)(std::forward<Args>(args)...);
        }
        
        Delegate<ReturnType, Args...>& getDelegate() {
            return *reinterpret_cast<Delegate<ReturnType, Args...>*>(&buffer);
        }
        const Delegate<ReturnType, Args...>& getDelegate() const {
            return *reinterpret_cast<const Delegate<ReturnType, Args...>*>(&buffer);
        }
        operator Delegate<ReturnType, Args...>&() {
            return getDelegate();
        }
        operator const Delegate<ReturnType, Args...>&() const {
            return getDelegate();
        }
    };
    
    /**
     * A specialized allocator specifically designed for the MulticastDelegate's
     * internal buffer.
     */
    struct NEURO_API MulticastDelegateAllocator {
        uint32 m_size;
        DelegateBase* m_data;
        
        MulticastDelegateAllocator() = default;
        MulticastDelegateAllocator(uint32 desiredSize) : m_size(0), m_data(nullptr) { resize(desiredSize); }
        MulticastDelegateAllocator(const MulticastDelegateAllocator& other) : m_size(0), m_data(nullptr) {
            resize(other.m_size);
            copy(0, other.m_data, other.m_size);
        }
        MulticastDelegateAllocator(MulticastDelegateAllocator&&) = default;
        MulticastDelegateAllocator& operator=(const MulticastDelegateAllocator& other) {
            if (m_data) std::free(m_data);
            m_data = nullptr;
            resize(other.m_size);
            copy(0, other.m_data, other.m_size);
            return *this;
        }
        MulticastDelegateAllocator& operator=(MulticastDelegateAllocator&&) = default;
        ~MulticastDelegateAllocator() {
            if (m_data) std::free(m_data);
            m_data = nullptr;
            m_size = 0;
        }
        
        void resize(uint32 desiredSize) {
            if (m_size != desiredSize || !m_data) {
                // Store the old data until we have a new buffer.
                DelegateBase* old = m_data;
                m_data = reinterpret_cast<DelegateBase*>(std::malloc(sizeof(DelegateBase) * desiredSize));
                
                // Copy old data over to the new buffer and free the old.
                if (old) {
                    copy(0, old, std::min(m_size, desiredSize));
                    delete[] old;
                }
                
                m_size = desiredSize;
            }
        }
        
        // Method must exist for Buffer::addNew and Buffer::insertNew to work.
        void create(uint32 index, uint32 count) {
            for (uint32 i = index; i < count && i < m_size; ++i) {
                new (m_data + i) DelegateBase();
            }
        }
        void copy(uint32 index, const DelegateBase* source, uint32 count) {
            for (uint32 i = 0; i < count && index + i < m_size; ++i) {
                source[i].copyTo(m_data + index + i);
            }
        }
        void copy(uint32 toIndex, uint32 fromIndex, uint32 count) {
            copy(toIndex, m_data + fromIndex, count);
        }
        void destroy(uint32 index, uint32 count) {
            if (m_data) {
                for (uint32 i = index; i < count && i < m_size; ++i) {
                    m_data[i].~DelegateBase();
                }
            }
        }
        
        DelegateBase* get(uint32 index) { return m_data + index; }
        const DelegateBase* get(uint32 index) const { return m_data + index; }
        
        uint32 size() const { return m_size; }
        uint32 actual_size() const { return m_size; }
        uint32 numBytes() const { return sizeof(DelegateBase) * m_size; }
        
        DelegateBase* data() { return m_data; }
        const DelegateBase* data() const { return m_data; }
    };
    
    template<typename ReturnType, typename... Args>
    class NEURO_API MulticastDelegate {
        // All we store inside the delegates is a pointer at most. The pointer is
        // not managed in any way, hence we can easily just copy bytewise.
        Buffer<DelegateBase, MulticastDelegateAllocator> buffer;
        
    public:
        MulticastDelegate& add(const Delegate<ReturnType, Args...>& deleg) {
            buffer.addNew();
            deleg.copyTo(&buffer[buffer.length() - 1]);
            return *this;
        }
        MulticastDelegate& add(const MulticastDelegate& other) {
            buffer.merge(other.buffer);
            return *this;
        }
        
        MulticastDelegate& remove(const Delegate<ReturnType, Args...>& deleg) {
            buffer.remove(deleg);
            return *this;
        }
        
        /**
         * Adds the specified delegate to this multicast.
         * @chainable
         */
        MulticastDelegate& operator+=(const Delegate<ReturnType, Args...>& deleg) { return add(deleg); }
        /**
         * Merges this multicast with the other, adding the other multicast's
         * delegates to this one.
         * 
         * Unfortunately it's not quite as easy (or efficient) to remove another
         * multicast's delegates from another as it lies in O(n*m).
         */
        MulticastDelegate& operator+=(const MulticastDelegate& other) { return add(other); }
        /**
         * Removes the first instance of the specified delegate from this multicast.
         * @chainable
         */
        MulticastDelegate& operator-=(const Delegate<ReturnType, Args...>& deleg) { return remove(deleg); }
        /**
         * Checks if this multicast has an equal instance of the specified delegate.
         */
        bool has(const Delegate<ReturnType, Args...>& deleg) const {
            return buffer.find(deleg) != npos;
        }
        
        /**
         * Removes all registered delegates. Useful when the context of the
         * delegate changes and needs to be reset.
         */
        MulticastDelegate& clear() {
            buffer.clear();
            return *this;
        }
        
        /**
         * Invoke every delegate in this multicast and accumulate their return
         * values inside a buffer which is then returned.
         */
        template<typename Ret = ReturnType>
        auto operator()(Args... args) const
         -> std::enable_if_t<!std::is_same_v<Ret, void>, Buffer<Ret>>
        {
            Buffer<Ret> result;
            for (const DelegateBase& deleg : *this) {
                auto real = reinterpret_cast<const Delegate<Ret, Args...>*>(&deleg);
                result.add((*real)(std::forward<Args>(args)...));
            }
            return result;
        }
        
        /**
         * Specialization for delegates without any return value. We obviously
         * can't accumulate "void" in a buffer.
         */
        template<typename Ret = ReturnType>
        auto operator()(Args... args) const
         -> std::enable_if_t<std::is_same_v<Ret, void>>
        {
            for (const DelegateBase& deleg : *this) {
                auto real = reinterpret_cast<const Delegate<Ret, Args...>*>(&deleg);
                (*real)(std::forward<Args>(args)...);
            }
        }
        
        const Delegate<ReturnType, Args...>* begin() const { return cbegin(); }
        const Delegate<ReturnType, Args...>* cbegin() const { return reinterpret_cast<const Delegate<ReturnType, Args...>*>(buffer.cbegin()); }
        const Delegate<ReturnType, Args...>* end() const { return cend(); }
        const Delegate<ReturnType, Args...>* cend() const { return reinterpret_cast<const Delegate<ReturnType, Args...>*>(buffer.cend()); }
    };
    
    template<typename... Args>
    using EventDelegate = MulticastDelegate<void, Args...>;
}
