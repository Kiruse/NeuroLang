////////////////////////////////////////////////////////////////////////////////
// The C++ interface of the Neuro RT Garbage Collector, and its actual
// implementation.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include "Error.hpp"
#include "NeuroBuffer.hpp"
#include "NeuroTypes.h"

namespace Neuro {
    namespace Runtime {
        namespace GC
        {
            /** Adds the specified neuro object to the root. */
            Error addRoot(neuroObject* object);
            
            /** Removes the specified neuro object from the root. */
            Error removeRoot(neuroObject* object);
            
            /** Marks the specified neuro object as dead. */
            Error markDead(neuroObject* object);
            
            /**
             * Scans for dead (i.e. unreachable) objects. This operation is
             * linear to the number of alive objects.
             * 
             * This is a lengthy operation and should be performed in a separate
             * thread. Alternatively a latent async version of this function may
             * be called instead.
             * 
             * TODO: Delegates for events & callbacks!
             */
            Error scanDead();
            
            /**
             * Purges memory buffers marked as dead. This operation should be
             * relatively fast.
             */
            Error purge();
            
            /**
             * Attempts to optimize memory usage. Memory may be shifted around
             * to optimize random access performance, and to minimize cache misses.
             * 
             * This is a lengthy operation and should be performed in a separate
             * thread. Alternatively a latent async version of this function may
             * be called instead.
             * 
             * TODO: Delegates for events & callbacks!
             */
            Error optimize();
        }
    }
}
