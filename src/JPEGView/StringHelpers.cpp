#include "stdafx.h"
#include "StringHelpers.h"

namespace StringHelpers
{
    std::string WideToUtf8(LPCWSTR wstr) {
        if (!wstr) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        if (len == 0) return {};
        std::string out(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &out[0], len, nullptr, nullptr);
        return out;
    }
};