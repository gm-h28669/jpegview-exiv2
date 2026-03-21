#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "Exiv2Parser.h"
#include <exiv2/exiv2.hpp>
#include <exiv2/exif.hpp>
#include <cstdint> 
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include "StringHelpers.h">


#define IS_VALID_TAG(iter, exif) (iter != (exif).end() && (iter)->size())

namespace Exiv2Parser {
    constexpr uint16_t APP1_MARKER = 0xFFE1;

    static void logError(const std::string& errorMessage, const char* functionName) {
        std::cerr << "E:" << functionName << ": " << errorMessage << std::endl;
    }

    static std::tm parseDateTime(const std::string& exifDateTime, const std::string& format) {
        std::tm tm = {};
        std::istringstream ss(exifDateTime);
        ss >> std::get_time(&tm, format.c_str());
        if (ss.fail()) {
            // handle parse failure (tm stays zeroed)
            logError("Failed to parse date: " + exifDateTime, __func__);
        }
        return tm;
    }

    static bool findExifTag(Exiv2::ExifData& exif, std::string key, Exiv2::ExifData::const_iterator& iter) {
        iter = exif.findKey(Exiv2::ExifKey(key));
        return (iter != exif.end() && iter->size() > 0);
    }

    static std::string toUpper(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\f\v");
        size_t last = str.find_last_not_of(" \t\n\r\f\v");
        return (first == std::string::npos) ? "" : str.substr(first, (last - first + 1));
    }

    double convertRationalToDouble(const rational& number) {
        return (number.Denominator != 0)
            ? (double)number.Numerator / number.Denominator
            : std::numeric_limits<double>::quiet_NaN();
    }

    static void loadImageMetadata(Exiv2::ExifData& exif, Exiv2Parser::imageMetadata& imageMeta) {
        Exiv2::ExifData::const_iterator iter;

        // get camera make
        iter = Exiv2::make(exif);
        if (IS_VALID_TAG(iter, exif)) {
            imageMeta.make = trim(iter->value().toString());
        }

        // get camera model
        iter = Exiv2::model(exif);
        if (IS_VALID_TAG(iter, exif)) {
            imageMeta.model = trim(iter->value().toString());
        }

        // get user comment
        if (findExifTag(exif, "Exif.Photo.UserComment", iter)) {
            imageMeta.userComment = trim(iter->value().toString());
        }

        // get description
        if (findExifTag(exif, "Exif.Image.ImageDescription", iter)) {
            imageMeta.description = trim(iter->value().toString());
        }

        // get software that created the image
        if (findExifTag(exif, "Exif.Image.Software", iter)) {
            imageMeta.software = trim(iter->value().toString());
        }

        // get date taken
        iter = Exiv2::dateTimeOriginal(exif);
        if (IS_VALID_TAG(iter, exif)) {
            std::string dateTaken = iter->value().toString();
            imageMeta.dateTaken = parseDateTime(dateTaken, "%Y:%m:%d %H:%M:%S");
        }

        // get date modified (this is the date when exif metadata was last modified)
        if (findExifTag(exif, "Exif.Image.DateTime", iter)) {
            std::string dateModified = iter->value().toString();
            imageMeta.dateModified = parseDateTime(dateModified, "%Y:%m:%d %H:%M:%S");
        }

        // get image orientation
        iter = Exiv2::orientation(exif);
        if (IS_VALID_TAG(iter, exif)) {
            imageMeta.orientation = iter->value().toUint32();
        }

        // get exposure time (shutter speed)
        iter = Exiv2::exposureTime(exif);
        if (IS_VALID_TAG(iter, exif)) {
            Exiv2::Rational exposureTime = iter->value().toRational();
            imageMeta.exposureTime = rational{ exposureTime.first, exposureTime.second };
        }

        // get exposure bias (EV compensation)
        iter = Exiv2::exposureBiasValue(exif);
        if (IS_VALID_TAG(iter, exif)) {
            Exiv2::Rational exposureBias = iter->value().toRational();
            imageMeta.exposureBias = rational{ exposureBias.first, exposureBias.second };
        }

        // get aperture (f-number)
        iter = Exiv2::fNumber(exif);
        if (IS_VALID_TAG(iter, exif)) {
            imageMeta.aperture = iter->value().toFloat();
        }

        // get ISO speed
        iter = Exiv2::isoSpeed(exif);
        if (IS_VALID_TAG(iter, exif)) {
            imageMeta.isoSpeed = iter->value().toUint32();
        }
        else {
            // Exif.Photo.ISOSpeed is alternative location as per Exif 3.0 specification
            if (findExifTag(exif, "Exif.Photo.ISOSpeed", iter)) {
                imageMeta.isoSpeed = iter->value().toUint32();
            }
        }

        // get white balance mode
        iter = Exiv2::whiteBalance(exif);
        if (IS_VALID_TAG(iter, exif)) {
            imageMeta.whiteBalance = iter->value().toUint32();
        }

        // get flash fired
        iter = Exiv2::flash(exif);
        if (IS_VALID_TAG(iter, exif)) {
            uint32_t flashValue = iter->value().toUint32();
            // bit 0 of the flash value indicates whether flash was fired or not
            imageMeta.flashFired = (flashValue & 0x01) != 0;
        }

        // get lens model
        iter = Exiv2::lensName(exif);
        if (IS_VALID_TAG(iter, exif)) {
            imageMeta.lensName = iter->print(&exif);
        }

        // get lens info (4 rational values: min focal length, max focal length, min aperture, max aperture)
        if (findExifTag(exif, "Exif.Photo.LensSpecification", iter)) {
            const Exiv2::Value& value = iter->value();
            if (value.count() == 4) {
                for (int i = 0; i < 4; ++i) {
                    Exiv2::Rational lensInfo = value.toRational(i);
                    imageMeta.lensInfo[i] = rational{ lensInfo.first, lensInfo.second };
                }
            }
        }

        // get focal length
        iter = Exiv2::focalLength(exif);
        if (IS_VALID_TAG(iter, exif)) {
            Exiv2::Rational focalLength = iter->value().toRational();
            imageMeta.focalLength = focalLength.second != 0
                ? static_cast<double>(focalLength.first) / focalLength.second
                : std::numeric_limits<double>::quiet_NaN();
        }

        // get 35mm focal length equivalent
        if (findExifTag(exif, "Exif.Photo.FocalLengthIn35mmFilm", iter)) {
            imageMeta.focalLengthEquivalent = iter->value().toFloat();
        }

        // get GPS latitude relative to equator: N or S
        bool hasLatitude = findExifTag(exif, "Exif.GPSInfo.GPSLatitudeRef", iter);
        if (hasLatitude) {
            std::string latitudeRef = trim(iter->value().toString());
            imageMeta.gps.latitude.relativePos = latitudeRef;
        }

        // get GPS latitude in degrees, minutes, seconds
        if (hasLatitude && findExifTag(exif, "Exif.GPSInfo.GPSLatitude", iter)) {
            const Exiv2::Value& value = iter->value();
            if (value.count() == 3) {
                Exiv2::Rational degrees = value.toRational(0);
                Exiv2::Rational minutes = value.toRational(1);
                Exiv2::Rational seconds = value.toRational(2);
                imageMeta.gps.latitude.degrees = static_cast<double>(degrees.first) / degrees.second;
                imageMeta.gps.latitude.minutes = static_cast<double>(minutes.first) / minutes.second;
                imageMeta.gps.latitude.seconds = static_cast<double>(seconds.first) / seconds.second;
                imageMeta.gps.positionAvailable = true;
            }
        }

        // get GPS longitude relative to Greenwich meridian: E or W
        bool hasLongitude = findExifTag(exif, "Exif.GPSInfo.GPSLongitudeRef", iter);
        if (hasLongitude) {
            std::string longitudeRef = trim(iter->value().toString());
            imageMeta.gps.longitude.relativePos = longitudeRef;
        }

        // get GPS longitude in degrees, minutes, seconds
        if (hasLongitude && findExifTag(exif, "Exif.GPSInfo.GPSLongitude", iter)) {
            const Exiv2::Value& value = iter->value();
            if (value.count() == 3) {
                Exiv2::Rational degrees = value.toRational(0);
                Exiv2::Rational minutes = value.toRational(1);
                Exiv2::Rational seconds = value.toRational(2);
                imageMeta.gps.longitude.degrees = static_cast<double>(degrees.first) / degrees.second;
                imageMeta.gps.longitude.minutes = static_cast<double>(minutes.first) / minutes.second;
                imageMeta.gps.longitude.seconds = static_cast<double>(seconds.first) / seconds.second;
                imageMeta.gps.positionAvailable = true;
            }
        }

        // get GPS altitude relative to sea level: 0 = above sea level, 1 = below sea level
        bool hasAltitude = findExifTag(exif, "Exif.GPSInfo.GPSAltitudeRef", iter);
        std::uint32_t altitudeRef = hasAltitude ? iter->value().toUint32() : 0;

        // get GPS altitude in meters; take into consideration the reference (above or below sea level)
        if (hasAltitude && findExifTag(exif, "Exif.GPSInfo.GPSAltitude", iter)) {
            Exiv2::Rational altitudeRaw = iter->value().toRational();
            double altitude = static_cast<double>(altitudeRaw.first) / altitudeRaw.second;
            imageMeta.gps.altitude = altitudeRef == 0 ? altitude : -altitude;
            imageMeta.gps.positionAvailable = true;
        }

        imageMeta.loaded = true;
    }

    imageMetadata getExifMetadata(const uint8_t* pApp1Block) {
        imageMetadata imageMeta;

        // check if APP1 segment starts with the correct marker
        if ((!pApp1Block || ((pApp1Block[0] << 8) | pApp1Block[1]) != APP1_MARKER)) {
            logError("Invalid APP1 segment: missing or incorrect marker", __func__);
            return imageMeta;
        }

        // get length of APP1 segment (big endian) 
        int nApp1Size = (pApp1Block[2] << 8) | pApp1Block[3];

        // caclculate length of exif segment
        const Exiv2::byte* exifPtr = pApp1Block + 4;          // starts after marker and length = offset 4
        size_t exifSize = static_cast<size_t>(nApp1Size - 2); // length includes the two length bytes, segment = L-2

        // basic sanity check
        if (exifSize < 6) {
            logError("APP1 segment too short to contain valid EXIF data", __func__);
            return imageMeta;
        }

        // APP1 segment expected to start with "Exif\0\0"
        const unsigned char exifHeader[] = { 'E', 'x', 'i', 'f', 0, 0 };
        const Exiv2::byte* tiffPtr = exifPtr;
        size_t tiffSize = exifSize;
        if (memcmp(exifPtr, exifHeader, 6) == 0) {
            // skip it to get to the TIFF header.
            tiffPtr += 6;
            tiffSize -= 6;
        }
        else {
            // Not the expected EXIF signature
            logError("APP1 segment does not start with expected EXIF header", __func__);
            return imageMeta;
        }

        if (tiffSize < 2) {
            // invalid TIFF header
            logError("Invalid TIFF header", __func__);
            return imageMeta;
        }

        const unsigned char* p = reinterpret_cast<const unsigned char*>(tiffPtr);
        // little-endian ("II")
        imageMeta.littleEndian = (p[0] == 'I' && p[1] == 'I');

        try {
            Exiv2::ExifData exif;
            Exiv2::ExifParser::decode(exif, tiffPtr, static_cast<long>(tiffSize));


            loadImageMetadata(exif, imageMeta);
            return imageMeta;
        }
        catch (Exiv2::Error& e) {
            logError(std::string("Exif parsing error: ") + e.what(), __func__);
        }

        imageMeta.loaded = true;
        return imageMeta;
    }

    imageMetadata getExifMetadata(LPCWSTR imagePath) {
        imageMetadata imageMeta;

        std::string imagePathUtf8 = StringHelpers::WideToUtf8(imagePath);
        Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(imagePath);
        if (!image) {
            logError("Failed to open image: " + imagePathUtf8, __func__);
            return imageMeta;
        }

        try {
            image->readMetadata();

			// get indication if little or big endian byte order is used in the EXIF metadata
            Exiv2::ByteOrder byteOrder = image->byteOrder();
            imageMeta.littleEndian = (byteOrder == Exiv2::littleEndian);

            Exiv2::ExifData& exif = image->exifData();

            if (exif.empty()) {
                logError("No Exif metadata found in image" + imagePathUtf8, __func__);
            }

            loadImageMetadata(exif, imageMeta);
            return imageMeta;
        }
        catch (Exiv2::Error& e) {
            logError(std::string("Exif parsing error: ") + e.what(), __func__);
        }
    }
}
