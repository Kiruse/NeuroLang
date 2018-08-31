////////////////////////////////////////////////////////////////////////////////
// Some non-templated source and global / static variable initialization.
// -----
// Copyright (c) Kiruse 2018 Germany
#include <algorithm>
#include <cctype>
#include <vector>
#include "NeuroTestingFramework.hpp"
#include "Platform.hpp"

#define INDENTATIONSTREAMBUFFER_INTERNALBUFFERSIZE 200

namespace Neuro {
    namespace Testing
    {
        ////////////////////////////////////////////////////////////////////////
        // The stream buffer class we use to automatically wrap lines, as well
        // as indent them according to our indentation level.
        
        class IndentationStreamBuffer : public std::streambuf {
            typedef std::basic_string<char_type> string_type;
            
        public:
            IndentationStreamBuffer(std::streambuf* baseBuffer = std::cout.rdbuf()) : m_baseBuffer(baseBuffer), m_indentationLevel(0), m_endedOnNewLine(true) {}
            
            std::streambuf* baseBuffer() const { return m_baseBuffer; }
            std::streambuf* baseBuffer(std::streambuf* newBuffer) { return m_baseBuffer = newBuffer; }
            
            void indent()     { if (m_indentationLevel < 63) ++m_indentationLevel; }
            void unindent()   { m_indentationLevel = std::max<unsigned int>(0, m_indentationLevel - 1); }
            
        protected:
            virtual int_type overflow(int_type ch = traits_type::eof()) override {
                if (sync() != 0) return traits_type::eof();
                if (ch != traits_type::eof()) {
                    m_internalBuffer[0] = traits_type::to_char_type(ch);
                    pbump(1);
                }
                return ch;
            }
            
            virtual int sync() override {
                if (m_baseBuffer && pbase() != pptr()) {
                    string_type rawString(pbase(), pptr());
                    string_type indentationReplacer(m_indentationLevel * 4, ' ');
                    indentationReplacer.insert(0, "\n");
                    
                    // Before we replace all the new lines with new lines + indentation, check this...
                    bool endsOnNewLine = rawString.length() > 0 && rawString.back() == '\n';
                    
                    // We'll take care of indenting the next input when the next input is synced.
                    if (endsOnNewLine) rawString.pop_back();
                    
                    // Indent at the start of the line, if need be.
                    if (m_endedOnNewLine) rawString.insert(0, string_type(m_indentationLevel * 4, ' '));
                    
                    // Always split by new-line characters.
                    // Note: the std seems to standardize the NL character to \n.
                    std::streamsize pos = rawString.find('\n');
                    while (pos != std::string::npos) {
                        rawString.replace(pos, 1, indentationReplacer);
                        pos = rawString.find('\n', pos + 1);
                    }
                    
                    // We still want to write the new-line.
                    if (endsOnNewLine) rawString.push_back('\n');
                    m_baseBuffer->sputn(rawString.c_str(), rawString.length());
                    m_endedOnNewLine = rawString.length() > 0 && rawString.back() == '\n';
                }
                setp(m_internalBuffer, m_internalBuffer + INDENTATIONSTREAMBUFFER_INTERNALBUFFERSIZE);
                return 0;
            }
            
        private:
            unsigned int m_indentationLevel : 6; // More than 64 indentations, each at 4 characters, is just insane and won't fit any console...
            unsigned int m_endedOnNewLine : 1; // Whether the last sync ended on a new-line character.
            std::streambuf* m_baseBuffer;
            char_type m_internalBuffer[INDENTATIONSTREAMBUFFER_INTERNALBUFFERSIZE];
        };
        
        
        ////////////////////////////////////////////////////////////////////////
        // Globals
        
        std::ostream out(new IndentationStreamBuffer(std::cout.rdbuf()));
        std::ostream err(new IndentationStreamBuffer(std::cerr.rdbuf()));
        
        static std::vector<std::string> testSections;
        
        
        ////////////////////////////////////////////////////////////////////////
        // ostream manipulator functions
        
        std::ostream& indent(std::ostream& stream) {
            stream.flush();
            if (auto buffer = dynamic_cast<IndentationStreamBuffer*>(stream.rdbuf())) {
                buffer->indent();
            }
            return stream;
        }
        
        std::ostream& unindent(std::ostream& stream) {
            stream.flush();
            if (auto buffer = dynamic_cast<IndentationStreamBuffer*>(stream.rdbuf())) {
                buffer->unindent();
            }
            return stream;
        }
        
        ////////////////////////////////////////////////////////////////////////
        // Unit Test section related functions
        
        void pushTestSection(const std::string& name) {
            out << name << std::endl << indent;
            err << indent;
        }
        void popTestSection() {
            out << unindent;
            err << unindent;
        }
    }
}
