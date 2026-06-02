#ifndef GOC_LOGGER_H
#define GOC_LOGGER_H

#include <ostream>
#include <sstream>

class Logger {
public:
    static void setDebugEnabled(bool enabled);
    static void setDebugBuffered(bool enabled);
    static void flushDebug();
    static bool isDebugEnabled();

    static std::ostream& out();
    static std::ostream& error();
    static std::ostream& debug();

private:
    class NullBuffer;

    static bool debug_enabled;
    static bool debug_buffered;
    static NullBuffer null_buffer;
    static std::ostream null_stream;
    static std::ostringstream debug_buffer;
};

#endif // GOC_LOGGER_H
