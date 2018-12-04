////////////////////////////////////////////////////////////////////////////////
// Provides windows-specific platform implementations.
// -----
// Copyright (c) Kiruse 2018 Germany
#pragma once

#include <windows.h>

#include "NeuroString.hpp"

namespace Neuro {
    namespace Test {
        namespace Platform {
            namespace Windows
            {
                ////////////////////////////////////////////////////////////////
                // Internally used functions
                
                bool createStdPipes(HANDLE& stdin_read, HANDLE& stdin_write, HANDLE& stdout_read, HANDLE& stdout_write, HANDLE& stderr_read, HANDLE& stderr_write) {
                    if (!createPipe(stdin_read,  stdin_write,  TRUE)) {
                        return false;
                    }
                    
                    if (!createPipe(stdout_read, stdout_write, TRUE)) {
                        CloseHandle(stdin_read);
                        CloseHandle(stdin_write);
                        return false;
                    }
                    
                    if (!createPipe(stderr_read, stdierrwrite, TRUE)) {
                        CloseHandle(stdin_read);
                        CloseHandle(stdin_write);
                        CloseHandle(stdout_read);
                        CloseHandle(stdout_write);
                        return false;
                    }
                    
                    setInteritable(stdin_write, FALSE);
                    setInteritable(stdout_read, FALSE);
                    setInteritable(stderr_read, FALSE);
                    
                    return true;
                }
                
                bool createPipe(HANDLE& readEnd, HANDLE& writeEnd, BOOL inheritable) {
                    SECURITY_ATTRIBUTES sa;
                    sa.nLength = sizeof(sa);
                    sa.bInheritHandle = inheritable;
                    sa.lpSecurityDescriptor = NULL;
                    return !!CreatePipe(&readEnd, &writeEnd, &sa, 0);
                }
                
                void closePipe(HANDLE& readEnd, HANDLE& writeEnd) {
                    CloseHandle(readEnd);
                    CloseHandle(writeEnd);
                }
                
                void drainPipe(HANDLE& pipeRead, String& buffer) {
                    CHAR buff[1024];
                    DWORD bytesRead;
                    
                    do {
                        ReadFile(pipeRead, buff, sizeof(buffer) / sizeof(CHAR), &bytesRead, NULL);
                        stdout.add(buff, buff + bytesRead);
                    }
                    while (bytesRead);
                }
                
                bool setInheritable(HANDLE handle, BOOL inheritable) {
                    return !!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, inheritable ? 1 : 0);
                }
                
                
                ////////////////////////////////////////////////////////////////
                // Interface function implementations
                
                int getTtyCols() {
                    CONSOLE_SCREEN_BUFFER_INFO csbi;
                    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
                    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
                }
                
                int getTtyRows() {
                    CONSOLE_SCREEN_BUFFER_INFO csbi;
                    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
                    return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
                }
                
                inline uint32 launch(const std::path& path, uint32 timeout = (uint32)-1, String& stdout, String& stderr, bool& timedout, uint32& extraerror) {
                    stdout.clear();
                    stderr.clear();
                    timedout = false;
                    extraerror = 0;
                    
                    // Pipes for stdin, out & err, read & write ends
                    HANDLE hstdinr, hstdinw, hstdoutr, hstdoutw, hstderrr, hstderrw;
                    
                    if (!createStdPipes(hstdinr, hstdinw, hstdoutr, hstdoutw, hstderrr, hstderrw)) {
                        extraerror = (uint32)GetLastError();
                        return LAUNCH_WINERR;
                    }
                    
                    // Process startup information
                    STARTUPINFOA si;
                    ZeroMemory(&si, sizeof(si));
                    si.cb = sizeof(si);
                    si.hStdInput   = hstdinr;
                    si.hStdOutput  = hstdoutw;
                    si.hStdError   = hstderrw;
                    si.dwFlags    |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
                    si.wShowWindow = SW_HIDE;
                    
                    // Process identification information
                    PROCESS_INFORMATION pi;
                    ZeroMemory(&pi, sizeof(pi));
                    
                    // Finally create the process
                    if (!CreateProcess(NULL, path.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
                        closePipe(hstdinr, hstdinw);
                        closePipe(hstdoutr, hstdoutw);
                        closePipe(hstderrr, hstderrw);
                        extraerror = (uint32)GetLastError();
                        return LAUNCH_WINERR;
                    }
                    
                    // Wait for the program to complete
                    DWORD waitResult = WaitForSingleObject(pi.hProcess, (DWORD)timeout);
                    
                    // Did we time out?
                    if (waitResult == WAIT_TIMEOUT) {
                        timedout = true;
                    }
                    
                    // Close process resources regardless of timeout
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    
                    // Extract all data from the pipes
                    drainPipe(hstdoutr, stdout);
                    drainPipe(hstderrr, stderr);
                    
                    // If we timed out, we return -2 (-1 indicates windows error)
                    if (timedout) {
                        return LAUNCH_TIMEOUT;
                    }
                    
                    // Retrieve exit code
                    DWORD exitcode;
                    if (!GetExitCodeProcess(pi.hProcess, &exitcode)) {
                        extraerror = (uint32)GetLastError();
                        return LAUNCH_WINERR;
                    }
                    return (uint32)exitcode;
                }
                
                namespace fs
                {
                    
                }
            }
        }
    }
}
