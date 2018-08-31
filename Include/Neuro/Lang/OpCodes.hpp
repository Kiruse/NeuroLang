////////////////////////////////////////////////////////////////////////////////
// Provides Neuro IL opcodes and information on said opcodes.
// ---
// Copyright (c) Kiruse. See license in LICENSE.txt, or online at http://carbon.kirusifix.com/license.
// ---
// Neuro is, very similar to other languages like most other languages, compiled
// into an intermediate language. However, unlike most interpreted languages,
// Neuro has a very Assembler-like set of opcodes. Because Neuro is designed to
// be compiled down to native applications in the future as well, keeping it
// as close to assembler as possible simplifies the phase two compilation.
#pragma once

#include "Numeric.hpp"

namespace Neuro {
    namespace Lang {
        typedef uint32 OpCodeType;
        
        // The order of op codes is NOT to be changed in future updates, for the sake
        // of backwards compatibility!
        // OpCodes must be in a continuous range for the sake of easy iteration.
        enum class OpCodes : OpCodeType {
            
            ////////////////////////////////////////////////////////////////////////
            // V1.0
            
            // Memory management
            EnterFrame,
            PushStack,
            Alloc,
            Realloc,
            Dealloc,
            PopStack,
            ExitFrame,
            LoadEffectiveAddress, // x86 LEA
            
            // Object Management
            Init,
            Copy,
            Move,
            Destroy,
            
            // Arithmetics
            Add,
            Subtract,
            Multiply,
            Divide,
            Modulus,
            ShiftLeft,
            ShiftRight,
            
            // Flow Control
            
            // Note that we heavily simplify the various conditioned jumps. It seems
            // easier, for the time being, especially considering support for both
            // interpreted and future native mode, to prefer introducing additional
            // opcodes for the various flag checks (overflow, sign, parity, etc.)
            // and use the result of their computation with JumpZero or JumpNonZero,
            // then later emit proper native code based on such patterns.
            
            // Also note that JumpZero is used to check falsey conditions, while
            // JumpNonZero is used to check truey conditions in standard emitted IL
            // code.
            
            Jump,
            JumpZero, // Jump if EAX is zero
            JumpNonZero, // Jump if EAX is non-zero
            
            ////////////////////////////////////////////////////////////////////////
            // Fake opcode marking the last of our opcodes.
            LAST_OPCODE
        };
    }
}
