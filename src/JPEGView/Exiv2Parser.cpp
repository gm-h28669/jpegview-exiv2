#pragma once
#include "Exiv2Parser.h"
#include <exiv2/exiv2.hpp>
#include <exiv2/exif.hpp>
#include <cstdint> 
#include <string> 

namespace Exiv2Parser {
    std::string GetLensModel(const uint8_t* pApp1Block) {
        if (!pApp1Block || pApp1Block[0] != 0xFF || pApp1Block[1] != 0xE1) {
            return "";
        }

        int nApp1Size = (pApp1Block[2] << 8) | pApp1Block[3]; // JPEG length field (big endian)
        // payload starts after marker (2) + length (2) = offset 4
        const Exiv2::byte* exifPtr = pApp1Block + 4;
        size_t exifSize = static_cast<size_t>(nApp1Size - 2); // length includes the two length bytes, payload = L-2

        // Basic sanity checks
        if (exifSize < 6) return "";

        // The APP1 payload should start with "Exif\0\0". Skip it to get to the TIFF header.
        const unsigned char exifHeader[] = { 'E','x','i','f',0,0 };
        const Exiv2::byte* tiffPtr = exifPtr;
        size_t tiffSize = exifSize;
        if (memcmp(exifPtr, exifHeader, 6) == 0) {
            tiffPtr += 6;
            tiffSize -= 6;
        } else {
            // Not the expected EXIF signature — bail out (or attempt parse anyway)
            return "";
        }

        try {
            Exiv2::ExifData exif;
            Exiv2::ExifParser::decode(exif, tiffPtr, static_cast<long>(tiffSize));

            // try the standard tag first, vendor tags (like OlympusEq) may be different
            Exiv2::ExifKey key("Exif.Photo.LensModel");
            auto it = exif.findKey(key);
            if (it != exif.end()) {
                return it->value().toString();
            }

            // fallback: vendor-specific keys (example)
            Exiv2::ExifKey key2("Exif.OlympusEq.LensModel");
            it = exif.findKey(key2);
            if (it != exif.end()) {
                return it->value().toString();
            }
        }
        catch (Exiv2::Error& e) {
            // Return or log e.what() for diagnostics; don't swallow silently while debugging
            // e.what() will explain why parsing failed (truncated, invalid TIFF, etc.)
            (void)e; // remove this line and log e.what() in your app
        }

        return "";
    }
}
