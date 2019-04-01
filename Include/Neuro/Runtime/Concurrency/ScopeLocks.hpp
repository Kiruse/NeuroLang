////////////////////////////////////////////////////////////////////////////////
// Several locks for use with our mutex / semaphore classes.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include "DLLDecl.h"
#include "Numeric.hpp"

namespace Neuro {
    namespace Runtime {
        namespace Concurrency
        {
            template<typename T>
            class SharedLock {
            private:
                T& lockable;
                
            public:
                SharedLock(T& lockable) : lockable(lockable) {
                    lockable.lockShared();
                }
                SharedLock(const SharedLock&) = delete;
                SharedLock(SharedLock&&) = delete;
                SharedLock& operator=(const SharedLock&) = delete;
                SharedLock& operator=(SharedLock&&) = delete;
                ~SharedLock() {
                    lockable.unlockShared();
                }
            };
            
            template<typename T>
            class TrySharedLock {
            private:
                bool acquired;
                T& lockable;
                
            public:
                TrySharedLock(T& lockable) : acquired(), lockable(lockable) {
                    acquired = lockable.tryLockShared();
                }
                TrySharedLock(const TrySharedLock&) = delete;
                TrySharedLock(TrySharedLock&&) = delete;
                TrySharedLock& operator=(const TrySharedLock&) = delete;
                TrySharedLock& operator=(TrySharedLock&&) = delete;
                ~TrySharedLock() {
                    if (acquired) lockable.unlockShared();
                }
                
                operator bool() const {
                    return acquired;
                }
            };
            
            template<typename T>
            class UniqueLock {
            private:
                T& lockable;
                
            public:
                UniqueLock(T& lockable) : lockable(lockable) {
                    lockable.lock();
                }
                UniqueLock(const UniqueLock&) = delete;
                UniqueLock(UniqueLock&&) = delete;
                UniqueLock& operator=(const UniqueLock&) = delete;
                UniqueLock& operator=(UniqueLock&&) = delete;
                ~UniqueLock() {
                    lockable.unlock();
                }
            };
            
            template<typename T>
            class TryUniqueLock {
            private:
                bool acquired;
                T& lockable;
                
            public:
                TryUniqueLock(T& lockable) : acquired(), lockable(lockable) {
                    acquired = lockable.tryLock();
                }
                TryUniqueLock(const TryUniqueLock&) = delete;
                TryUniqueLock(TryUniqueLock&&) = delete;
                TryUniqueLock& operator=(const TryUniqueLock&) = delete;
                TryUniqueLock& operator=(TryUniqueLock&&) = delete;
                ~TryUniqueLock() {
                    if (acquired) lockable.unlock();
                }
                
                operator bool() const {
                    return acquired;
                }
            };
        }
    }
}