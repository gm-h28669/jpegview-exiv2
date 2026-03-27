#include "stdafx.h"
#include "Logging.h"
#include <Windows.h>

namespace Logging {
    void LogError(const std::string& msg, const char* functionName) {
        std::string combined = "E: [" + std::string(functionName) + "]: " + msg + "\n";
        OutputDebugStringA(combined.c_str());
    }
    void LogWarning(const std::string& msg, const char* functionName) {
        std::string combined = "W: [" + std::string(functionName) + "]: " + msg + "\n";
        OutputDebugStringA(combined.c_str());
    }
}
