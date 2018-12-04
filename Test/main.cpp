////////////////////////////////////////////////////////////////////////////////
// Neuro Unit Test user interface application.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GNU GPL 3.0
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "Platform/Broker.hpp"

#include "Error.hpp"
#include "ExceptionBase.hpp"
#include "NeuroTestingFramework.hpp"
#include "Numeric.hpp"
#include "NeuroBuffer.hpp"
#include "NeuroString.hpp"
#include "NeuroSet.hpp"

using namespace Neuro;
using fs = std::filesystem;

////////////////////////////////////////////////////////////////////////////////
// Helper structs & classes

struct UnitTestResult
{
    /**
     * Absolute path to the unit test.
     */
    std::path testPath;
    
    int32 exitCode;
    bool timedout;
    Buffer<String> stdout;
    Buffer<String> stderr;
    
    operator bool() const { return error == NoError::instance(); }
};


////////////////////////////////////////////////////////////////////////////////
// Command line parsing stuff

String getCmdOption(const Buffer<String>& args, const String& opt) {
    auto it = std::find(args.begin(), args.end(), opt);
    if (it != args.end() && ++it != args.end()) {
        return *it;
    }
    return "";
}

bool hasCmdOption(const Buffer<String>& args, const String& opt) {
    return std::find(args.begin(), args.end(), opt) != args.end();
}


////////////////////////////////////////////////////////////////////////////////
// Forward declarations of helper functions

Buffer<fs::path> listUnitTests(const fs::path& path);
Buffer<UnitTestResult> executeUnitTests(const Buffer<fs::path>& tests);


////////////////////////////////////////////////////////////////////////////////
// Subcommand Declarations

void printHelp() {
    using namespace std;
    cout << "Commands:" << endl;
    cout << "  help"    << endl;
    cout << "  ls"      << endl;
    cout << "  execute" << endl;
    cout << "  list"    << endl;
}

//------------------------------------------------------------------------------
// ls command

int cmd_ls(const Buffer<String>& args) {
    using namespace std;
    
    if (hasCmdOption(args, "--help") || hasCmdOption(args, "-h")) {
        cout << "Discovers and lists unit tests." << endl << endl;
        
        cout << "Options:" << endl << endl;
        
        cout << "  --help, -h" << endl;
        cout << "    Show this help." << endl << endl;
        
        cout << "  --dir" << endl;
        cout << "    Specify directory to search for unit tests. Defaults to \"./tests\"." << endl << endl;
        
        cout << "  --json" << endl;
        cout << "    Print list of unit tests in a JSON array format." << endl << endl;
        
        return 0;
    }
    
    String path = getCmdOption(args, "--dir");
    if (!path.length()) {
        path = "tests";
    }
    
    const Buffer<std::path> paths = listUnitTests(path);
    
    // Format as JSON array
    if (hasCmdOption(args, "--json")) {
        cout << '[';
        for (uint32 i = 0; i < paths.length(); ++i) {
            cout << paths[i];
            
            if (i != paths.length() - 1) {
                cout << ',';
            }
        }
        cout << ']' << endl;
    }
    
    // Otherwise one unit test per line
    else {
        for (auto path : paths) {
            cout << path << endl;
        }
    }
    
    return 0;
}


//------------------------------------------------------------------------------
// execute command & subcommands

int cmd_execute(const Buffer<String>& args) {
    int exitcode = 0;
    bool showHelp = hasCmdOption(args, "help") || hasCmdOption(args, "-h");
    
    if (!showHelp) {
        if (args[1] == "list") {
            return subcmd_execute_lists(args);
        }
        else if (args[1] == "tests") {
            return subcmd_execute_tests(args);
        }
        else {
            showHelp = true;
            exitcode = 1;
        }
    }
    
    if (showHelp) {
        using namespace std;
        cout << "Subcommands:" << endl;
        cout << "  list"  << endl;
        cout << "  tests" << endl;
        cout << "  all"   << endl;
        
        cout << endl;
        
        cout << "Options:" << endl << endl;
        
        cout << "  --dir" << endl;
        cout << "    Directory from which to read the unit tests. Defaults to \"./tests\"" << endl << endl;
        
        cout << "  --timeout, -t" << endl;
        cout << "    Timeout in milliseconds to grant each test before mercy killing." << endl << endl;
    }
    
    return exitcode;
}

int subcmd_execute_list(const Buffer<String>& args) {
    std::cerr << "Not yet implemented" << std::endl;
    return 2;
}

int subcmd_execute_tests(const Buffer<String>& args) {
    std::cerr << "Not yet implemented" << std::endl;
    return 2;
}

int subcmd_execute_all(const Buffer<String>& args) {
    String testsDir = getCmdOption(args, "--dir");
    if (!testsDir.length()) {
        testsDir = "tests";
    }
    
    // Parse timeout option
    uint32 timeout = 5000;
    String strTimeout = getCmdOption(args, "--timeout");
    if (!strTimeout.length()) strTimeout = getCmdOption(args, "-t");
    if (strTimeout.length() && strTimeout[0] >= '0' && strTimeout[0] < '9') {
        timeout = 0;
        for (auto cdigit : strTimeout) {
            if (cdigit < '0' || cdigit > '9') break;
            const uint8 digit = '0' - cdigit;
            timeout = 10 * timeout + digit;
        }
    }
    
    // Retrieve list of all unit tests
    Buffer<UnitTestResult> results = executeUnitTests(listUnitTests(testsDir));
    
    // TODO: Iterate through the results
    
    std::cerr << "Not yet implemented" << std::endl;
    return 2;
}

//------------------------------------------------------------------------------
// list command & subcommands

int cmd_list(const Buffer<String>& args) {
    const String path("lists.txt");
    
    int exitcode = 0;
    bool showHelp = hasCmdOption(args, "help") || hasCmdOption(args, "-h");
    
    if (!showHelp) {
        if (args[1] == "ls") {
            exitcode = subcmd_list_ls(args);
        }
        else if (args[1] == "show") {
            exitcode = subcmd_list_show(args);
        }
        else if (args[1] == "create") {
            exitcode = subcmd_list_create(args);
        }
        else if (args[1] == "delete") {
            exitcode = subcmd_list_delete(args);
        }
        else {
            showHelp = true;
            exitcode = 1;
        }
    }
    
    if (showHelp) {
        using namespace std;
        cout << "Subcommands:" << endl;
        cout << "  ls"     << endl;
        cout << "  show"   << endl;
        cout << "  create" << endl;
        cout << "  delete" << endl;
    }
}

int subcmd_list_ls(const Buffer<String>& args) {
    std::cerr << "Not yet implemented" << std::endl;
    return 2;
}

int subcmd_list_show(const Buffer<String>& args) {
    std::cerr << "Not yet implemented" << std::endl;
    return 2;
}

int subcmd_list_create(const Buffer<String>& args) {
    std::cerr << "Not yet implemented" << std::endl;
    return 2;
}

int subcmd_list_delete(const Buffer<String>& args) {
    std::cerr << "Not yet implemented" << std::endl;
    return 2;
}



////////////////////////////////////////////////////////////////////////////////
// Main Application

int main(int argc, char* argv[]) {
    // Store the args in a more convenient buffer.
    Buffer<String> args;
    for (int32 i = 0; i < argc; ++i) {
        args.add(argv[i]);
    }
    
    if (args[0] == "ls") {
        return cmd_ls(args);
    }
    else if (args[0] == "execute") {
        return cmd_execute(args);
    }
    else if (args[0] == "list") {
        return cmd_list(args);
    }
    
    // Help on request (w/ exit code 0)
    else if (args[0] == "help") {
        printHelp();
        return 0;
    }
    // Help due to unknown command (w/ exit code 1)
    else {
        printHelp();
        return 1;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Implementation of forward declared helper functions

Buffer<fs::path> listUnitTests(const fs::path& path) {
    Buffer<std::path> result;
    
    if (!path.exists()) return result;
    
    for (fs::directory_iterator dir(path); dir; ++dir) {
        if (dir->is_regular_file() && Testing::fs::isExecutable(dir->path())) {
            paths.add(dir->path());
        }
    }
    
    return paths;
}

Buffer<UnitTestResult> executeUnitTests(const Buffer<std::path>& tests) {
    Buffer<UnitTestResult> results;
    
    // Buffer for stdout and stderr of the current unit test.
    String stdout, stderr;
    
    // Launch all unit tests, one after another
    for (auto path : pathsToTests) {
        UnitTestResult testresult;
        testresult.testPath = path;
        
        uint32 extraerror;
        testresult.exitCode = Test::Platform::launch(path, timeout, testresult.stdout, testresult.stderr, testresult.timedout, extraerror);
        
        results.add(testresult);
    }
    
    return results;
}
