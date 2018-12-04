////////////////////////////////////////////////////////////////////////////////
// A specialization of the Hash Set associating the hash-coded key with a single
// value.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include "DLLDecl.h"
#include "Maybe.hpp"
#include "NeuroSet.hpp"

namespace Neuro
{
    template<typename KeyT, typename ValueT>
    struct NEURO_API HashMapPair {
        hashT hashcode;
        KeyT key;
        Maybe<ValueT> value;
        
        operator ValueT&() { return *value; }
        operator const ValueT&() const { return *value; }
        HashMapPair& operator=(const ValueT& newValue) {
            value = newValue;
            return *this;
        }
    };
    
    /**
     * A specialization for the HashMapPair which simply returns the precalculated
     * hashcode from the pair.
     */
    template<typename KeyT, typename ValueT>
    inline NEURO_API hashT calculateHash(const HashMapPair<KeyT, ValueT>& pair) {
        return calculateHash(pair.key);
    }
    
    // A comparison function that makes clear that we are not comparing two pairs
    // for exact equality, but only their keys.
    namespace is
    {
        template<typename KeyT, typename ValueT>
        bool sameKey(const HashMapPair<KeyT, ValueT>& lhs, const HashMapPair<KeyT, ValueT>& rhs) {
            return lhs.key == rhs.key;
        }
    }
    
    // We're cheating here. The hash map is literally just a hash set with a special
    // element type (a pair), customized comparator, and specialized calculateHash. ;)
    // Oh, and an additional `ValueT& operator[](const KeyT&)`.
    template<typename KeyT,
             typename ValueT,
             bool (*Comparator)(const HashMapPair<KeyT, ValueT>&, const HashMapPair<KeyT, ValueT>&) = is::sameKey,
             typename BucketT = StandardHashSetBucket<HashMapPair<KeyT, ValueT>, Comparator, AutoHeapAllocator<HashMapPair<KeyT, ValueT>>>,
             typename Allocator = RawHeapAllocator<BucketT>
            >
    class NEURO_API StandardHashMap : public StandardHashSet<HashMapPair<KeyT, ValueT>, Comparator, calculateHash, BucketT, Allocator>
    {
        typedef StandardHashSet<HashMapPair<KeyT, ValueT>, Comparator, calculateHash, BucketT, Allocator> Base;
        
    public:
        bool has(const KeyT& key) const {
            return contains(createPair(key));
        }
        
        StandardHashMap& remove(const KeyT& key) {
            Base::remove(createPair(key));
            return *this;
        }
        
        ValueT& operator[](const KeyT& key) {
            return getOrCreate(key).value.get();
        }
        const ValueT& operator[](const KeyT& key) const {
            return get(createPair(key)).value.get();
        }
        
        HashMapPair<KeyT, ValueT>& getOrCreate(const KeyT& key) {
            hashT hashcode = calculateHash(key);
            auto tmpPair = createPair(key, hashcode);
            
            // Attempt to find an existing key.
            auto id = find(tmpPair);
            if (id) {
                return get(id);
            }
            
            // Find an existing bucket of the hash code or create a new one.
            BucketT& bucket = getOrCreateBucket(hashcode);
            bucket.add(tmpPair);
            return bucket.last();
        }
        
    protected:
        static HashMapPair<KeyT, ValueT> createPair(const KeyT& key) {
            return createPair(calculateHash(key), key);
        }
        
        static HashMapPair<KeyT, ValueT> createPair(const KeyT& key, hashT hashcode) {
            HashMapPair<KeyT, ValueT> pair;
            pair.hashcode = hashcode;
            pair.key = key;
            return pair;
        }
    };
}
