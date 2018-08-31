////////////////////////////////////////////////////////////////////////////////
// Declares the official, vanilla Neuro IL interpreter.
// ---
// Copyright (c) Kiruse. See license in LICENSE.txt, or online at http://carbon.kirusifix.com/license.

#pragma once

#include "NeuroBuffer.hpp"
#include "NeuroString.hpp"
#include "OpCodes.hpp"

namespace Neuro {
    namespace Lang {
        /**
         * @brief The official default, unflavored Neuro IL interpreter.
         */
        class Interpreter {
        public:
            Interpreter();
            Interpreter(const Interpreter&) = default;
            Interpreter(Interpreter&&) = default;
            virtual Interpreter& operator=(const Interpreter&) = default;
            virtual Interpreter& operator=(Interpreter&&) = default;
            virtual ~Interpreter();
            
        public:
            /**
             * @brief Reads the opcodes from the specified binary file into the internal buffer.
             * @param path neuroString storing the path to the string to read in.
             */
            virtual void readFile(const String& path);
            
            /**
             * @brief Copies the given opcodes over the internal buffer.
             * @param opcodes neuroBuffer storing the opcodes to read in.
             */
            virtual void readOpCodes(const ByteBuffer& opcodes);
            
            /**
             * @brief Executes the internal buffer of OpCodes.
             */
            virtual void execute();
        };
    }
}
