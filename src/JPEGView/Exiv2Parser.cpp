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
#include "StringHelpers.h"
#include "Logging.h"

#define IS_VALID_EXIF_TAG(iter, exif) (iter != exif.end() && iter->size())
#define IS_VALID_XMP_TAG(iter, xmp) (iter != xmp.end() && iter->size())

namespace Exiv2Parser {
    static bool findExifTag(Exiv2::ExifData& exif, std::string key, Exiv2::ExifData::const_iterator& iter) {
        iter = exif.findKey(Exiv2::ExifKey(key));
        return (iter != exif.end() && iter->size() > 0);
    }

    static bool findXmpTag(Exiv2::XmpData& xmp, std::string key, Exiv2::XmpData::const_iterator& iter) {
        iter = xmp.findKey(Exiv2::XmpKey(key));
        return (iter != xmp.end() && iter->size() > 0);
    }

    static void loadXmpMeta(Exiv2::XmpData& xmp, Exiv2Parser::imageMetadata& imageMeta) {
        Exiv2::XmpData::const_iterator iter;

        // get image rating
        if (findXmpTag(xmp, "Xmp.xmp.Rating", iter)) {
            imageMeta.rating = iter->value().toUint32();
        }

    }

    static void loadExifMeta(Exiv2::ExifData& exif, Exiv2Parser::imageMetadata& imageMeta) {
        Exiv2::ExifData::const_iterator iter;

        // get camera make
        iter = Exiv2::make(exif);
        if (IS_VALID_EXIF_TAG(iter, exif)) {
            imageMeta.make = StringHelpers::trim(iter->value().toString());
        }

        // get camera model
        iter = Exiv2::model(exif);
        if (IS_VALID_EXIF_TAG(iter, exif)) {
            imageMeta.model = StringHelpers::trim(iter->value().toString());
        }

        // get user comment
        if (findExifTag(exif, "Exif.Photo.UserComment", iter)) {
            imageMeta.userComment = StringHelpers::trim(iter->value().toString());
        }

        // get description
        if (findExifTag(exif, "Exif.Image.ImageDescription", iter)) {
            imageMeta.description = StringHelpers::trim(iter->value().toString());
        }

        // get software that created the image
        if (findExifTag(exif, "Exif.Image.Software", iter)) {
            imageMeta.software = StringHelpers::trim(iter->value().toString());
        }

        // get date taken
        iter = Exiv2::dateTimeOriginal(exif);
        if (IS_VALID_EXIF_TAG(iter, exif)) {
            std::string dateTaken = iter->value().toString();
            imageMeta.dateTaken = StringHelpers::parseDateTime(dateTaken, "%Y:%m:%d %H:%M:%S");
        }

        // get date modified (this is the date when exif metadata was last modified)
        if (findExifTag(exif, "Exif.Image.DateTime", iter)) {
            std::string dateModified = iter->value().toString();
            imageMeta.dateModified = StringHelpers::parseDateTime(dateModified, "%Y:%m:%d %H:%M:%S");
        }

        // get image orientation
        iter = Exiv2::orientation(exif);
        if (IS_VALID_EXIF_TAG(iter, exif)) {
            imageMeta.orientation = iter->value().toUint32();
        }

        // get exposure time (shutter speed)
        iter = Exiv2::exposureTime(exif);
        if (IS_VALID_EXIF_TAG(iter, exif)) {
            Exiv2::Rational exposureTime = iter->value().toRational();
            imageMeta.exposureTime = rational{ exposureTime.first, exposureTime.second };
        }

        // get exposure bias (EV compensation)
        iter = Exiv2::exposureBiasValue(exif);
        if (IS_VALID_EXIF_TAG(iter, exif)) {
            Exiv2::Rational exposureBias = iter->value().toRational();
            imageMeta.exposureBias = rational{ exposureBias.first, exposureBias.second };
        }

        // get aperture (f-number)
        iter = Exiv2::fNumber(exif);
        if (IS_VALID_EXIF_TAG(iter, exif)) {
            imageMeta.aperture = iter->value().toFloat();
        }

        // get ISO speed
        iter = Exiv2::isoSpeed(exif);
        if (IS_VALID_EXIF_TAG(iter, exif)) {
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
        if (IS_VALID_EXIF_TAG(iter, exif)) {
            imageMeta.whiteBalance = iter->print();
        }

        // get flash fired
        iter = Exiv2::flash(exif);
        if (IS_VALID_EXIF_TAG(iter, exif)) {
            uint32_t flashValue = iter->value().toUint32();
            // bit 0 of the flash value indicates whether flash was fired or not
            imageMeta.flashFired = (flashValue & 0x01) != 0;
        }

        // get lens model
        iter = Exiv2::lensName(exif);
        if (IS_VALID_EXIF_TAG(iter, exif)) {
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
        if (IS_VALID_EXIF_TAG(iter, exif)) {
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
            std::string latitudeRef = StringHelpers::trim(iter->value().toString());
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
            std::string longitudeRef = StringHelpers::trim(iter->value().toString());
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

    static void loadImageMeta(Exiv2::Image::UniquePtr& image, Exiv2Parser::imageMetadata& imageMeta) {
        image->readMetadata();

        // get EXIF metadata
        Exiv2::ExifData& exif = image->exifData();
        if (exif.empty()) {
            LOG_WARNING("No Exif metadata found in image");
        }
        loadExifMeta(exif, imageMeta);

        // get XMP metadata
        Exiv2::XmpData& xmp = image->xmpData();
        if (xmp.empty()) {
            LOG_WARNING("No XMP metadata found in image");
        }
        loadXmpMeta(xmp, imageMeta);

        // get thumbnail image metadata (if available)
        Exiv2::PreviewManager mgr(*image);
        auto props = mgr.getPreviewProperties(); // sorted smallest->largest
        if (!props.empty()) {
            auto info = props.back();            // previews sorted from smalleest to largest, get largest available preview
            Exiv2::PreviewImage preview = mgr.getPreviewImage(info);
            imageMeta.thumbWidth = preview.width();
            imageMeta.thumbHeight = preview.height();
            imageMeta.thumbJpegEncoded = (preview.mimeType() == "image/jpeg");
            imageMeta.thumbSizeInBytes = static_cast<int>(preview.size());
            imageMeta.hasThumb = true;
        }
        else {
            imageMeta.hasThumb = false;
        }
    }

    double ConvertRationalToDouble(const rational& number) {
        return (number.Denominator != 0)
            ? (double)number.Numerator / number.Denominator
            : std::numeric_limits<double>::quiet_NaN();
    }

    // expects pointer to TIFF header (start immediately after EXIF header within APP1 segment)
	// expects size of the TIFF data in bytes (from TIFF header to the end of EXIF metadata, including all IFDs and values)
    imageMetadata GetImageMeta(const uint8_t* pTiff, size_t tiffSize) {
        imageMetadata imageMeta;
        try {
            auto io = std::make_unique<Exiv2::MemIo>(pTiff, tiffSize);
            auto image = Exiv2::ImageFactory::open(std::move(io));
            if (!image) {
                LOG_ERROR("Image cration from TIFF failed");
            }

            //Exiv2::ExifData exif;
            //Exiv2::ExifParser::decode(exif, (const Exiv2::byte*)pTiff, static_cast<long>(tiffSize));
            //Exiv2::ExifData& exif = image->exifData();
            //loadExifMeta(exif, imageMeta);

            loadImageMeta(image, imageMeta);
            return imageMeta;
        }
        catch (Exiv2::Error& e) {
            LOG_ERROR(std::string("Exiv2 exception: ") + e.what());
        }

        imageMeta.loaded = true;
        return imageMeta;
    }

    imageMetadata GetImageMeta(LPCWSTR imagePath) {
        imageMetadata imageMeta;

        try {
            std::string imagePathUtf8 = StringHelpers::toUTF8(imagePath);
            Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(imagePath);
            if (!image) {
                LOG_ERROR("Failed to open image: " + imagePathUtf8);
                return imageMeta;
            }

            loadImageMeta(image, imageMeta);
            return imageMeta;
        }
        catch (Exiv2::Error& e) {
            LOG_ERROR(std::string("Exiv2 exception: ") + e.what());
        }
    }
}
