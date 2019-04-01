////////////////////////////////////////////////////////////////////////////////
// The reverse of a semaphore, designed to allow multiple readers until locked.
// While readers are available, a writer attempting to lock the semaphore will
// block until all current readers have released their lock. While the lock is
// reserved, new readers requesting shared access to the associated resource are
// blocked until the writer releases its lock again.
// 
// Naturally this means a reader turning into a writer must always first release
// his shared access before requesting the hard lock.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include <mutex>
#include <shared_mutex>
#include <condition_variable>

#include "DLLDecl.h"
#include "Numeric.hpp"

#pragma warning(push)
#pragma warning(disable: 4251) // class ... needs to have dll-interface ...

namespace Neuro {
    namespace Runtime {
        namespace Concurrency
        {
            class NEURO_API ReverseSemaphore {
            private: // Properties
                uint32 numSharedUsers       : 31;
				uint32 isExclusiveRequested :  1;
                
                std::mutex syncMutex;
                std::mutex exclusiveMutex;
                std::condition_variable syncNotif;
                
            public:  // RAII
                ReverseSemaphore();
                ReverseSemaphore(const ReverseSemaphore&) = delete;
                ReverseSemaphore(ReverseSemaphore&&) = delete;
                ReverseSemaphore& operator=(const ReverseSemaphore&) = delete;
                ReverseSemaphore& operator=(ReverseSemaphore&&) = delete;
                ~ReverseSemaphore();
                
            public:  // Shared Lock
                /**
                 * Locks the semaphore in shared access.
                 * 
                 * Supports recursive invocation. For every call, .unlockShared()
                 * must be called to fully release semaphore.
                 * 
                 * If another thread already owns the semaphore in exclusive access,
                 * blocks until available. Multiple shared accesses are permitted.
                 */
                void lockShared();
                
                /**
                 * Unlocks shared access of this semaphore once. The semaphore
                 * is considered fully released when all calls to .lockShared()
                 * have been concluded with a call to .unlockShared().
                 * 
                 * If called from a thread that currently does not own shared
                 * access, causes undefined behavior.
                 */
                void unlockShared();
                
                /**
                 * Attempts to lock semaphore for shared access. Fails under
                 * the same conditions as .lockShared(). If failed, returns
                 * false, otherwise returns true and semaphore is locked for
                 * shared access until another invocation of .unlockShared().
                 */
                bool tryLockShared();
                
                /**
                 * Checks if currently one or more shared users are accessing
                 * the semaphore.
                 * 
                 * Should not be used to test if acquisition is safe. Instead,
                 * prefer tryLockShared(). Otherwise race conditions between
                 * this call and lock acquisition may occur.
                 * 
                 * Intended for debugging and testing purposes.
                 */
                bool hasSharedUsers() const { return numSharedUsers > 0; }
                
            public:  // Exclusive Lock
                /**
                 * Locks the semaphore in exclusive access.
                 * 
                 * Supports recursive invocation. For every call, .unlock()
                 * must be called to fully release semaphore.
                 * 
                 * If another thread already owns the semaphore in exclusive OR
                 * shared access, blocks until available. Exclusive lock requests
                 * are prioritized over shared lock requests, i.e. every call to
                 * lockShared() will block until every exclusive user has had its
                 * turn.
                 */
                void lock();
                
                /**
                 * Unlocks exclusive access of this semaphore.
                 * 
                 * If called from a thread that currently does not own the semaphore,
                 * causes undefined behavior.
                 */
                void unlock();
                
                /**
                 * Attempts to lock semaphore for exclusive access. Fails and
                 * returns false if another thread is currently already in
                 * exclusive OR shared access, otherwise returns true and the
                 * semaphore is locked until another invocation of .unlock().
                 */
                bool tryLock();
                
                /**
                 * Checks if currently one or more exclusive users are accessing
                 * or queued for access to the semaphore.
                 * 
                 * Should not be used to test if acquisition is safe. Instead,
                 * prefer tryLock(). Otherwise race conditions between
                 * this call and lock acquisition may occur.
                 * 
                 * Intended for debugging and testing purposes.
                 */
                bool hasExclusiveUsers() const { return isExclusiveRequested && numSharedUsers == 0; }
            };
        }
    }
}

#pragma warning(pop)
