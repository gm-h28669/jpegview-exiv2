#pragma once

// Signed rational number: numerator/denominator
class SignedRational {
public:
	SignedRational(int num, int denom) { Numerator = num; Denominator = denom; }
	int Numerator;
	int Denominator;
};

// Unsigned rational number: numerator/denominator
class Rational {
public:
	Rational() { Numerator = 0; Denominator = 0; }
	Rational(unsigned int num, unsigned int denom) { Numerator = num; Denominator = denom; }
	unsigned int Numerator;
	unsigned int Denominator;
	bool IsEmpty() const {
		return Numerator == 0 && Denominator == 0;
	}
	operator double() const {
		return Denominator != 0 ? (double)Numerator / (double)Denominator
			: std::numeric_limits<double>::quiet_NaN();
	}
};

class GPSCoordinate {
public:
	GPSCoordinate(LPCTSTR reference, double degrees, double minutes, double seconds) {
		m_sReference = CString(reference);
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
	}
	LPCTSTR GetReference() { return m_sReference; }
	double Degrees;
	double Minutes;
	double Seconds;
private:
	CString m_sReference;
};

// Reads and parses the EXIF data of JPEG images
class CEXIFReader {
public:
	// The pApp1Block must point to the APP1 block of the EXIF data, including the APP1 block marker
	// The class does not take ownership of the memory (no copy made), thus the APP1 block must not be deleted
	// while the EXIF reader class is deleted.
	CEXIFReader(void* pApp1Block, EImageFormat eImageFormat);
	~CEXIFReader(void);

	// Parse date string in the EXIF date/time format
	static bool ParseDateString(SYSTEMTIME & date, const CString& str);

public:
	// Camera model, image comment and description. The returned pointers are valid while the EXIF reader is not deleted.
	LPCTSTR GetCameraModel() { return m_sModel; }
	LPCTSTR GetUserComment() { return m_sUserComment; }
	LPCTSTR GetImageDescription() { return m_sImageDescription; }
	LPCTSTR GetSoftware() { return m_sSoftware; }
	bool GetCameraModelPresent() { return !m_sModel.IsEmpty(); }
	bool GetSoftwarePresent() { return !m_sSoftware.IsEmpty(); }
	// Date-time the picture was taken
	const SYSTEMTIME& GetAcquisitionTime() { return m_acqDate; }
	bool GetAcquisitionTimePresent() { return m_acqDate.wYear > 1600; }
	// Date-time the picture was saved/modified (used by editing software)
	const SYSTEMTIME& GetDateTime() { return m_dateTime; }
	bool GetDateTimePresent() { return m_dateTime.wYear > 1600; }
	// Exposure time
	const Rational& GetExposureTime() { return m_exposureTime; }
	bool GetExposureTimePresent() { return m_exposureTime.Denominator != 0; }
	// Exposure bias
	double GetExposureBias() { return m_dExposureBias; }
	bool GetExposureBiasPresent() { return m_dExposureBias != UNKNOWN_DOUBLE_VALUE; }
	// Flag if flash fired
	bool GetFlashFired() { return m_bFlashFired; }
	bool GetFlashFiredPresent() { return m_bFlashFlagPresent; }
	// Focal length (mm), the focal lenght is stored as a rational number
	double GetFocalLength() { return (double)m_focalLength.Numerator / (double)m_focalLength.Denominator; }
	bool GetFocalLengthPresent() { return m_focalLength.Denominator != 0; }
	// F-Number
	double GetFNumber() { return m_dFNumber; }
	bool GetFNumberPresent() { return m_dFNumber != UNKNOWN_DOUBLE_VALUE; }
	// ISO speed value
	int GetISOSpeed() { return m_nISOSpeed; }
	bool GetISOSpeedPresent() { return m_nISOSpeed > 0; }
	// Focal length equivalent for standard 36mm x 24mm
	int GetFocalLengthEquivalent() { return m_focalLengthEquivalent; }
	bool GetFocalLengthEquivalentPresent() { return m_focalLengthEquivalent != 0; }
	// Camera make
	LPCTSTR GetCameraMake() { return m_sMake; }
	bool GetCameraMakePresent() { return !m_sMake.IsEmpty(); }
	// Lens info (zoom range and maximum aperture range)
	CString GetLensInfo();
	// Lens model
	LPCTSTR GetLensModel() { return m_sLensModel; }
	bool GetLensModelPresent() { return !m_sLensModel.IsEmpty(); }
	// Image orientation as detected by sensor, coding according EXIF standard (thus no angle in degrees)
	int GetImageOrientation() { return m_nImageOrientation; }
	bool ImageOrientationPresent() { return m_nImageOrientation > 0; }
	// Thumbnail image information
	bool HasJPEGCompressedThumbnail() { return m_bHasJPEGCompressedThumbnail; }
	int GetJPEGThumbStreamLen() { return m_nJPEGThumbStreamLen; }
	int GetThumbnailWidth() { return m_nThumbWidth; }
	int GetThumbnailHeight() { return m_nThumbHeight; }
	// GPS information
	bool IsGPSInformationPresent() { return m_pLatitude != NULL && m_pLongitude != NULL; }
	bool IsGPSAltitudePresent() { return m_dAltitude != UNKNOWN_DOUBLE_VALUE; }
	GPSCoordinate* GetGPSLatitude() { return m_pLatitude; }
	GPSCoordinate* GetGPSLongitude() { return m_pLongitude; }
	double GetGPSAltitude() { return m_dAltitude; }

	// Sets the image orientation to given value (if tag was present in input stream).
	// Writes to the APP1 block passed in constructor.
	void WriteImageOrientation(int nOrientation);
	
	// Updates an existing JPEG compressed thumbnail image by given JPEG stream (SOI stripped)
	// Writes to the APP1 block passed in constructor. Make sure that enough memory is allocated for this APP1
	// block to hold the additional thumbnail data.
	void UpdateJPEGThumbnail(unsigned char* pJPEGStream, int nStreamLen, int nEXIFBlockLenCorrection, CSize sizeThumb);

	// Delete the thumbnail image
	// Writes to the APP1 block passed in constructor.
	void DeleteThumbnail();

	// calculate 25mm focal lenght equivalent
	double CalcFocalLengthEquiv(double focalLength);
public:
	// unknown double value
	static double UNKNOWN_DOUBLE_VALUE;

private:
	CString m_sModel;
	CString m_sUserComment;
	CString m_sImageDescription;
	CString m_sSoftware;
	SYSTEMTIME m_acqDate;
	SYSTEMTIME m_dateTime;
	Rational m_exposureTime;
	double m_dExposureBias;
	bool m_bFlashFired;
	bool m_bFlashFlagPresent;
	double m_dFocalLength;
	double m_dFNumber;
	int m_nISOSpeed;
	int m_nImageOrientation;
	bool m_bHasJPEGCompressedThumbnail;
	int m_nThumbWidth;
	int m_nThumbHeight;
	int m_nJPEGThumbStreamLen;
	GPSCoordinate* m_pLatitude;
	GPSCoordinate* m_pLongitude;
	double m_dAltitude;
	CString m_sMake;
	CString m_sLensModel;
	Rational m_focalLength;
	Rational m_lensInfo[4];
	int m_focalLengthEquivalent;

	bool m_bLittleEndian;
	uint8* m_pApp1;
	uint8* m_pTagOrientation;
	uint8* m_pLastIFD0;
	uint8* m_pIFD1;
	uint8* m_pLastIFD1;

	void ReadGPSData(uint8* pTIFFHeader, uint8* pTagGPSIFD, int nApp1Size, bool bLittleEndian);
	GPSCoordinate* ReadGPSCoordinate(uint8* pTIFFHeader, uint8* pTagLatOrLong, LPCTSTR reference, bool bLittleEndian);
};
