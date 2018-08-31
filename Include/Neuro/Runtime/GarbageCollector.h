/*******************************************************************************
 * The C Interface to communicate with the Neuro RT Garbage Collector.
 * -----
 * Copyright (c) Kiruse 2018
 * License: GPL 3.0
 */
#ifndef NEURO_GARBAGECOLLECTOR_H
#define NEURO_GARBAGECOLLECTOR_H

#include "DLLDecl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Adds the specified object to the GC root. */
NEURO_API int neuroGCAddRoot(struct neuroObject* object);

/** Removes the specified object from the GC root. */
NEURO_API int neuroGCRemoveRoot(struct neuroObject* object);

/** Marks the specified object as dead. It will be destroyed in the next call to neuroGCPurge (which may happen automatically). */
NEURO_API int neuroGCMarkDead(struct neuroObject* object);

/**
 * Searches the GC tree for dead objects and marks them for deletion.
 * 
 * This is a lengthy operation which should be performed in a separate thread
 * where possible.
 */
NEURO_API int neuroGCScanDead();

/** Deletes marked objects. This operation should be relatively fast. */
NEURO_API int neuroGCPurge();

/**
 * Instructs the GC to optimize the allocated memory. Objects may be shifted
 * around in an attempt to improve random access performance.
 * 
 * This is a lengthy operation which should be performed in a separate thread
 * where possible.
 */
NEURO_API int neuroGCOptimize();

#ifdef __cplusplus
}
#endif

#endif /* NEURO_GARBAGECOLLECTOR_H */
