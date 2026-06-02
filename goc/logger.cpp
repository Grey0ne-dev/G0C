#include "logger.h"

#include <iostream>
#include <streambuf>

class Logger::NullBuffer : public std::streambuf {
public:
    int overflow(int ch) override {
        return ch;
    }
};

bool Logger::debug_enabled = false;
bool Logger::debug_buffered = false;
Logger::NullBuffer Logger::null_buffer;
std::ostream Logger::null_stream(&Logger::null_buffer);
std::ostringstream Logger::debug_buffer;

void Logger::setDebugEnabled(bool enabled) {
    debug_enabled = enabled;
}

void Logger::setDebugBuffered(bool enabled) {
    if (!enabled) {
        flushDebug();
    }
    debug_buffered = enabled;
}

void Logger::flushDebug() {
    const std::string contents = debug_buffer.str();
    if (!contents.empty()) {
        std::cout.flush();
        std::ostream& destination = debug_buffered ? std::cout : std::cerr;
        destination << contents;
        destination.flush();
        debug_buffer.str("");
        debug_buffer.clear();
    }
}

bool Logger::isDebugEnabled() {
    return debug_enabled;
}

std::ostream& Logger::out() {
    return std::cout;
}

std::ostream& Logger::error() {
    return std::cerr;
}

std::ostream& Logger::debug() {
    if (!debug_enabled) {
        return null_stream;
    }
    return debug_buffered ? debug_buffer : std::cerr;
}
