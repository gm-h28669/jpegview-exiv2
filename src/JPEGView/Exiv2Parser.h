#pragma once
#include <string>    
#include <cstdint> 
#include <ctime>

namespace Exiv2Parser {
    typedef struct rational {
        int32_t Numerator = 0;
        int32_t Denominator = 0;
    } rational;

    typedef struct gpsCoordinate {
        double degrees = 0.0;
        double minutes = 0.0;
        double seconds = 0.0;
		std::string relativePos; // north/south of equator for latitude, west/east of Greenwich meridian for longitude

        gpsCoordinate() = default;
    } gpsCoordinate;

    typedef struct gpsMetadata {
        bool positionAvailable = false;
        gpsCoordinate latitude;
		gpsCoordinate longitude;    
        double altitude = 0.0;

        gpsMetadata() = default;
    } gpsMetadata;

    typedef struct imageMetadata {
        bool loaded = false;

        std::string make;
        std::string model;
        std::string userComment;
        std::string description;
        std::string software;
        double cropFactor = 0.0;

        std::tm dateTaken = {};
        std::tm dateModified = {};

        uint32_t width = 0;
        uint32_t height = 0;
        double aspectRatio = 0.0;
		uint32_t orientation = 0;  // valid values > 0, we use 0 to indicate that orientation tag was not found in the EXIF metadata

        rational exposureTime;
        rational exposureBias;
        uint32_t exposureMode = 0;
        double aperture = 0.0;
        uint32_t isoSpeed = 0;
        uint32_t whiteBalance = 0;
        bool flashFired = false;

        std::string lensName;
        rational lensInfo[4];
        double focalLength = 0.0;
        double focalLengthEquivalent = 0.0;

        bool jpegThumbnailPresent = false;
        uint32_t thumbnailWidth = 0;
        uint32_t thumbnailHeight = 0;
        int jpegThumbnailStreamLenght = 0;

        gpsMetadata gps;

        imageMetadata() = default;
    } imageMetadata;

    imageMetadata getExifMetadata(const uint8_t* pApp1Block);
    double convertRationalToDouble(const rational& number);
}
