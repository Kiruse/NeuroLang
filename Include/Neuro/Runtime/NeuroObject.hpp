////////////////////////////////////////////////////////////////////////////////
// Managed, classless object of the Neuro Runtime.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

#include "DLLDecl.h"
#include "NeuroTypes.h"
//#include "NeuroMap.hpp"
#include "NeuroString.hpp"
#include "NeuroValue.hpp"

namespace Neuro
{
    /**
     * Managed, classless object.
     */
    class NEURO_API Object {
        
        
    public:
        Object(String name);
        Object(Object&&);
        Object& operator=(Object&&);
        ~Object();
        
    public:
        
    };
}
