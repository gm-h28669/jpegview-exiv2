#include "StdAfx.h"
#include "EXIFReader.h"
#include "ImageProcessingTypes.h"
#include "Helpers.h"
#include "EXIFHelpers.h"
#include "Exiv2Parser.h"
#include <vector>
#include <cassert>
#include "Logging.h"
#include <tuple>

constexpr uint16_t APP1_MARKER = 0xFFE1;
constexpr unsigned char EXIF_HEADER[6] = { 'E','x','i','f', 0, 0 };
constexpr unsigned char TIFF_HEADER_LITTLE_ENDIAN[4] = { 'I', 'I', 0x2A, 0x00 };
constexpr unsigned char TIFF_HEADER_BIG_ENDIAN[4] = { 'M', 'M', 0x00, 0x2A };
constexpr size_t TIFF_HEADER_LEN = 8;		// TIFF header consists of: 2 (byte order) + 2 (magic) + 4 (offset to 0th IFD) = 8 bytes
constexpr size_t IFD_ENTRY_LEN = 12;		// each IFD entry is 12 bytes long

enum ByteOrder {
	LittleEndian = 0,
	BigEndian = 1,
	Invalid = 2
};

// only left tags that are still used to access EXIF direclty and not through Exiv2 library
namespace EXIF {
	// IFD0 Tags (TIFF/EXIF Image IFD)
	constexpr uint16 TAG_ORIENTATION = 0x0112;
	constexpr uint16 TAG_EXIF_SUBIFD = 0x8769;

	// IFD1 Thumbnail Tags (TIFF)
	constexpr uint16 TAG_JPEG_INTERCHANGE = 0x0201;  // JPEGInterchangeFormat
	constexpr uint16 TAG_JPEG_INTERCHANGE_LEN = 0x0202;

	// Data Type Constants
	constexpr uint16 TYPE_SHORT = 0x0003;
	constexpr uint16 TYPE_LONG = 0x0004;

	// TIFF Header Byte Order (bytes 0-1)
	constexpr uint16 TIFF_LITTLE_ENDIAN = 0x4949;
	constexpr uint16 TIFF_BIG_ENDIAN = 0x4D4D;
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

static uint8* FindTag(uint8* ptr, uint8* ptrEnd, uint16 tag, bool littleEndian) {
	while (ptr < ptrEnd) {
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

CString FormatDate(const std::tm& tm, const char* format) {
	char buffer[64];
	strftime(buffer, sizeof(buffer), format, &tm);
	return CString(buffer);
}

static void tmToSystemTime(const std::tm& tm, SYSTEMTIME& st) {
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
static CString getReplacementOnExactMatch(const CString& str, const CString& replacement, const std::vector<CString>& targets) {
	for (const auto& target : targets) {
		if (_tcsicmp(str, target) == 0) {
			return replacement;
		}
	}
	return str;
}

// case-insensitive partial match: if str contains target (ignoring case), return target, otherwise return str
static CString getTargetIfContained(const CString& str, const CString target) {
	if (EXIFHelpers::ContainsCaseInsensitive(str, target)) {
		return target;
	}
	return str;
}

static bool isValidApp1Marker(const uint8_t* pApp1) {
	if (!pApp1) return false;
	uint16_t marker = (pApp1[0] << 8) | pApp1[1];
	return marker == APP1_MARKER;
}

static bool isValidExifHeader(const uint8_t* pExif, size_t exifSize) {
	if (!pExif || exifSize < sizeof(EXIF_HEADER)) return false;
	return std::memcmp(pExif, EXIF_HEADER, sizeof(EXIF_HEADER)) == 0;
}

static bool isValidTiffHeader(const uint8_t* pTiff, size_t tiffSize) {
	if (pTiff == NULL || tiffSize < TIFF_HEADER_LEN) return false;

	// "II" 0x2A or "MM" 0x2A
	if (std::memcmp(pTiff, TIFF_HEADER_LITTLE_ENDIAN, sizeof(TIFF_HEADER_LITTLE_ENDIAN)) == 0 ||
		std::memcmp(pTiff, TIFF_HEADER_BIG_ENDIAN, sizeof(TIFF_HEADER_BIG_ENDIAN)) == 0) {
		return true;
	}
	return false;
}

static ByteOrder getByteOrder(const uint8_t* pTiff, size_t tiffSize) {
	uint16_t byteOrder = (pTiff[0] << 8) | pTiff[1];
	if (byteOrder == EXIF::TIFF_LITTLE_ENDIAN) {
		return ByteOrder::LittleEndian;
	} else if (byteOrder == EXIF::TIFF_BIG_ENDIAN) {
		return ByteOrder::BigEndian;
	} else {
		return ByteOrder::Invalid;
	}
}

static bool isWithinApp1(const uint8* ptr, const uint8* pApp1, size_t app1Size) {
	// APP1 size does not include the 2 bytes of APP1 marker, so add them to the end boundary check
	return (ptr >= pApp1) && (ptr < pApp1 + app1Size + 2);
}

// validates an IFD segment and returns the pointer to the first IFD entry and to the end of the IFD entries
// returns NULL if the IFD is invalid or out of bounds.
static std::pair<uint8*, uint8*> getBoundsIFDEntries(uint8* pIFD, uint8* pApp1, uint32 app1Size, bool littleEndian) {
	// validate IFD segment start is within APP1 boundaries
	if (!pIFD || !isWithinApp1(pIFD, pApp1, app1Size)) {
		return std::make_pair((uint8*) NULL, (uint8*) NULL);
	}

	// read number of entries (2 bytes)
	uint16 numTags = ReadUShort(pIFD, littleEndian);

	// calculate pointer to 1st IFD entry (skipping the 2-byte count)
	uint8* pFirstEntry = pIFD + 2;

	// calculate pointer to byte following last IFD entry (pointer will point to 4-byte offset to next IFD segment
	uint8* pEndIFD = pFirstEntry + (numTags * IFD_ENTRY_LEN);

	// validate IFD segment end is within APP1 boundaries
	if (!isWithinApp1(pEndIFD + 4, pApp1, app1Size)) {
		return std::make_pair((uint8*)NULL, (uint8*)NULL);
	}

	return std::make_pair(pFirstEntry, pEndIFD);
}

IFDBounds CEXIFReader::resolveIFD(uint32 offset, uint8* pTiff, size_t app1Size, const char* errMsg) {
	// if offset is 0, the IFD simply doesn't exist (not an error for optional IFDs)
	if (offset == 0) {
		return { NULL, NULL };
	}

	// calculate and validate the pointer to the IFD start
	uint8* pIFD = pTiff + offset;
	if (!isWithinApp1(pIFD, _pApp1, app1Size)) {
		LOG_ERROR(errMsg);
		return { NULL, NULL };
	}

	// get the boundaries (start and end of IFD entries)
	auto bounds = getBoundsIFDEntries(pIFD, _pApp1, app1Size, _littleEndian);

	// validate the bounds
	if (bounds.first == NULL || bounds.second == NULL) {
		LOG_ERROR(errMsg);
		return { NULL, NULL };
	}

	// return the IFD segment pointers
	return { (uint8*)bounds.first, (uint8*)bounds.second };
}

CEXIFReader::CEXIFReader(void* pApp1, EImageFormat eImageFormat, LPCTSTR imagePath)
	: _exposureTime { 0, 0 },
	_dateTaken{ 0 },
	_dateLastModified{ 0 }
{
	assert(pApp1 != NULL);
	if (imagePath != NULL) assert(_tcslen(imagePath) > 0);

	_pApp1 = (uint8*)pApp1;
	_imageFormat = eImageFormat;

	// APP1 segment must start with correct marker
	if (!isValidApp1Marker(_pApp1)) {
		LOG_ERROR("APP1 segment invalid");
		return;
	}

	// get length of APP1 segment after marker, the length includes also the two length bytes
	size_t app1Size = (_pApp1[2] << 8) | _pApp1[3];			// length encoded in big endian order

	// EXIF segment must start with the correct header
	uint8* pExif = _pApp1 + 4;				// EXIF segment starts after marker and length = offset 4
	size_t exifSize = app1Size - 2;			// subtract 2, since APP1 segment length includes the two length bytes
	if (!isValidExifHeader(pExif, exifSize)) {
		LOG_ERROR("EXIF header invalid");
		return;
	}

	// data in EXIF segment is stored in TIFF format, TIFF header starts immediately after the EXIF header
	uint8* pTiff = pExif + sizeof(EXIF_HEADER);
	size_t tiffSize = exifSize - sizeof(EXIF_HEADER);

	// TIFF segment must start with correct header (different depending on byte order)
	if (!isValidTiffHeader(pTiff, tiffSize)) {
		LOG_ERROR("TIFF header invalid");
		return;
	}

	// get byte order (endianness) from TIFF header
	ByteOrder byteOrder = getByteOrder(pTiff, tiffSize);
	_littleEndian = (byteOrder == ByteOrder::LittleEndian);

	// resolve IFD0 (Mandatory)
	uint32 offsetIFD0 = ReadUInt(pTiff + 4, _littleEndian);
	auto ifd0 = resolveIFD(offsetIFD0, pTiff, app1Size, "IFD0 segment invalid");
	if (!ifd0.isValid()) return;
	_pFirstEntryIFD0 = ifd0.first;
	_pEndIFD0 = ifd0.end;

	// resolve IFD1 (optional - thumbnail)
	uint32 offsetIFD1 = ReadUInt(_pEndIFD0, _littleEndian);
	auto ifd1 = resolveIFD(offsetIFD1, pTiff, app1Size, "IFD1 segment invalid");
	_pFirstEntryIFD1 = ifd1.first;
	_pEndIFD1 = ifd1.end;

	// resolve EXIF SubIFD (optional)
	uint8* pTagExifSubIFD = FindTag(_pFirstEntryIFD0, _pEndIFD0, EXIF::TAG_EXIF_SUBIFD, _littleEndian);
	if ((pTagExifSubIFD != NULL)) {
		uint32 offsetExif = ReadLongTag(pTagExifSubIFD, _littleEndian);
		auto exifSub = resolveIFD(offsetExif, pTiff, app1Size, "EXIF IFD segment invalid");
		_pFirstEntryExifIFD = exifSub.first;
		_pEndExifIFD = exifSub.end;
	}

	// get EXIF and other image metadata using via Exiv2 library
	Exiv2Parser::imageMetadata imageMeta;
	if (pTiff != NULL && tiffSize > 0) {
		// use EXIF segment that is already in memory
		imageMeta = Exiv2Parser::GetImageMeta(pTiff, tiffSize);
	} 
	else {
		// fallback: use image path, this will open image in Exiv2, but is lightweight operation
		imageMeta = Exiv2Parser::GetImageMeta(imagePath);
	}

	// tranfer data from DTO to member variables of old class, so that the old code can use it without change
	populateFromImageMeta(imageMeta);
}

CEXIFReader::CEXIFReader(LPCTSTR imagePath)
	: _exposureTime{ 0, 0 },
	_dateTaken{ 0 },
	_dateLastModified{ 0 }
{
	assert(imagePath != NULL);
	size_t  imagePathLen = _tcslen(imagePath);
	assert(imagePathLen > 0);

	// new implementation gets EXIF metadata using Exiv2 library
	// Exiv2 library returns more accurate values for some tags (e.g. focal length, lens name, etc)
	// with new approach zhe code will also become more simple to maintain in future
	Exiv2Parser::imageMetadata imageMeta = Exiv2Parser::GetImageMeta(imagePath);

	populateFromImageMeta(imageMeta);
}

CEXIFReader::~CEXIFReader(void) {
	delete _latitude;
	delete _longitude;
}

void CEXIFReader::WriteImageOrientation(int orientation) {
	if (_imageFormat == IF_JXL || _imageFormat == IF_HEIF || _imageFormat == IF_AVIF) {
		// orientation tag must be ignored for JXL and HEIF/AVIF
		return;
	}
	uint8* pTagOrientation = FindTag(_pFirstEntryIFD0, _pEndIFD0, EXIF::TAG_ORIENTATION, _littleEndian);
	if (pTagOrientation != NULL) {
		WriteShortTag(pTagOrientation, orientation, _littleEndian);
	}
}

void CEXIFReader::DeleteThumbnail() {
	// _pEndIFD0 points to 4-byte offset to next IFD (in our case IFD1 that holds thumbnail)
	// setting to zero means this is last IFD and so IFD1 with the thumbnail will not be found
	if (_pEndIFD0 != NULL) {
		std::memset(_pEndIFD0, 0, 4);
	}
}

void CEXIFReader::UpdateJPEGThumbnail(unsigned char* pJpegStream, int streamLen, int exifBlockLenCorrection, CSize sizeThumb) {
	if (!_thumbJpegEncoded) {
		return;
	}
	uint8* pTagOffsetSOI = FindTag(_pFirstEntryIFD1, _pEndIFD1, EXIF::TAG_JPEG_INTERCHANGE, _littleEndian);
	uint32 nOffsetSOI = ReadLongTag(pTagOffsetSOI, _littleEndian);
	uint8* pTIFFHeader = _pApp1 + 10;
	uint8* pSOI = pTIFFHeader + nOffsetSOI;
	memcpy(pSOI + 2, pJpegStream, streamLen);

	uint8* pTagJPEGBytes = FindTag(_pFirstEntryIFD1, _pEndIFD1, EXIF::TAG_JPEG_INTERCHANGE_LEN, _littleEndian);
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
		if (!m_LensInfo[i].IsZeroOrEmpty()) {
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
	if (m_LensInfo[2].IsZeroOrEmpty() && m_LensInfo[3].IsZero()) {
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

// copy data from image metadata to member variables of this class, so that the existing code can use it without change
void CEXIFReader::populateFromImageMeta(Exiv2Parser::imageMetadata& imageMeta)
{
	// get camera make
	_make = imageMeta.make.c_str();
	// return only the well known brand, for example "Olympus Imaging Corporation" is simplified to "Olympus"
	_make = getTargetIfContained(_make, _T("Olympus"));

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

	// get rating
	_rating = imageMeta.rating;

	// get date taken and last modified date
	tmToSystemTime(imageMeta.dateTaken, _dateTaken);
	tmToSystemTime(imageMeta.dateModified, _dateLastModified);

	// get basic exposure info
	_exposureTime = Rational(imageMeta.exposureTime);
	_exposureBias = Exiv2Parser::ConvertRationalToDouble(imageMeta.exposureBias);
	_aperture = imageMeta.aperture;
	_isoSpeed = imageMeta.isoSpeed;
	_flashFired = imageMeta.flashFired;
	_whiteBalanceMode = imageMeta.whiteBalance;
	_orientation = (uint8)imageMeta.orientation;

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

	// get thumbnail metadata info
	_hasThumb = imageMeta.hasThumb;
	_thumbJpegEncoded = imageMeta.thumbJpegEncoded;
	_thumbWidth = imageMeta.thumbWidth;
	_thumbHeight = imageMeta.thumbHeight;
	_thumbSizeInBytes = imageMeta.thumbSizeInBytes;
}
