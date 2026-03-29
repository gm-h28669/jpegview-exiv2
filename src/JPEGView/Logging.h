#pragma once

#include <atlstr.h>
#include <string>

namespace Logging {
    void LogInfo(const std::string& msg, const char* functionName);
    void LogWarning(const std::string&, const char* functionName);
    void LogError(const std::string&, const char* functionName);
	void LogInfoWide(const CString& msg, const char* functionName);
	void LogWarningWide(const CString& msg, const char* functionName);
	void LogErrorWide(const CString& msg, const char* functionName);
}

#ifdef DEBUG
    #define LOG_INFO(msg) Logging::LogInfo(msg, __func__)
    #define LOG_WARNING(msg) Logging::LogWarning(msg, __func__)
    #define LOG_ERROR(msg) Logging::LogError(msg, __func__)
    #define LOG_INFO_WIDE(msg) Logging::LogInfoWide(msg, __func__)
    #define LOG_WARNING_WIDE(msg) Logging::LogWarningWide(msg, __func__)
    #define LOG_ERROR_WIDE(msg) Logging::LogErrorWide(msg, __func__)
#else
    #define LOG_INFO(msg) ((void)0)
    #define LOG_ERROR(msg) ((void)0)
    #define LOG_WARNING(msg) ((void)0)
    #define LOG_INFO_WIDE(msg) ((void)0)
    #define LOG_ERROR_WIDE(msg) ((void)0)
    #define LOG_WARNING_WIDE(msg) ((void)0)
#endif
