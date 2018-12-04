/*******************************************************************************
 * The C Interface to communicate with the Neuro RT Garbage Collector.
 * -----
 * Copyright (c) Kiruse 2018
 * License: GPL 3.0
 */
#ifndef NEURO_GARBAGECOLLECTOR_H
#define NEURO_GARBAGECOLLECTOR_H

#include <stdint.h>

#include "DLLDecl.h"
#include "NeuroTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Allocates memory to a new managed object. */
NEURO_API struct neuroObject* neuroGCAllocateObject();

/** Allocates memory to a new managed continuous memory buffer / array. */
NEURO_API struct neuroBuffer* neuroGCAllocateBuffer(size_t numberOfElements);

/** Adds the specified object to the GC root. Inherently thread-safe. */
NEURO_API int neuroGCAddRoot(struct neuroObject* object);

/** Removes the specified object from the GC root. Inherently thread-safe. */
NEURO_API int neuroGCRemoveRoot(struct neuroObject* object);

/**
 * Marks the specified object as dead. It will be destroyed in the next purge.
 * Because the flag is only set and never unset, this is inherently thread-safe.
 */
NEURO_API int neuroGCMarkDead(struct neuroObject* object);

/**
 * Finds dead (i.e. unreachable) objects in the graph and destroys them.
 * 
 * This is likely a lengthy operation and should preferably be conducted in a
 * second thread.
 */
NEURO_API int neuroGCPurge();

/** Clears the entire managed memory and resets the Garbage Collector. */
NEURO_API int neuroGCClear();

#ifdef __cplusplus
}
#endif

#endif /* NEURO_GARBAGECOLLECTOR_H */
