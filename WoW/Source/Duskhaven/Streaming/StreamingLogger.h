#pragma once
#include <iostream>
#include <sstream>
#include <string>

namespace Duskhaven::Streaming
{
    class LogMessage
    {
    public:
        LogMessage(const char* level) : m_level(level) {}
        ~LogMessage()
        {
            std::cout << "[" << m_level << "] " << m_stream.str() << std::endl;
        }

        template <typename T>
        LogMessage& operator<<(const T& val)
        {
            m_stream << val;
            return *this;
        }

    private:
        const char* m_level;
        std::ostringstream m_stream;
    };
}

#ifndef LOG_DEBUG
#define LOG_DEBUG Duskhaven::Streaming::LogMessage("DEBUG")
#endif
#ifndef LOG_INFO
#define LOG_INFO Duskhaven::Streaming::LogMessage("INFO")
#endif
#ifndef LOG_WARN
#define LOG_WARN Duskhaven::Streaming::LogMessage("WARN")
#endif
#ifndef LOG_ERROR
#define LOG_ERROR Duskhaven::Streaming::LogMessage("ERROR")
#endif
