#include "StdAfx.h"
#include "EXIFReader.h"
#include "ImageProcessingTypes.h"
#include "Helpers.h"
#include "EXIFHelpers.h"
#include "Exiv2Parser.h"
#include <vector>

// only left tags that are still used to access EXIF direclty and not through Exiv2 library
namespace EXIF {
	// IFD0 Tags (TIFF/EXIF Image IFD)
	constexpr uint16 TAG_ORIENTATION = 0x0112;
	constexpr uint16 TAG_EXIF_IFD = 0x8769;

	// IFD1 Thumbnail Tags (TIFF)
	constexpr uint16 TAG_IMAGE_WIDTH = 0x0100;  // ImageWidth (for thumb)
	constexpr uint16 TAG_IMAGE_HEIGHT = 0x0101;  // ImageHeight (for thumb)
	constexpr uint16 TAG_COMPRESSION = 0x0103;
	constexpr uint16 TAG_JPEG_INTERCHANGE = 0x0201;  // JPEGInterchangeFormat
	constexpr uint16 TAG_JPEG_INTERCHANGE_LEN = 0x0202;

	// Data Type Constants
	constexpr uint16 TYPE_SHORT = 0x0003;
	constexpr uint16 TYPE_LONG = 0x0004;

	// TIFF Header Byte Order (bytes 0-1)
	constexpr uint16 TIFF_LITTLE_ENDIAN = 0x4949;
	constexpr uint16 TIFF_BIG_ENDIAN = 0x4D4D;

	// TIFF Magic Number (bytes 2-3) - always 42
	constexpr uint16 TIFF_VERSION = 0x002A;
}

static uint32 ReadUInt(void* ptr, bool littleEndian) {
	uint32 nValue = *((uint32*)ptr);
	if (!littleEndian) {
		return _byteswap_ulong(nValue);
	} else {
		return nValue;
	}
}

static void WriteUInt(void* ptr, uint32 value, bool littleEndian) {
	uint32 val = value;
	if (!littleEndian) {
		val = _byteswap_ulong(val);
	}
	*((uint32*)ptr) = val;
}

static uint16 ReadUShort(void* ptr, bool littleEndian) {
	uint16 value = *((uint16*)ptr);
	if (!littleEndian) {
		return _byteswap_ushort(value);
	} else {
		return value;
	}
}

static void WriteUShort(void* ptr, uint16 value, bool littleEndian) {
	uint16 val = value;
	if (!littleEndian) {
		val = _byteswap_ushort(val);
	}
	*((uint16*)ptr) = val;
}

static uint8* FindTag(uint8* ptr, uint8* ptrLast, uint16 tag, bool littleEndian) {
	while (ptr < ptrLast) {
		if (ReadUShort(ptr, littleEndian) == tag) {
			return ptr;
		}
		ptr += 12;
	}
	return NULL;
}

static uint32 ReadLongTag(uint8* ptr, bool littleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, littleEndian) == EXIF::TYPE_LONG) {
		return ReadUInt(ptr + 8, littleEndian);
	} else {
		return 0;
	}
}

static uint16 ReadShortTag(uint8* ptr, bool littleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, littleEndian) == EXIF::TYPE_SHORT) {
		return ReadUShort(ptr + 8, littleEndian);
	} else {
		return 0;
	}
}

static uint32 ReadShortOrLongTag(uint8* ptr, bool littleEndian) {
	if (ptr != NULL) {
		uint16 nType = ReadUShort(ptr + 2, littleEndian);
		if (nType == EXIF::TYPE_SHORT) {
			return ReadUShort(ptr + 8, littleEndian);
		} else if (nType == EXIF::TYPE_LONG) {
			return ReadUInt(ptr + 8, littleEndian);
		}
	}
	return 0;
}

static void WriteShortTag(uint8* ptr, uint16 value, bool littleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, littleEndian) == EXIF::TYPE_SHORT) {
		WriteUShort(ptr + 8, value, littleEndian);
	}
}

static void WriteLongTag(uint8* ptr, uint32 value, bool littleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, littleEndian) == EXIF::TYPE_LONG) {
		WriteUInt(ptr + 8, value, littleEndian);
	}
}

bool CEXIFReader::ParseExifDateTimeToSysTime(const CString& exifDateTime, SYSTEMTIME& date) {
	int year, month, day, hour, minute, second;
	if (_stscanf(exifDateTime, _T("%d:%d:%d %d:%d:%d"), &year, &month, &day, &hour, &minute, &second) == 6) {
		date.wYear = year;
		date.wMonth = month;
		date.wDay = day;
		date.wHour = hour;
		date.wMinute = minute;
		date.wSecond = second;
		date.wMilliseconds = 0;
		date.wDayOfWeek = 0;
		return true;
	}
	return false;
}

CString FormatDate(const std::tm& tm, const char* format) {
	char buffer[64];
	strftime(buffer, sizeof(buffer), format, &tm);
	return CString(buffer);
}

void tmToSystemTime(const std::tm& tm, SYSTEMTIME& st) {
	st.wYear = tm.tm_year + 1900;
	st.wMonth = tm.tm_mon + 1;
	st.wDayOfWeek = 0;           // Calculate if needed: mktime(&ptm)->tm_wday
	st.wDay = tm.tm_mday;
	st.wHour = tm.tm_hour;
	st.wMinute = tm.tm_min;
	st.wSecond = tm.tm_sec;
	st.wMilliseconds = 0;        // tm doesn't store ms
}

// case-insensitive full match: if str fully matches any of the targets (ignoring case), return replacement, otherwise return str
CString getReplacementOnExactMatch(const CString& str, const CString& replacement, const std::vector<CString>& targets) {
	for (const auto& target : targets) {
		if (_tcsicmp(str, target) == 0) {
			return replacement;
		}
	}
	return str;
}

// case-insensitive partial match: if str contains target (ignoring case), return target, otherwise return str
CString getTargetIfContained(const CString& str, const CString target) {
	if (EXIFHelpers::ContainsCaseInsensitive(str, target)) {
		return target;
	}
	return str;
}

CEXIFReader::CEXIFReader(void* pApp1Block, EImageFormat eImageFormat)
	: _exposureTime { 0, 0 },
	_dateTaken{ 0 },
	_dateLastModified{ 0 }
{
	_thumbWidth = -1;
	_thumbHeight = -1;

	// check if APP1 marker valid
	_pApp1 = (uint8*)pApp1Block;
	if (_pApp1[0] != 0xFF || _pApp1[1] != 0xE1) {
		return;
	}
	int app1Size = _pApp1[2]*256 + _pApp1[3] + 2;

	// read TIFF header
	uint8* pTIFFHeader = _pApp1 + 10;
	bool littleEndian;
	if (*(short*)pTIFFHeader == EXIF::TIFF_LITTLE_ENDIAN) {
		littleEndian = true;
	} else if (*(short*)pTIFFHeader == EXIF::TIFF_BIG_ENDIAN) {
		littleEndian = false;
	} else {
		return;
	}
	_littleEndian = littleEndian;

	// ensure IFD0 segment lays within the APP1 block
	uint8* pIFD0 = pTIFFHeader + ReadUInt(pTIFFHeader + 4, littleEndian);
	if (pIFD0 - _pApp1 >= app1Size) {
		return;
	}

	// get IFD0 segment info: pointer to the first IFD0 tag and pointer to the end of the IFD0 segment
	// this info is required for searching tags in IFD0
	uint16 numTags = ReadUShort(pIFD0, littleEndian);
	pIFD0 += 2;
	uint8* pLastIFD0 = pIFD0 + numTags*12;
	if (pLastIFD0 - _pApp1 + 4 >= app1Size) {
		return;
	}
	_pLastIFD0 = pLastIFD0;

	// get offset to IFD1 segment (thumbnail image)
	uint32 offsetIFD1 = ReadUInt(pLastIFD0, littleEndian);

	// get EXIF segment info: pointer to the first EXIF tag and pointer to the end of the EXIF segment
	uint8* pTagEXIFIFD = FindTag(pIFD0, pLastIFD0, EXIF::TAG_EXIF_IFD, littleEndian);
	bool hasExifSegment = (pTagEXIFIFD != NULL);

	uint8* pEXIFIFD = NULL;
	uint8* pLastEXIF = NULL;

	if (hasExifSegment) {
		uint32 offsetEXIF = ReadLongTag(pTagEXIFIFD, littleEndian);
		if (offsetEXIF == 0) {
			return;
		}
		pEXIFIFD = pTIFFHeader + offsetEXIF;
		if (pEXIFIFD - _pApp1 >= app1Size) {
			return;
		}
		numTags = ReadUShort(pEXIFIFD, littleEndian);
		pEXIFIFD += 2;
		pLastEXIF = pEXIFIFD + numTags * 12;
		if (pLastEXIF - _pApp1 >= app1Size) {
			return;
		}
	}

	// orientation tags must be ignored for JXL and HEIF/AVIF
	if (eImageFormat != IF_JXL && eImageFormat != IF_HEIF && eImageFormat != IF_AVIF) {
		// hack: old code keeps a pointer to the orientation entry in EXIF
		// it needs it when saving the image (to preserve the original orientation)
		_pTagOrientation = FindTag(pIFD0, pLastIFD0, EXIF::TAG_ORIENTATION, littleEndian);
	}
	
	// new implementation gets EXIF metadata using Exiv2 library
	// Exiv2 library returns more accurate values for some tags (e.g. focal length, lens name, etc)
	// with new approach zhe code will also become more simple to maintain in future
	Exiv2Parser::imageMetadata imageMeta = Exiv2Parser::getExifMetadata(_pApp1);

	// tranfer data from DTO to member variables of old class, so that the old code can use it without change

	// get camera make
	_make = imageMeta.make.c_str();
	// return only the well known brand, for example "Olympus Imaging Corporation" is simplified to "Olympus"
	_make =getTargetIfContained(_make, _T("Olympus"));

	// get camera model
	_model = imageMeta.model.c_str();
	_model = EXIFHelpers::RemoveMakeFromModel(_make, _model);

	// get user comment and remove useless user comments added by some cameras
	_userComment = imageMeta.userComment.c_str();
	_userComment = getReplacementOnExactMatch(_userComment, _T(""), { _T("User comments") });

	// get image description
	_description = imageMeta.description.c_str();
	// remove useless image descriptions added by some cameras
	_description = getReplacementOnExactMatch(_description, _T(""), { _T("OLYMPUS DIGITAL CAMERA"), _T("raw") });

	// get software used to create the image
	_software = imageMeta.software.c_str();

	// get date taken and last modified date
	tmToSystemTime(imageMeta.dateTaken, _dateTaken);
	tmToSystemTime(imageMeta.dateModified, _dateLastModified);

	// get basic exposure info
	_exposureTime = Rational(imageMeta.exposureTime);
	_exposureBias = Exiv2Parser::convertRationalToDouble(imageMeta.exposureBias);
	_aperture = imageMeta.aperture;
	_isoSpeed = imageMeta.isoSpeed;
	_flashFired = imageMeta.flashFired;
	m_WhiteBalanceMode = imageMeta.whiteBalance;
	_imageOrientation = (uint8)imageMeta.orientation;

	// get lens related info
	_focalLength = imageMeta.focalLength;
	_focalLengthEquivalent = imageMeta.focalLengthEquivalent;
	
	_lensName = imageMeta.lensName.c_str();
	// if lens was not found, Exiv2 returns "N/A", but in this case we want to have empty string in _lensName variable
	_lensName = getReplacementOnExactMatch(_lensName, _T(""), { _T("N/A") });

	for (int i = 0; i < 4; i++) {
		m_LensInfo[i] = Rational(imageMeta.lensInfo[i]);
	}

	// get GPS related info
	if (imageMeta.gps.positionAvailable) {
		_latitude = new GPSCoordinate(imageMeta.gps.latitude);
		_longitude = new GPSCoordinate(imageMeta.gps.longitude);
		_altitude = imageMeta.gps.altitude;
	}

	// get IFD1 segment info: pointer to the first IFD1 tag and pointer to the end of the IFD1 segment
	// this segment contains thumbnail image info and thumbnail itself (if JPEG compressed thumbnail is present)
	if (offsetIFD1 != 0) {
		_pIFD1 = pTIFFHeader + offsetIFD1;
		if (_pIFD1 - _pApp1 >= app1Size || _pIFD1 - _pApp1 < 0) {
			return;
		}
		numTags = ReadUShort(_pIFD1, littleEndian);
		_pIFD1 += 2;
		uint8* pLastIFD1 = _pIFD1 + numTags*12;
		if (pLastIFD1 - _pApp1 >= app1Size) {
			return;
		}
		_plastIFD1 = pLastIFD1;

		// check if thumbnail is JPEG compressed or not
		uint8* pTagCompression = FindTag(_pIFD1, pLastIFD1, EXIF::TAG_COMPRESSION, littleEndian);
		if (pTagCompression == NULL) {
			return;
		}

		// according to EXIF spec, if the value of Compression tag is 6, then the thumbnail is JPEG compressed, 
		// otherwise it is uncompressed (TIFF)
		if (ReadShortTag(pTagCompression, littleEndian) == 6) {
			// compressed
			uint8* pTagOffsetSOI = FindTag(_pIFD1, pLastIFD1, EXIF::TAG_JPEG_INTERCHANGE, littleEndian);
			uint8* pTagJPEGLen = FindTag(_pIFD1, pLastIFD1, EXIF::TAG_JPEG_INTERCHANGE_LEN, littleEndian);
			if (pTagOffsetSOI != NULL && pTagJPEGLen != NULL) {
				uint32 offsetSOI = ReadLongTag(pTagOffsetSOI, littleEndian);
				uint32 jpegBytes = ReadLongTag(pTagJPEGLen, littleEndian);
				uint8* pSOI = pTIFFHeader + offsetSOI;
				uint8* pSOF = (uint8*) Helpers::FindJPEGMarker(pSOI, jpegBytes, 0xC0);
				if (pSOF != NULL) {
					_thumbWidth = pSOF[7]*256 + pSOF[8];
					_thumbHeight = pSOF[5]*256 + pSOF[6];
					_jpegThumbStreamLength = jpegBytes;
					_hasJpegCompressedThumbnail = true;
				}
			}
		} else {
			// uncompressed
			uint8* pTagThumbWidth = FindTag(_pIFD1, pLastIFD1, EXIF::TAG_IMAGE_WIDTH, littleEndian);
			uint8* pTagThumbHeight = FindTag(_pIFD1, pLastIFD1, EXIF::TAG_IMAGE_HEIGHT, littleEndian);
			if (pTagThumbWidth != NULL && pTagThumbHeight != NULL) {
				_thumbWidth = ReadShortOrLongTag(pTagThumbWidth, littleEndian);
				_thumbHeight = ReadShortOrLongTag(pTagThumbHeight, littleEndian);
			}
		}
	}
}

CEXIFReader::~CEXIFReader(void) {
	delete _latitude;
	delete _longitude;
}

void CEXIFReader::WriteImageOrientation(int orientation) {
	if (_pTagOrientation != NULL) {
		WriteShortTag(_pTagOrientation, orientation, _littleEndian);
	}
}

void CEXIFReader::DeleteThumbnail() {
	if (_pLastIFD0 != NULL) {
		*_pLastIFD0 = 0;
	}
}

void CEXIFReader::UpdateJPEGThumbnail(unsigned char* pJPEGStream, int streamLen, int exifBlockLenCorrection, CSize sizeThumb) {
	if (!_hasJpegCompressedThumbnail) {
		return;
	}
	uint8* pTagOffsetSOI = FindTag(_pIFD1, _plastIFD1, EXIF::TAG_JPEG_INTERCHANGE, _littleEndian);
	uint32 nOffsetSOI = ReadLongTag(pTagOffsetSOI, _littleEndian);
	uint8* pTIFFHeader = _pApp1 + 10;
	uint8* pSOI = pTIFFHeader + nOffsetSOI;
	memcpy(pSOI + 2, pJPEGStream, streamLen);

	uint8* pTagJPEGBytes = FindTag(_pIFD1, _plastIFD1, EXIF::TAG_JPEG_INTERCHANGE_LEN, _littleEndian);
	WriteLongTag(pTagJPEGBytes, streamLen + 2, _littleEndian);
	int newApp1Size = _pApp1[2]*256 + _pApp1[3] + exifBlockLenCorrection;
	_pApp1[2] = newApp1Size >> 8;
	_pApp1[3] = newApp1Size & 0xFF;
}

double CEXIFReader::CalcFocalLengthEquiv(double focalLength) {
	return EXIFHelpers::CalcFocalLengthEquivalent(_make, _model, focalLength);
}

bool CEXIFReader::HasLensInfo() {
	bool isEmpty = true;
	for (int i = 0; i < 4; i++) {
		if (!m_LensInfo[i].IsEmpty()) {
			return true;
		}
	}
	return false;
}

// Print conversion for lens info
// array [0..3] of rational values: (min focal, max focal, min F, max F)
// returns: string in the form "12-20mm f/3.8-4.5" or "50mm f/1.4"
CString CEXIFReader::GetLensInfo() {
	if (!HasLensInfo()) {
		return _T("");
	}

	CString focalRange;
	double minFocal = m_LensInfo[0];
	double maxFocal = m_LensInfo[1];
	if (abs(maxFocal - minFocal) < 1E-3) {
		focalRange.Format(_T("%.0fmm"), minFocal);
	}
	else {
		focalRange.Format(_T("%.0f-%.0fmm"), minFocal, maxFocal);
	}

	CString fNumberRange;
	if (m_LensInfo[2].IsEmpty() && m_LensInfo[3].IsEmpty()) {
		fNumberRange = _T("");
	}
	else {
		double minFNumber = m_LensInfo[2];
		double maxFNumber = m_LensInfo[3];
		if (minFNumber == 0 && maxFNumber == 0) {
			fNumberRange = _T("");
		}
		else if (abs(maxFNumber - minFNumber) < 1E-3) {
			fNumberRange.Format(_T(" f/%.1f"), minFNumber);
		}
		else {
			fNumberRange.Format(_T(" f/%.1f-%.1f"), minFNumber, maxFNumber);
		}
	}
	return focalRange + fNumberRange;
}
