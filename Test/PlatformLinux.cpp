#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Neuro {
    namespace Test {
        namespace Platform {
            namespace Linux
            {
                int getTtyCols() {
                    winsize w;
                    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
                    return w.ws_col;
                }
                
                int getTtyRows() {
                    winsize w;
                    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
                    return w.ws_row;
                }
                
                //uint32 launch(const std::path& path, uint32 timeout, String& stdout, String& stderr, bool& timedout, uint32& extraerror)
                
                namespace fs
                {
                    bool isExecutable(const std::filesystem::path& path) {
                        struct stat pathstat;
                        if (stat(path.c_str(), &pathstat)) {
                            return false;
                        }
                        
                        return pathstat & X_OK;
                    }
                }
            }
        }
    }
}