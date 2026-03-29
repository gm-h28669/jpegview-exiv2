#include "stdafx.h"
#include "Logging.h"

namespace Logging {
    static void Log(char severity, const std::string& msg, const char* functionName) {
        std::string combined = std::string("[") + severity + "] " + functionName + ": " + msg + "\n";
        OutputDebugStringA(combined.c_str());
    }
    static void LogWide(const wchar_t severity, const CString& msg, const char* functionName) {
        CString combined = CString(_T("[")) + severity + _T("] ") + functionName + _T(": ") + msg + _T("\n");
        OutputDebugStringW(combined);
    }
    void LogInfo(const std::string& msg, const char* functionName) {
		Log('I', msg, functionName);
    }
    void LogWarning(const std::string& msg, const char* functionName) {
		Log('W', msg, functionName);
    }
    void LogError(const std::string& msg, const char* functionName) {
        Log('E', msg, functionName);
    }
    void LogInfoWide(const CString& msg, const char* functionName) {
        LogWide(_T('I'), msg, functionName);
    }
    void LogWarningWide(const CString& msg, const char* functionName) {
        LogWide(_T('W'), msg, functionName);
    }
    void LogErrorWide(const CString& msg, const char* functionName) {
        LogWide(_T('E'), msg, functionName);
    }
}
