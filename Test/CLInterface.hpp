////////////////////////////////////////////////////////////////////////////////
// Defines the CLI outbound interface to allow secondary and third party
// programs to read and parse the results of the unit test.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
// -----
// TODO: Implement dependency on other tests
////////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <stdexcept>
#include "Delegate.hpp"
#include "ExceptionBase.hpp"
#include "NeuroString.hpp"

namespace Neuro {
    namespace Testing
    {
        using ResultCallback = Delegate<void>;
        
        class AssertionException : public Exception {
        public:
            AssertionException() : Exception("AssertionException", "Assertion failed") {}
            AssertionException(const std::string& message) : Exception("AssertionException", message) {}
        };
        
        void error(const String& message) {
            std::cout << "error " << message << std::endl;
        }
        
        class Section {
        protected: // Member fields
            String name;
            String type;
            bool instantEnter;
            bool entered;
            
        public:
            Section(const String& name, bool instantEnter = true) : name(name), type("section"), instantEnter(instantEnter), entered(false) {
                if (instantEnter) {
                    enter();
                }
            }
            
            ~Section() {
                if (instantEnter) {
                    leave();
                }
            }
            
            void enter() {
                if (entered) throw new std::logic_error("Already entered");
                entered = true;
                enterMsg();
            }
            
            void leave() {
                if (!entered) throw new std::logic_error("Not yet entered");
                entered = false;
                leaveMsg();
            }
            
            template<typename CALLBACK>
            void use(CALLBACK& callback) {
                if (entered) throw new std::logic_error("Already entered");
                
                enterMsg();
                callback();
                leaveMsg();
            }
            
        protected:
            void enterMsg() {
                std::cout << "enter " << type << " " << name << std::endl;
            }
            
            void leaveMsg() {
                std::cout << "leave " << type << " " << name << std::endl;
            }
        };
        
        class Test : public Section {
        public:
            Test(const String& name, bool instantEnter = false) : Section(name, instantEnter) {
                type = "test";
            }
        };
        
        template<typename CALLBACK>
        void section(const String& name, CALLBACK& callback) {
            Section sect(name, false);
            sect.use(callback);
        }
        
        template<typename CALLBACK>
        void test(const String& name, CALLBACK& callback) {
            Test tst(name, false);
            tst.enter();
            
            try {
                callback();
            }
            catch (std::exception* ex) {
                error(formatException(ex));
            }
            catch (Exception* ex) {
                error(formatException(ex));
            }
            catch (...) {
                error("Unknown error");
            }
            
            tst.leave();
        }
        
        void assert(bool truthy, const String& message) {
            if (!truthy) throw new AssertionException(message.c_str());
        }
        
        /**
         * Wraps the body of a thread in order to catch and properly handle
         * assertions and catch exceptions properly.
         */
		template<typename CALLBACK, typename... ARGS>
        auto thread(CALLBACK& body, ARGS... args) {
			return [&body]() {
				try {
					body(std::forward<ARGS>(args)...);
				}
				catch (std::exception* ex) {
					error(formatException(ex));
				}
				catch (Exception* ex) {
					error(formatException(ex));
				}
				catch (...) {
					error("Unknown error");
				}
			};
        }
        
        
        String formatException(std::exception* ex) {
            return ex->what();
        }
        
        String formatException(Exception* ex) {
            String message = (ex->title() + ": " + ex->message()).c_str();
            if (ex->cause()) {
                message += " (and more)";
            }
            return message;
        }
    }
}
