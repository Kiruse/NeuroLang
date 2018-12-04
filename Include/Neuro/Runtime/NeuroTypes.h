/*******************************************************************************
 * Various types used within the neuro library, and exposed together with the
 * C interface. These types are either PoD or used as pointers.
 * -----
 * Copyright (c) Kiruse 2018
 * License: GPL 3.0
 */
#ifndef NEURO_TYPES_H
#define NEURO_TYPES_H

struct neuroArchetype;
struct neuroBuffer;
struct neuroFrame;
struct neuroManagedMemory;
struct neuroObject;
struct neuroValue;

/**
 * Enumeration of general data types of neuroValue. Either one of primitives
 * (bool, byte, short, integer, long, float, double), or a (managed or unmanaged)
 * object.
 * 
 * Note: The Neuro Language (currently) only distinguishes between primitive
 * values and object (pointers).
 */
enum neuroValueType {
    NVT_Undefined,
    NVT_Bool,
    NVT_Byte,
    NVT_Short,
    NVT_Integer,
    NVT_Long,
    NVT_Float,
    NVT_Double,
    NVT_NativeObject,
    NVT_Object,
    NVT_MAX
};

#endif /* NEURO_TYPES_H */
