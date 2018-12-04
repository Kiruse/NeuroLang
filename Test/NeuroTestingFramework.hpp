////////////////////////////////////////////////////////////////////////////////
// A slightly elaborate unit testing framework that the Neuro testing suite can
// use to easily define unit tests.
// 
// The formatting of Neuro::Testing::out and err only works reliably under
// Windows and Linux. Other operating systems might work as well.
// 
// !!! Currently only supports narrow characters !!!
// Because screw making myself more work with trying to find a standardized
// solution to single- and multibyte characters with different encodings! Where
// possible, code has been templated in regular C++ standard fashion, which
// should make it a bit easier to transpire the library to multibyte character
// strings with different encodings in the future.
// 
// TODO: Buffer section & test output and write at the end of the program.
// This allows us to sort the results beforehand and maybe do other nice things.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GNU GPL 3.0
#pragma once

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include "ExceptionBase.hpp"

namespace Neuro
{
    namespace Testing
    {
        /**
         * @brief The output stream which should be used to write messages.
         */
        extern std::ostream out;
        
        /**
         * @brief The output stream which should be used to report errors.
         */
        extern std::ostream err;
        
        /**
         * @brief Attempt to indent the ostream's underlying streambuf object.
         * 
         * Only works if the underlying buffer object is the testing framework's
         * internal buffer object (whose class is not exposed as of the writing
         * of this documentation, but may be in the future).
         * 
         * Intended to be used as manipulator, like Neuro::Testing::out << indent;
         * 
         * @param stream 
         * @return ostream& 
         */
        std::ostream& indent(std::ostream& stream);
        
        /**
         * @brief Attempt to indent the ostream's underlying streambuf object.
         * 
         * Only works if the underlying buffer object is the testing framework's
         * internal buffer object (whose class is not exposed as of the writing
         * of this documentation, but may be in the future).
         * 
         * Intended to be used as manipulator, like Neuro::Testing::out << unindent;
         * 
         * @param stream 
         * @return ostream& 
         */
        std::ostream& unindent(std::ostream& stream);
        
        void pushTestSection(const std::string& name);
        void popTestSection();
        
        ////////////////////////////////////////////////////////////////////////
        // Testing Section Management
        
        template<typename Closure>
        void section(const std::string& name, const Closure& closure) {
            pushTestSection(name);
            try {
                closure();
            }
            catch (...) {}
            popTestSection();
        }
        
        template<typename Closure>
        void test(const std::string& name, const Closure& closure) {
            out << name << " ... " << indent;
            err << indent;
            
            try {
                closure();
                out << "PASSED" << std::endl;
            }
            catch (Exception ex) {
                out << "FAILED" << std::endl;
                err << ex << std::endl;
            }
            catch (...) {
                out << "FAILED" << std::endl;
            }
            
            out << unindent;
            err << unindent;
        }
    }
}
