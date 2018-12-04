////////////////////////////////////////////////////////////////////////////////
// An Identifier is a special type of string mapped to a unique integer based
// on a binary search tree which does not support deletion.
// 
// TODO: Track which properties are requested the most and arrange the trees to
// bring these properties up higher.
// -----
// Copyright (c) Kirusifix 2018
// License: GPL 3.0
#pragma once

#include "HashCode.hpp"
#include "Numeric.hpp"

namespace Neuro {
    namespace Runtime
    {
        /**
         * A PoD Type returned by the Identifier Registry Interface.
         * 
         * And yes, it is still a PoD type even if its defaulted default constructor
         * is private. Because technically private, protected and public are
         * only relevant to C++, not to the machine itself.
         */
        struct NEURO_API Identifier {
        private:
            friend NEURO_API hashT calculateHash(Identifier);
            uint32 number;
            
        private:
            Identifier() = default;
            Identifier(uint32 number) : number(number) {}
            
        public:
            Identifier(const Identifier&) = default;
            Identifier(Identifier&&) = default;
            Identifier& operator=(const Identifier&) = default;
            Identifier& operator=(Identifier&&) = default;
            ~Identifier() = default;
            
            bool operator==(const Identifier& other) const { return number == other.number; }
            bool operator!=(const Identifier& other) const { return number != other.number; }
            
            uint32 getUID() const { return number; }
            
            
            static Identifier lookup(const String& name);
            static void resetRegistry();
            
            /**
             * Creates a new Identifier instance with the given UID.
             * 
             * We prefer an explicit static method over turning the equivalent
             * constructor public to make the intention clear: Only use this
             * method if you really need to! It's intended to lookup Identifiers
             * or to create them from other instances.
             */
            static Identifier fromUID(uint32 uid) { return Identifier(uid); }
        };
        
        /**
         * calculateHash specialization for the above defined identifier.
         */
        inline NEURO_API hashT calculateHash(Identifier id) {
            return id.number;
        }
    }
}
