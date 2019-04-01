////////////////////////////////////////////////////////////////////////////////
// Implementation of the reverse semaphore.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#include "Concurrency/ReverseSemaphore.hpp"

namespace Neuro {
    namespace Runtime {
        namespace Concurrency
        {
            ReverseSemaphore::ReverseSemaphore()
             : numSharedUsers(0)
             , isExclusiveRequested(false)
             , syncMutex()
             , exclusiveMutex()
             , syncNotif()
            {}
            ReverseSemaphore::~ReverseSemaphore() {}
            
            
            void ReverseSemaphore::lockShared() {
                std::unique_lock<std::mutex> lock(syncMutex);
                syncNotif.wait(lock, [&]() { return !isExclusiveRequested; });
                numSharedUsers++;
            }
            
            void ReverseSemaphore::unlockShared() {
                std::unique_lock<std::mutex> lock(syncMutex);
                numSharedUsers--;
                syncNotif.notify_all();
            }
            
            bool ReverseSemaphore::tryLockShared() {
                if (!syncMutex.try_lock()) {
                    return false;
                }
                
                if (isExclusiveRequested) {
                    syncMutex.unlock();
                    return false;
                }
                
                numSharedUsers++;
                syncMutex.unlock();
                return true;
            }
            
            
            void ReverseSemaphore::lock() {
				exclusiveMutex.lock();
                isExclusiveRequested = true;
                std::unique_lock<std::mutex> lock(syncMutex);
                syncNotif.wait(lock, [&]() { return numSharedUsers == 0; });
            }
            
            void ReverseSemaphore::unlock() {
                std::unique_lock<std::mutex> lock(syncMutex);
                isExclusiveRequested = false;
                exclusiveMutex.unlock();
                syncNotif.notify_all();
            }
            
            bool ReverseSemaphore::tryLock() {
                // Another thread is already attempting to acquire shared or
                // exclusive access
                if (!syncMutex.try_lock()) {
                    return false;
                }
                
                // Another thread is already in exclusive access
                if (!exclusiveMutex.try_lock()) {
                    syncMutex.unlock();
                    return false;
                }
                
                // Catches case where not in exclusive access, but shared users
                // are still doing work
                if (numSharedUsers > 0) {
                    syncMutex.unlock();
                    return false;
                }
                
                isExclusiveRequested = true;
                syncMutex.unlock();
                return true;
            }
        }
    }
}
