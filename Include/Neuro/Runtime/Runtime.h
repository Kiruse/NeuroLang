/*******************************************************************************
 * The Runtime Library of the Neuro Language. Writing the RTLib in C allows us
 * to link to it from most other languages, such that the NeuroLib technically
 * is a standalone library.
 * ---
 * Copyright (c) Kiruse. See license in LICENSE.txt, or online at http://neuro.kirusifix.com/license.
 */
#ifndef NEURO_RUNTIME_H
#define NEURO_RUNTIME_H

#include "DLLDecl.h"
#include "NeuroTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize NeuroRT
 * @returns int Error code. 0 if successful.
 */
NEURO_API int neuroInit();

/**
 * @brief Clean up NeuroRT (incl. GC).
 * @returns int Error code. 0 if successful.
 */
NEURO_API int neuroShutdown();

/**
 * @brief Gets the last error message, if any.
 * 
 * The error message is not stacked. If a new error arose before the old
 * error's message is retrieved, the message is lost.
 * 
 * @return const char* The error message, or NULL if none.
 */
NEURO_API const char* neuroGetLastErrorMessage();

/**
 * @brief Allocates and initializes a new frame, optionally with the specified parent frame.
 * 
 * @param parent_frame Parent frame whose variables are accessible in this frame.
 * @return struct neuroFrame* 
 */
NEURO_API struct neuroFrame* neuroFrameEnter(struct neuroFrame* parent_frame);

/**
 * @brief Leaves all nested frames in order
 * @param frame Frame to leave.
 */
NEURO_API void neuroFrameLeave(struct neuroFrame* frame);

/**
 * @brief Allocates and initializes a new generic (hash) object.
 * 
 * Technically this is tightly associated with the Garbage Collector, but to the
 * Neuro language, this is such an essential tool that it is exposed through the
 * runtime directly.
 * 
 * @return struct neuroObject* 
 */
NEURO_API struct neuroObject* neuroObjectCreate();

/**
 * @brief Destroys the given object.
 */
NEURO_API void neuroObjectDestroy(struct neuroObject*);

#ifdef __cplusplus
}
#endif

#endif /* NEURO_RUNTIME_H */
