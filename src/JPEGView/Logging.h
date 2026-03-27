#pragma once
#include <string>

namespace Logging {
    void LogError(const std::string&, const char*);
    void LogWarning(const std::string&, const char*);
}

#ifdef LOGGING_ENABLED
    #define LOG_ERROR(msg) Logging::LogError(msg, __func__)
    #define LOG_WARNING(msg) Logging::LogWarning(msg, __func__)
#else
    #define LOG_ERROR(msg) ((void)0)
    #define LOG_WARNING(msg) ((void)0)
#endif
