////////////////////////////////////////////////////////////////////////////////
// Unit Test for ReverseSemaphores.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
#include "Assert.hpp"
#include "CLInterface.hpp"
#include "Concurrency/ReverseSemaphore.hpp"
#include "Concurrency/ScopeLocks.hpp"

#include <thread>
#include <vector>
#include <iostream>

using namespace Neuro;
using namespace Neuro::Runtime;
using namespace Neuro::Runtime::Concurrency;


int main(int argc, char** argv) {
    using seconds      = std::chrono::seconds;
    using millis       = std::chrono::milliseconds;
    using steady_clock = std::chrono::steady_clock;
    using std::this_thread::sleep_for;
    
    Testing::section("ReverseSemaphore", []() {
        Testing::test("Multiple readers", []() {
            ReverseSemaphore semaphore;
            
            Testing::assert(!semaphore.hasSharedUsers(), "Unexpected shared user after initialization");
            Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user after initialization");
            
            auto reader1 = [&]() {
                SharedLock{semaphore};
                sleep_for(seconds(2));
            };
            
            auto reader2 = [&]() {
                TrySharedLock lock(semaphore);
                Testing::assert(!!lock, "Failed to acquired shared lock");
            };
            
            std::thread r1(reader1);
            std::thread r2(reader2);
            
            r1.join();
            r2.join();
            
            Testing::assert(!semaphore.hasSharedUsers(), "Unexpected shared user at test conclusion");
            Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user at test conclusion");
        });
        
        Testing::test("Single writer", []() {
            ReverseSemaphore semaphore;
            auto start = steady_clock::now();
            seconds waitDur(10);
            
            Testing::assert(!semaphore.hasSharedUsers(), "Unexpected shared user after initialization");
            Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user after initialization");
            
            auto writer1 = Testing::thread([&]() {
				UniqueLock lock(semaphore);
				sleep_for(waitDur);
            });
            
            auto writer2 = Testing::thread([&]() {
                sleep_for(seconds(1));
                Testing::assert(semaphore.hasExclusiveUsers(), "Expected an exclusive user to already have acquired access");
                
				UniqueLock lock(semaphore);
                auto end = steady_clock::now();
                Testing::assert(end - start >= waitDur, "Gained exclusive lock unexpectedly early");
                Testing::assert(semaphore.hasExclusiveUsers(), "Expected to have an exclusive user (us!)");
            });
            
            std::thread w1(writer1);
            std::thread w2(writer2);
            w1.join();
            w2.join();
            
            auto end = steady_clock::now();
            Testing::assert(end - start >= waitDur, "Test concluded unexpectedly early");
            Testing::assert(!semaphore.hasSharedUsers(), "Unexpected shared user at test conclusion");
            Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user at test conclusion");
        });
        
        Testing::section("Many readers, one writer", []() {
            Testing::test("Read-Write", []() {
                ReverseSemaphore semaphore;
                
                auto start = steady_clock::now();
                auto reader = Testing::thread([&]() {
                    Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user");
                    
                    SharedLock lock(semaphore);
                    
                    Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user");
                    sleep_for(seconds(1));
                });
                
                std::thread r1(reader);
                std::thread r2(reader);
                
                sleep_for(millis(500));
                {
                    Testing::assert(semaphore.hasSharedUsers(), "Expected shared users");
                    
                    UniqueLock lock(semaphore);
                    
                    Testing::assert(!semaphore.hasSharedUsers(), "Unexpected shared users during exclusive access");
                    
                    auto end = steady_clock::now();
                    Testing::assert(end - start >= seconds(1), "Gained exclusive access unexpectedly early");
                }
                
                r1.join(); r2.join();
                
                auto end = steady_clock::now();
                Testing::assert(end - start >= seconds(1), "Test concluded unexpectedly early");
                Testing::assert(!semaphore.hasSharedUsers(), "Unexpected shared user at test conclusion");
                Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user at test conclusion");
            });
            
            Testing::test("Write-Read", []() {
                ReverseSemaphore semaphore;
                auto start = steady_clock::now();
                
                auto reader = Testing::thread([&]() {
                    Testing::assert(semaphore.hasExclusiveUsers(), "Expected exclusive user before shared lock acquisition");
                    
                    SharedLock lock(semaphore);
                    Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user after shared lock acquisition");
                    
                    auto end = steady_clock::now();
                    Testing::assert(end - start >= seconds(1), "Acquired shared lock unexpectedly early");
                });
                
                auto writer = Testing::thread([&]() {
                    Testing::assert(!semaphore.hasSharedUsers(), "Unexpected active reader before unique lock acquisition");
                    
                    UniqueLock lock(semaphore);
                    sleep_for(seconds(1));
                });
                
                std::thread w(writer);
                std::thread r1(reader);
                std::thread r2(reader);
                
                w.join(); r1.join(); r2.join();
                
                auto end = steady_clock::now();
                Testing::assert(end - start >= seconds(1), "Test concluded unexpectedly early");
                Testing::assert(!semaphore.hasSharedUsers(), "Unexpected shared user at test conclusion");
                Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user at test conclusion");
            });
            
            Testing::test("Read-Write-Read", []() {
                ReverseSemaphore semaphore;
                auto start = steady_clock::now();
                
                auto reader1 = Testing::thread([&]() {
                    Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user before 1st shared lock acquisition");
                    
                    SharedLock lock(semaphore);
                    Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user after 1st shared lock acquisition");
                    sleep_for(seconds(1));
                });
                
                auto reader2 = Testing::thread([&]() {
                    sleep_for(millis(1500));
                    Testing::assert(semaphore.hasExclusiveUsers(), "Expected exclusive user before 2nd shared lock acquisition");
                    
                    SharedLock lock(semaphore);
                    Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user after 2nd shared lock acquisition");
                    
                    auto end = steady_clock::now();
                    Testing::assert(end - start >= seconds(2), "Gained 2nd shared lock unexpectedly early");
                });
                
                auto writer = Testing::thread([&]() {
                    sleep_for(millis(500));
                    Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user before unique lock acquisition");
                    Testing::assert(semaphore.hasSharedUsers(), "Expected shared user before unique lock acquisition");
                    
                    UniqueLock lock(semaphore);
                    Testing::assert(!semaphore.hasSharedUsers(), "Unexpected shared user after unique lock acquisition");
                    
                    auto end = steady_clock::now();
                    Testing::assert(end - start >= seconds(1), "Gained unique lock unexpectedly early");
                    
                    sleep_for(seconds(1));
                });
                
                std::thread r1(reader1);
                std::thread r2(reader2);
                std::thread w(writer);
                
                r1.join(); r2.join(); w.join();
                
                auto end = steady_clock::now();
                Testing::assert(end - start >= seconds(2), "Test concluded unexpectedly early");
                Testing::assert(!semaphore.hasSharedUsers(), "Unexpected shared user at test conclusion");
                Testing::assert(!semaphore.hasExclusiveUsers(), "Unexpected exclusive user at test conclusion");
            });
        });
    });
}
