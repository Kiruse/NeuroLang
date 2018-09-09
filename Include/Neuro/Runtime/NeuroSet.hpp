////////////////////////////////////////////////////////////////////////////////
// A specialized implementation of the hash set.
// 
// The hash set is the heart of the Neuro Objects. Because objects within the
// Neuro language don't have any hardcoded layout, it becomes extremely overhead-
// heavy to resolve symbols to absolute offsets, even entirely impossible once
// second-phase-compiled. We thus cannot get around a hash table lookup. But what
// we can do is optimize the hash set speed...
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <iterator>
#include <iostream>

#include "Allocator.hpp"
#include "Assert.hpp"
#include "DLLDecl.h"
#include "HashCode.hpp"
#include "NeuroBuffer.hpp"
#include "Numeric.hpp"
#include "Misc.hpp"

namespace Neuro
{
    /** Trivial identifier for a single element in the hash set. Valid as long as the associated hash set doesn't change. */
    struct NEURO_API StandardHashSetElementIdentifier {
        uint32 bucketIndex;
        uint32 indexInBucket;
        StandardHashSetElementIdentifier() = default;
        StandardHashSetElementIdentifier(uint32 bucketIndex, uint32 indexInBucket) : bucketIndex(bucketIndex), indexInBucket(indexInBucket) {}
        operator bool() const { return bucketIndex != npos && indexInBucket != npos; }
        bool operator==(const StandardHashSetElementIdentifier& other) const {
            // Both are invalid OR both have the exact same indices.
            return (!(bool)*this && !(bool)other) || (bucketIndex == other.bucketIndex && indexInBucket == other.indexInBucket);
        }
        bool operator!=(const StandardHashSetElementIdentifier& other) const { return !(*this==other); }
    };
    
    template<typename T,
             bool (*Comparator)(const T&, const T&),
             typename Allocator = AutoHeapAllocator<T>
            >
    class NEURO_API StandardHashSetBucket {
        Buffer<T, Allocator> buffer;
        hashT m_hashcode;
        
    public:
        StandardHashSetBucket(uint32 hashcode, uint32 size = 1, uint32 expand = 1) : buffer(size, expand), m_hashcode(hashcode) {}
        
        void add(const T& elem) {
            buffer.add(elem);
        }
        
        void remove(const T& elem) {
            remove(find(elem));
        }
        
        void remove(uint32 index) {
            buffer.splice(index);
        }
        
        uint32 find(const T& elem) const {
            for (uint32 i = 0; i < buffer.length(); ++i) {
                if (Comparator(buffer[i], elem)) return i;
            }
            return npos;
        }
        
        bool contains(const T& elem) const {
            return find(elem != StandardHashSet) != npos;
        }
        
        /** Shrinks this bucket to its minimum required size. */
        void shrink() {
            buffer.shrink();
        }
        
        T& get(uint32 index) {
            return buffer[index];
        }
        const T& get(uint32 index) const {
            return buffer[index];
        }
        
        T& last() { return buffer.last(); }
        const T& last() const { return buffer.last(); }
        
        const hashT hashcode() const { return m_hashcode; }
        
        const uint32 length() const { return buffer.length(); }
        const uint32 size() const { return buffer.size(); }
        const uint32 actual_size() const { return buffer.actual_size(); }
        
        bool empty() const { return !length(); }
    };
    
    /**
     * The Standard Hash Set is a well balanced algorithm with O(log(n)) reading
     * and writing, and O(1) resizing speeds.
     * 
     * The Comparator tests for equality of the data, and is used to distinguish
     * elements within the same bucket.
     */
    template<typename T,
             bool (*Comparator)(const T&, const T&) = is::equal,
             hashT (*Hasher)(const T&) = calculateHash,
             typename BucketT = StandardHashSetBucket<T, Comparator, AutoHeapAllocator<T>>,
             typename Allocator = RawHeapAllocator<BucketT>
            >
    class NEURO_API StandardHashSet
    {
        Buffer<BucketT, Allocator> buckets;
        
    public:
        template<bool Immutable>
        class Iterator {
        public:
            typedef uint32 difference_type;
            typedef T value_type;
            typedef T* pointer;
            typedef T& reference;
            typedef std::bidirectional_iterator_tag iterator_category;
            
        private:
            typedef toggle_const_t<StandardHashSet, Immutable> setType;
            setType& set;
            StandardHashSetElementIdentifier id;
            
        public:
            Iterator(setType& set, uint32 bucketIndex = 0, uint32 indexInBucket = 0) : set(set), id(bucketIndex, indexInBucket) {}
            
            Iterator& operator++() {
                if (id.bucketIndex < set.buckets.length()) {
                    if (++id.indexInBucket >= set.buckets[id.bucketIndex].length()) {
                        ++id.bucketIndex;
                        id.indexInBucket = 0;
                    }
                }
                normalizeEndIterator();
                return *this;
            }
            Iterator operator++(int) {
                Iterator copy(*this);
                ++*this;
                return copy;
            }
            Iterator& operator--() {
                if (id.bucketIndex < set.buckets.length()) {
                    if (--id.indexInBucket >= set.buckets[id.bucketIndex].length()) {
                        if (--id.bucketIndex < set.buckets.length()) {
                            id.indexInBucket = set.buckets[id.bucketIndex].length() - 1;
                        }
                    }
                }
                normalizeEndIterator();
                return *this;
            }
            Iterator operator--(int) {
                Iterator copy(*this);
                --*this;
                return copy;
            }
            
            template<bool OtherImmutable> bool operator==(const Iterator<OtherImmutable>& other) const { return set.buckets.data() == other.set.buckets.data() && id == other.id; }
            template<bool OtherImmutable> bool operator!=(const Iterator<OtherImmutable>& other) const { return !(*this == other); }
            
            toggle_const_t<T, Immutable>& operator*() {
                return set.get(id);
            }
            const T& operator*() const {
                return set.get(id);
            }
            
            operator bool() const { return (bool)id && id.bucketIndex < set.buckets.length() && id.indexInBucket < set.buckets[id.bucketIndex].length(); }
            
            void normalizeEndIterator() {
                if (id.bucketIndex >= set.buckets.length()) {
                    id.bucketIndex = id.indexInBucket = npos;
                }
            }
        };
        
        typedef Iterator<false> MutableIterator;
        typedef Iterator<true> ImmutableIterator;
        
    public:
        StandardHashSet(uint32 numberOfBuckets = 8) : buckets(numberOfBuckets) {}
        StandardHashSet(const std::initializer_list<T>& init) : buckets(init.size()) {
            for (auto curr : init) {
                add(curr);
            }
        }
        StandardHashSet(const StandardHashSet& other) : buckets(other.buckets.size()) {
            // There isn't really any other way to copy the contents of the set,
            // because the underlying allocator may copy bitwise...
            add(other);
        }
        StandardHashSet(StandardHashSet&&) = default;
        StandardHashSet& operator=(const std::initializer_list<T>& init) {
            clear();
            for (auto curr : init) {
                add(curr);
            }
            return *this;
        }
        StandardHashSet& operator=(const StandardHashSet& other) {
            clear();
            buckets.resize(other.buckets.length());
            add(other);
            return *this;
        }
        StandardHashSet& operator=(StandardHashSet&&) = default;
        ~StandardHashSet() {
            clear();
        }
        
        StandardHashSet& add(const T& elem) {
            BucketT& bucket = getOrCreateBucket(Hasher(elem));
			if (bucket.find(elem) == npos) bucket.add(elem);
            return *this;
        }
        StandardHashSet& add(const std::initializer_list<T>& list) { return add(list.begin(), list.end()); }
        StandardHashSet& add(const StandardHashSet& other) { return add(other.begin(), other.end()); }
        template<typename Iterator>
        StandardHashSet& add(const Iterator& first, const Iterator& last) {
            for (auto it = first; it != last; ++it) {
                add(*it);
            }
            return *this;
        }
        
        StandardHashSet& remove(const T& elem) {
            auto id = find(elem);
            if (id) {
                remove(id);
                if (buckets[id.bucketIndex].empty()) {
                    buckets.splice(id.bucketIndex, 1);
                }
            }
            return *this;
        }
        StandardHashSet& remove(const std::initializer_list<T>& list) { return remove(list.begin(), list.end()); }
        StandardHashSet& remove(const StandardHashSet& other) { return remove(other.begin(), other.end()); }
        template<typename Iterator>
        StandardHashSet& remove(const Iterator& first, const Iterator& last) {
            for (auto it = first; it != last; ++it) {
                remove(*it);
            }
            return *this;
        }
        
        StandardHashSet& remove(StandardHashSetElementIdentifier id) {
            if (id && id.bucketIndex < buckets.length() && id.indexInBucket < buckets[id.bucketIndex].length()) {
                buckets[id.bucketIndex].remove(id.indexInBucket);
            }
            return *this;
        }
        
        /**
         * Filters all elements out that are not also in the other set.
         * 
         * Because this operation is a rather expensive operation, we only
         * support other StandardHashSets for the improved average performance.
         */
        StandardHashSet& intersect(const StandardHashSet& other) {
            for (uint32 bucketIndex = 0; bucketIndex < buckets.length(); ++bucketIndex) {
                // If the other hash set contains this bucket's hash, we need to
                // compare every single element...
                if (auto otherBucketPtr = other.findBucket(buckets[bucketIndex].hashcode())) {
                    auto& thisBucket  = buckets[bucketIndex];
                    auto& otherBucket = *otherBucketPtr;
                    
                    for (uint32 elemIndex = 0; elemIndex < thisBucket.length(); ++elemIndex) {
                        auto& curr = thisBucket.get(elemIndex);
                        if (otherBucket.find(curr) == npos) {
                            thisBucket.remove(elemIndex);
                            --elemIndex;
                        }
                    }
                    
                    if (thisBucket.empty()) {
                        buckets.splice(bucketIndex, 1);
                        --bucketIndex;
                    }
                }
                
                // Otherwise we can simply remove the entire bucket directly.
                else {
                    buckets.splice(bucketIndex, 1);
                    --bucketIndex;
                }
            }
            return *this;
        }
        
        void clear() {
            buckets.clear();
        }
        
        StandardHashSetElementIdentifier find(const T& elem) const {
            hashT hash = Hasher(elem);
            for (uint32 i = 0; i < buckets.length(); ++i) {
                if (buckets[i].hashcode() == hash) {
                    return StandardHashSetElementIdentifier(i, buckets[i].find(elem));
                }
            }
            return StandardHashSetElementIdentifier(npos, npos);
        }
        
        bool contains(const T& elem) const {
            return find(elem);
        }
        
        T& get(const T& elem) { return get(find(elem)); }
        const T& get(const T& elem) const { return get(find(elem)); }
        T& get(StandardHashSetElementIdentifier id) { return buckets[id.bucketIndex].get(id.indexInBucket); }
        const T& get(StandardHashSetElementIdentifier id) const { return buckets[id.bucketIndex].get(id.indexInBucket); }
        
        /** Gets the current number of elements in this set. */
        uint32 count() const {
            uint32 result = 0;
            for (auto bucket : buckets) {
                result += bucket.length();
            }
            return result;
        }
        
        /** Gets the current max capacity of this entire set. */
        uint32 capacity() const {
            uint32 result = 0;
            for (auto bucket : buckets) {
                result += bucket.size();
            }
            return result;
        }
        
        /** Shrinks this set to its minimum required size. */
        void shrink() {
            buckets.shrink();
            for (auto bucket : buckets) {
                bucket.shrink();
            }
        }
        
        bool operator==(const StandardHashSet& other) const {
            if (buckets.length() != other.buckets.length()) return false;
            // Buckets are ordered for faster lookup.
            for (uint32 bucketIndex = 0; bucketIndex < buckets.length(); ++bucketIndex) {
                if (buckets[bucketIndex].length() != other.buckets[bucketIndex].length()) return false;
                
                // Elements in the buckets are not ordered, however.
                for (uint32 elemIndex = 0; elemIndex < buckets[bucketIndex].length(); ++elemIndex) {
                    if (other.buckets[bucketIndex].find(buckets[bucketIndex].get(elemIndex)) == npos) return false;
                }
                
                // It's enough to iterate over one set, because duplicates are illegal.
            }
            return true;
        }
        bool operator!=(const StandardHashSet& other) const { return !(*this==other); }
        
        MutableIterator begin() {
            MutableIterator result(*this);
            if (!result) ++result;
            return result;
        }
        ImmutableIterator begin() const { return cbegin(); }
        ImmutableIterator cbegin() const {
            ImmutableIterator result(*this);
            if (!result) ++result;
            return result;
        }
        MutableIterator end() { return MutableIterator(*this, npos, npos); }
        ImmutableIterator end() const { return cend(); }
        ImmutableIterator cend() const { return ImmutableIterator(*this, npos, npos); }
        
    protected:
        /**
         * Attempts to find the bucket holding elements of the specified hash within the designated range.
         * Returns nullptr if none found, including if the range is empty.
         */
        BucketT* findBucket(hashT hash, uint32 start = 0, uint32 end = npos) {
            if (end == npos) end = buckets.length();
            const uint32 dist = end - start;
            
            // Error! Should never happen!
            if (dist == 0) return nullptr;
            
            // Did we find it?!
            if (dist == 1) {
                if (buckets[start].hashcode() == hash) return &buckets[start];
                return nullptr;
            }
            
            // Divide and conquer!
            const uint32 pivot = start + dist / 2;
            
            if (hash < buckets[pivot].hashcode()) {
                return findBucket(hash, 0, pivot);
            }
            else {
                return findBucket(hash, pivot, end);
            }
        }
        
        /**
         * Same as non-const variant, except returned result is immutable.
         */
        const BucketT* findBucket(hashT hash, uint32 start = 0, uint32 end = npos) const {
            if (end == npos) end = buckets.length();
            const uint32 dist = end - start;
            
            // Error! Should never happen!
            if (dist == 0) return nullptr;
            
            // Did we find it?!
            if (dist == 1) {
                if (buckets[start].hashcode() == hash) return &buckets[start];
                return nullptr;
            }
            
            // Divide and conquer!
            const uint32 pivot = start + dist / 2;
            
            if (hash < buckets[pivot].hashcode()) {
                return findBucket(hash, 0, pivot);
            }
            else {
                return findBucket(hash, pivot, end);
            }
        }
        
        /**
         * @brief Insert the bucket into its appropriate location.
         * 
         * Does so through divide-and-conquer recursively. Assumes the fed indices
         * are within bounds.
         * 
         * CAVEAT: Fails if the range distance is 0!
         */
        uint32 insertBucket(uint32 hash, uint32 start, uint32 end) {
            const uint32 dist = end - start;
            
            // ERROR: This should never happen!
            if (dist == 0) return npos;
            
            // If this is the last element we're comparing against, we know we
            // belong here.
            if (dist == 1) {
                // Insert before the element.
                if (hash < buckets[start].hashcode()) {
                    buckets.insertNew(start, hash);
                    return start;
                }
                // Cannot "insert after" the last element.
                else if (start < buckets.length() - 1) {
                    buckets.insertNew(start + 1, hash);
                    return start + 1;
                }
                // Must add instead.
                else {
                    buckets.addNew(hash);
                    return buckets.length() - 1;
                }
            }
            
            // Divide and conquer!
            const uint32 midIndex = start + dist / 2;
            
            if (hash < buckets[midIndex].hashcode()) {
                return insertBucket(hash, 0, midIndex);
            }
            else {
                return insertBucket(hash, midIndex, end);
            }
        }
        
        /**
         * Gets the bucket associated with the specified hash.
         * 
         * If no such bucket exists, the bucket is created, and the set resized
         * if necessary.
         */
        BucketT& getOrCreateBucket(hashT hash) {
            if (auto found = findBucket(hash)) return *found;
            
            // Append if the list is empty
            if (buckets.empty()) {
                buckets.addNew(hash);
                return buckets[buckets.length() - 1];
            }
            
            // Otherwise create a new bucket and insert it where it belongs.
            uint32 index = insertBucket(hash, 0, buckets.length());
            return buckets[index];
        }
    };
    
    
    /**
     * A simple element wrapper for the fast hash set, so we even know which
     * buckets are valid and which are not.
     */
    template<typename T>
    struct NEURO_API FastHashSetElementWrapper {
        bool valid;
        T value;
    };
    
    /**
     * The Fast Hash Set is a specialized implementation especially efficient for
     * medium-size hash sets. It uses empty memory buffers and a special notion
     * of buckets and bucketbuckets to achieve O(m) reading and writing, but
     * O(n / m) = O(n) resizing, where m is a constant bucket size, and resizing
     * may become O(1) if the data type is trivial / can be moved bytewise,
     * depending on the platform's performance of std::memcpy.
     * 
     * Since within Neuro values are represented through Neuro::Value, which is
     * a trivial type, this is indeed an extremely fast implementation, at the
     * expense of buffer size. Nonetheless this is the preferred implementation.
     * 
     * The entire implementation revolves around mathematically simple formulas
     * to calculate the offset within the buffer. The bucketbuckets determine
     * a first-layer subrange in the buffer where we should attempt to find the
     * corresponding bucket of a hash code. Thus bucketbuckets have a fixed size
     * and buckets share the same size.
     * 
     * Oh, and also: TODO.
     */
    template<typename T, bool (*Comparator)(const T&, const T&), uint32 (*Hasher)(const T&)>
    struct NEURO_API FastHashSet {
        AutoHeapAllocator<FastHashSetElementWrapper<T>> alloc;
        
    };
}
