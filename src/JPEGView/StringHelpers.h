#pragma once
#include <string>

namespace StringHelpers
{
    std::string toUTF8(LPCWSTR wstr);
    std::string toUpper(const std::string& str);
    std::string trim(const std::string& str);
    std::tm parseDateTime(const std::string& exifDateTime, const std::string& format);
};

