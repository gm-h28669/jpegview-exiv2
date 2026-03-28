#pragma once
#include "Exiv2Parser.h"
#include <cmath>

// Signed rational number: numerator/denominator
class SignedRational {
public:
	SignedRational() {};
	SignedRational(int num, int denom) { Numerator = num; Denominator = denom; }
	int Numerator;
	int Denominator;
};

// Unsigned rational number: numerator/denominator
class Rational {
public:
	Rational() : Rational(0, 1) {}
	
	Rational(int num, int denom) {
		Numerator = num; 
		Denominator = denom; 
	}
	
	Rational(Exiv2Parser::rational num) {
		Numerator = num.Numerator;
		Denominator = num.Denominator;
	}

	int Numerator;
	int Denominator;

	operator double() const {
		return Denominator != 0 ? (double)Numerator / (double)Denominator
			: std::numeric_limits<double>::quiet_NaN();
	}

	bool IsZero() const { return Numerator == 0.0; }
	bool IsEmpty() const { return *this == Empty; }
	bool IsZeroOrEmpty() { return (IsZero() || IsEmpty()); }

	bool operator==(Rational const& r) const {
		return Numerator == r.Numerator && Denominator == r.Denominator;
	}
	bool operator!=(Rational const& r) const { return !(*this == r); }

	static const Rational Empty;
};

inline const Rational Rational::Empty = Rational(0, 0);

class GPSCoordinate {
public:
	GPSCoordinate(double degrees, double minutes, double seconds, CString relativePos) {
		if (minutes == 0.0 && seconds == 0.0) {
			minutes = 60 * abs(degrees - (int)degrees);
			degrees = (int)degrees;
		}
		if (seconds == 0.0) {
			seconds = 60 * abs(minutes - (int)minutes);
			minutes = (int)minutes;
		}
		Degrees = degrees;
		Minutes = minutes;
		Seconds = seconds;
		RelativePos = relativePos;
	}
	
	// New constructor for gpsCoordinate conversion
	GPSCoordinate(const Exiv2Parser::gpsCoordinate& coord) {
		Degrees = coord.degrees;
		Minutes = coord.minutes;
		Seconds = coord.seconds;
		RelativePos = CString(coord.relativePos.c_str());
	}

	double Degrees;
	double Minutes;
	double Seconds;
	CString RelativePos; // north/south of equator for latitude, west/east of Greenwich meridian for longitude
};

// first is a pointer to the 1st entry in IFD
// end is a pointer to the byte following the last IFD entry, i.e. pointer to 4-byte offset to next IFD
struct IFDBounds {
	uint8* first = nullptr;
	uint8* end = nullptr;
	bool isValid() const { return first != nullptr && end != nullptr; }
};

// Reads and parses the EXIF data of JPEG images
class CEXIFReader {
public:
	// The pApp1Block must point to the APP1 block of the EXIF data, including the APP1 block marker
	// The class does not take ownership of the memory (no copy made), thus the APP1 block must not be deleted
	// while the EXIF reader class is deleted.
	CEXIFReader(void* pApp1Block, EImageFormat eImageFormat, LPCTSTR imagePath);

	// get EXIF data for image given by image path using Exiv2 library
	CEXIFReader::CEXIFReader(LPCTSTR imagePath);

	~CEXIFReader(void);


public:
	// the returned pointers are valid while the EXIF reader instance is alive
	bool HasMake() { return !_make.IsEmpty(); }
	LPCTSTR GetMake() { return _make; }

	bool HasModel() { return !_model.IsEmpty(); }
	LPCTSTR GetModel() { return _model; }

	bool HasUserComment() { return !_userComment.IsEmpty(); }
	LPCTSTR GetUserComment() { return _userComment; }

	bool HasDescription() { return !_description.IsEmpty(); }
	LPCTSTR GetDescription() { return _description; }

	bool HasSoftware() { return !_software.IsEmpty(); }
	LPCTSTR GetSoftware() { return _software; }

	// rating
	bool HasRating() { return _rating > 0; }
	int GetRating() { return _rating; }

	// date and time the picture was taken
	bool HasDateTaken() { return _dateTaken.wYear >= 1970; }
	const SYSTEMTIME& GetDateTaken() { return _dateTaken; }

	// date ans time the picture was saved/modified
	bool HasDateModified() { return _dateLastModified.wYear >= 1970; }
	const SYSTEMTIME& GetDateModified() { return _dateLastModified; }

	// exposure time
	bool HasExposureTime() { return !_exposureTime.IsZeroOrEmpty(); }
	const Rational& GetExposureTime() { return _exposureTime; }

	// exposure bias
	bool HasExposureBias() { return _exposureBias != 0.0 && !std::isnan(_exposureBias); }
	double GetExposureBias() { return _exposureBias; }

	// aperture value (F-Number)
	bool HasApertureValue() { return _aperture > 0.0; }
	double GetApertureValue() { return _aperture; }
	
	// white balance mode
	bool HasWhiteBalanceMode() { return !_whiteBalanceMode.IsEmpty(); }
	LPCTSTR GetWhiteBalanceMode() { return _whiteBalanceMode; }
	
	// ISO speed value
	bool HasISOSpeed() { return _isoSpeed > 0; }
	int GetISOSpeed() { return _isoSpeed; }

	// flash fired
	bool GetFlashFired() { return _flashFired; }

	// lens model
	bool HasLensName() { return !_lensName.IsEmpty(); }
	LPCTSTR GetLensName() { return _lensName; }

	// Lens info (zoom range and maximum aperture range)
	bool HasLensInfo();
	CString GetLensInfo();

	// Focal length (mm), the focal lenght is stored as a rational number
	bool HasFocalLength() { return _focalLength > 0.0; }
	double GetFocalLength() { return _focalLength; }

	// Focal length equivalent for standard 36mm x 24mm
	bool HasFocalLengthEquivalent() { return _focalLengthEquivalent > 0.0; }
	double GetFocalLengthEquivalent() { return _focalLengthEquivalent; }

	// Image orientation as detected by sensor, coding according EXIF standard (thus no angle in degrees)
	// valid values are > 0, 0 means that orientation information is not available
	bool HasOrientation() { return _orientation > 0; }
	int GetOrientation() { return _orientation; }
	
	// GPS information
	bool HasGPSLocation() { return _latitude != NULL && _longitude != NULL; }
	GPSCoordinate* GetGPSLatitude() { return _latitude; }
	GPSCoordinate* GetGPSLongitude() { return _longitude; }
	bool HasAltitude() { return _altitude != 0.0; } // 0.0 usually indicates that device did not have altitude available at time of image taking
	double GetGPSAltitude() { return _altitude; }

	// Thumbnail image information
	bool HasThumb() { return _hasThumb;  }
	bool ThumbJpegEncoded() { return _thumbJpegEncoded; }
	int GetThumbSizeInBytes() { return _thumbSizeInBytes; }
	int GetThumbnailWidth() { return _thumbWidth; }
	int GetThumbnailHeight() { return _thumbHeight; }
	
	// Sets the image orientation to given value (if tag was present in input stream).
	// Writes to the APP1 block passed in constructor.
	void WriteImageOrientation(int orientation);
	
	// Updates an existing JPEG compressed thumbnail image by given JPEG stream (SOI stripped)
	// Writes to the APP1 block passed in constructor. Make sure that enough memory is allocated for this APP1
	// block to hold the additional thumbnail data.
	void UpdateJPEGThumbnail(unsigned char* jpegStream, int streamLen, int exifBlockLenCorrection, CSize sizeThumb);

	// Delete the thumbnail image
	// Writes to the APP1 block passed in constructor.
	void DeleteThumbnail();

	// calculate 25mm focal lenght equivalent
	double CalcFocalLengthEquiv(double focalLength);

private:
	void populateFromImageMeta(Exiv2Parser::imageMetadata& imageMeta);
	IFDBounds resolveIFD(uint32 offset, uint8* pTiff, size_t app1Size, const char* errMsg);

	CString _make;
	CString _model;
	CString _userComment;
	CString _description;
	CString _software;
	uint32_t _rating = 0;

	SYSTEMTIME _dateTaken = {};
	SYSTEMTIME _dateLastModified = {};
	
	Rational _exposureTime;
	double _exposureBias = 0.0;
	double _aperture = 0.0;
	uint32_t _isoSpeed = 0;
	bool _flashFired = false;
	CString _whiteBalanceMode;
	uint32_t _orientation = 0;

	CString _lensName;
	Rational m_LensInfo[4]; 
	double _focalLength = 0.0;
	double _focalLengthEquivalent = 0.0;
	
	bool _hasThumb = false;
	bool _thumbJpegEncoded = false;
	uint32_t _thumbWidth = 0;
	uint32_t _thumbHeight = 0;
	uint32_t _thumbSizeInBytes = 0;

	GPSCoordinate* _latitude = NULL;
	GPSCoordinate* _longitude = NULL;
	double _altitude = 0.0;

	bool _littleEndian = false;
	
	EImageFormat _imageFormat = IF_Unknown;
	uint8* _pApp1 = NULL;
	uint8* _pFirstEntryIFD0 = NULL;
	uint8* _pEndIFD0 = NULL;
	uint8* _pFirstEntryIFD1 = NULL;
	uint8* _pEndIFD1 = NULL;
	uint8* _pFirstEntryExifIFD = NULL;
	uint8* _pEndExifIFD = NULL;
};
