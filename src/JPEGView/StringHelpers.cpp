#include "StdAfx.h"
#include <windows.h>
#include <algorithm>
#include "StringHelpers.h"
#include <iomanip>
#include <sstream>
#include "Logging.h"

namespace StringHelpers
{
    std::string toUTF8(LPCWSTR wstr) {
        if (!wstr) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        if (len == 0) return {};
        std::string out(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &out[0], len, nullptr, nullptr);
        return out;
    }

    std::string toUpper(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\f\v");
        size_t last = str.find_last_not_of(" \t\n\r\f\v");
        return (first == std::string::npos) ? "" : str.substr(first, (last - first + 1));
    }

    std::tm parseDateTime(const std::string& exifDateTime, const std::string& format) {
        std::tm tm = {};
        std::istringstream ss(exifDateTime);
        ss >> std::get_time(&tm, format.c_str());
        if (ss.fail()) {
            // handle parse failure (tm stays zeroed)
            LOG_ERROR("Failed to parse date: " + exifDateTime);
        }
        return tm;
    }
};