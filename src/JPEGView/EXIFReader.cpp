#include "StdAfx.h"
#include "EXIFReader.h"
#include "ImageProcessingTypes.h"
#include "Helpers.h"
#include "EXIFHelpers.h"
#include "Exiv2Parser.h"

double CEXIFReader::UNKNOWN_DOUBLE_VALUE = 283740261.192864;

namespace EXIF {
	// IFD0 Tags (TIFF/EXIF Image IFD)
	constexpr uint16 TAG_IMAGE_DESCRIPTION = 0x010E;
	constexpr uint16 TAG_MAKE = 0x010F;
	constexpr uint16 TAG_MODEL = 0x0110;
	constexpr uint16 TAG_ORIENTATION = 0x0112;
	constexpr uint16 TAG_SOFTWARE = 0x0131;
	constexpr uint16 TAG_DATE_TIME = 0x0132;
	constexpr uint16 TAG_EXIF_IFD = 0x8769;
	constexpr uint16 TAG_GPS_IFD = 0x8825;

	// EXIF IFD Tags
	constexpr uint16 TAG_DATE_TIME_ORIGINAL = 0x9003;
	constexpr uint16 TAG_EXPOSURE_TIME = 0x829A;
	constexpr uint16 TAG_F_NUMBER = 0x829D;
	constexpr uint16 TAG_EXPOSURE_BIAS = 0x9204;
	constexpr uint16 TAG_FLASH = 0x9209;
	constexpr uint16 TAG_FOCAL_LENGTH = 0x920A;
	constexpr uint16 TAG_ISO_SPEED_RATINGS = 0x8827;
	constexpr uint16 TAG_ISO_SPEED_EXIF_V3 = 0x8833;  // EXIF 3.0 spec
	constexpr uint16 TAG_USER_COMMENT = 0x9286;
	constexpr uint16 TAG_XP_COMMENT = 0x9C9C;
	constexpr uint16 TAG_FOCAL_LENGHT_35MM = 0xA405;
	constexpr uint16 TAG_LENS_MODEL = 0xA434;
	constexpr uint16 TAG_LENS_INFO = 0xA432;

	// IFD1 Thumbnail Tags (TIFF)
	constexpr uint16 TAG_IMAGE_WIDTH = 0x0100;  // ImageWidth (for thumb)
	constexpr uint16 TAG_IMAGE_HEIGHT = 0x0101;  // ImageHeight (for thumb)
	constexpr uint16 TAG_COMPRESSION = 0x0103;
	constexpr uint16 TAG_JPEG_INTERCHANGE = 0x0201;  // JPEGInterchangeFormat
	constexpr uint16 TAG_JPEG_INTERCHANGE_LEN = 0x0202;

	// GPS IFD Tags
	constexpr uint16 TAG_GPS_LATITUDE_REF = 0x0001;
	constexpr uint16 TAG_GPS_LATITUDE = 0x0002;
	constexpr uint16 TAG_GPS_LONGITUDE_REF = 0x0003;
	constexpr uint16 TAG_GPS_LONGITUDE = 0x0004;
	constexpr uint16 TAG_GPS_ALTITUDE_REF = 0x0005;
	constexpr uint16 TAG_GPS_ALTITUDE = 0x0006;

	// Data Type Constants
	constexpr uint16 TYPE_ASCII = 0x0002;
	constexpr uint16 TYPE_SHORT = 0x0003;
	constexpr uint16 TYPE_LONG = 0x0004;
	constexpr uint16 TYPE_RATIONAL = 0x0005;
	constexpr uint16 TYPE_UNDEFINED = 0x0007;
	constexpr uint16 TYPE_SRATIONAL = 0x000A;

	// TIFF Header Byte Order (bytes 0-1)
	constexpr uint16 TIFF_LITTLE_ENDIAN = 0x4949;
	constexpr uint16 TIFF_BIG_ENDIAN = 0x4D4D;

	// TIFF Magic Number (bytes 2-3) - always 42
	constexpr uint16 TIFF_VERSION = 0x002A;
}

static uint32 ReadUInt(void * ptr, bool bLittleEndian) {
	uint32 nValue = *((uint32*)ptr);
	if (!bLittleEndian) {
		return _byteswap_ulong(nValue);
	} else {
		return nValue;
	}
}

static void WriteUInt(void * ptr, uint32 nValue, bool bLittleEndian) {
	uint32 nVal = nValue;
	if (!bLittleEndian) {
		nVal = _byteswap_ulong(nVal);
	}
	*((uint32*)ptr) = nVal;
}

static uint16 ReadUShort(void * ptr, bool bLittleEndian) {
	uint16 nValue = *((uint16*)ptr);
	if (!bLittleEndian) {
		return _byteswap_ushort(nValue);
	} else {
		return nValue;
	}
}

static void WriteUShort(void * ptr, uint16 nValue, bool bLittleEndian) {
	uint16 nVal = nValue;
	if (!bLittleEndian) {
		nVal = _byteswap_ushort(nVal);
	}
	*((uint16*)ptr) = nVal;
}

static uint8* FindTag(uint8* ptr, uint8* ptrLast, uint16 nTag, bool bLittleEndian) {
	while (ptr < ptrLast) {
		if (ReadUShort(ptr, bLittleEndian) == nTag) {
			return ptr;
		}
		ptr += 12;
	}
	return NULL;
}

static void ReadStringTag(CString & strOut, uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian, bool bTryReadAsUTF8 = false, uint32 maxLength = 65536) {
	try {
		if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == EXIF::TYPE_ASCII) {
			uint32 nSize = ReadUInt(ptr + 4, bLittleEndian);
			if (nSize > maxLength) {
				strOut.Empty();
				return;
			}
			LPCSTR pString = (nSize <= 4) ? (LPCSTR)(ptr + 8) : (LPCSTR)(pTIFFHeader + ReadUInt(ptr + 8, bLittleEndian));
			int nMaxChars = min(nSize, (int)strlen(pString));
			if (bTryReadAsUTF8) {
				CString strUTF8 = Helpers::TryConvertFromUTF8((uint8*)pString, nMaxChars);
				if (!strUTF8.IsEmpty()) {
					strOut = strUTF8;
					return;
				}
			}
			strOut = CString(pString, nMaxChars);
		} else {
			strOut.Empty();
		}
	} catch (...) {
		// EXIF corrupt?
		strOut.Empty();
	}
}

static void ReadUserCommentTag(CString & strOut, uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian) {
	try {
		if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == EXIF::TYPE_UNDEFINED) {
			int nSize = ReadUInt(ptr + 4, bLittleEndian);
			if (nSize > 10 && nSize <= 4096) {
				LPCSTR sCodeDesc = (LPCSTR)(pTIFFHeader + ReadUInt(ptr + 8, bLittleEndian));
				if (strcmp(sCodeDesc, "ASCII") == 0) {
					strOut = CString((LPCSTR)(sCodeDesc + 8), nSize - 8);
				} else if (strcmp(sCodeDesc, "UNICODE") == 0) {
					bool bLE = bLittleEndian;
					nSize -= 8;
					sCodeDesc += 8;
					if (sCodeDesc[0] == 0xFF && sCodeDesc[1] == 0xFE) {			// BOM UTF16 Little Endian
						bLE = true;
						nSize -= 2; sCodeDesc += 2;
					} else if (sCodeDesc[0] == 0xFE && sCodeDesc[1] == 0xFF) {	// BOM UTF16 Big Endian
						bLE = false;
						nSize -= 2; sCodeDesc += 2;
					}
					if (bLE) {
						strOut = CString((LPCWSTR)sCodeDesc, nSize / 2);
					} else {
						// swap 16 bit characters to little endian
						char* pString = new char[nSize];
						char* pSwap = pString;
						memcpy(pString, sCodeDesc, nSize);
						for (int i = 0; i < nSize / 2; i++) {
							char t = *pSwap;
							*pSwap = pSwap[1];
							pSwap[1] = t;
							pSwap += 2;
						}
						strOut = CString((LPCWSTR)pString, nSize / 2);
						delete[] pString;
					}
				}
			} // else comment is corrupt or too big
		}
	} catch (...) {
		// EXIF corrupt?
		strOut.Empty();
	}
}

static uint32 ReadLongTag(uint8* ptr, bool bLittleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == EXIF::TYPE_LONG) {
		return ReadUInt(ptr + 8, bLittleEndian);
	} else {
		return 0;
	}
}

static uint16 ReadShortTag(uint8* ptr, bool bLittleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == EXIF::TYPE_SHORT) {
		return ReadUShort(ptr + 8, bLittleEndian);
	} else {
		return 0;
	}
}

static uint32 ReadShortOrLongTag(uint8* ptr, bool bLittleEndian) {
	if (ptr != NULL) {
		uint16 nType = ReadUShort(ptr + 2, bLittleEndian);
		if (nType == EXIF::TYPE_SHORT) {
			return ReadUShort(ptr + 8, bLittleEndian);
		} else if (nType == EXIF::TYPE_LONG) {
			return ReadUInt(ptr + 8, bLittleEndian);
		}
	}
	return 0;
}

static void WriteShortTag(uint8* ptr, uint16 nValue, bool bLittleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == EXIF::TYPE_SHORT) {
		WriteUShort(ptr + 8, nValue, bLittleEndian);
	}
}

static void WriteLongTag(uint8* ptr, uint32 nValue, bool bLittleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == EXIF::TYPE_LONG) {
		WriteUInt(ptr + 8, nValue, bLittleEndian);
	}
}

static int ReadRationalTag(Rational & rational, uint8* ptr, uint8* pTIFFHeader, int index, bool bLittleEndian) {
	rational.Numerator = 0;
	rational.Denominator = 0;
	if (ptr != NULL) {
		uint16 nType = ReadUShort(ptr + 2, bLittleEndian);
		uint16 nCount = ReadUInt(ptr + 4, bLittleEndian);
		if (index < nCount) {
			if (nType == EXIF::TYPE_RATIONAL || nType == EXIF::TYPE_SRATIONAL) {
				int nOffset = ReadUInt(ptr + 8, bLittleEndian) + index * 8;
				rational.Numerator = ReadUInt(pTIFFHeader + nOffset, bLittleEndian);
				rational.Denominator = ReadUInt(pTIFFHeader + nOffset + 4, bLittleEndian);
				if (rational.Numerator != 0 && rational.Denominator != 0) {
					// Calculate the ggT
					uint32 nModulo;
					uint32 nA = (nType == EXIF::TYPE_SRATIONAL) ? abs((int)rational.Numerator) : rational.Numerator;
					uint32 nB = (nType == EXIF::TYPE_SRATIONAL) ? abs((int)rational.Denominator) : rational.Denominator;
					do {
						nModulo = nA % nB;
						nA = nB;
						nB = nModulo;
					} while (nB != 0);
					// normalize
					if (nType == EXIF::TYPE_SRATIONAL) {
						rational.Numerator = (int)rational.Numerator / (int)nA;
						rational.Denominator = (int)rational.Denominator / (int)nA;
					}
					else {
						rational.Numerator /= nA;
						rational.Denominator /= nA;
					}
				}
			}
		}
		return nType;
	}
	return 0;
}

static int ReadRationalTag(Rational & rational, uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian) {
	return ReadRationalTag(rational, ptr, pTIFFHeader, 0, bLittleEndian);
}

static bool ReadSignedRationalTag(SignedRational & rational, uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian) {
	ReadRationalTag((Rational &)rational, ptr, pTIFFHeader, bLittleEndian);
}

static double ReadDoubleTag(uint8* ptr, uint8* pTIFFHeader, int index, bool bLittleEndian) {
	Rational rational(0, 0);
	int nType = ReadRationalTag(rational, ptr, pTIFFHeader, index, bLittleEndian);
	if (rational.Denominator != 0) {
		if (nType == EXIF::TYPE_RATIONAL) {
			return (double)rational.Numerator / rational.Denominator;
		} else {
			int nNum = rational.Numerator;
			int nDenum = rational.Denominator;
			return (double)nNum/nDenum;
		}
	}
	return CEXIFReader::UNKNOWN_DOUBLE_VALUE;
}

static double ReadDoubleTag(uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian) {
	return ReadDoubleTag(ptr, pTIFFHeader, 0, bLittleEndian);
}

bool CEXIFReader::ParseDateString(SYSTEMTIME & date, const CString& str) {
	int nYear, nMonth, nDay, nHour, nMin, nSec;
	if (_stscanf(str, _T("%d:%d:%d %d:%d:%d"), &nYear, &nMonth, &nDay, &nHour, &nMin, &nSec) == 6) {
		date.wYear = nYear;
		date.wMonth = nMonth;
		date.wDay = nDay;
		date.wHour = nHour;
		date.wMinute = nMin;
		date.wSecond = nSec;
		date.wMilliseconds = 0;
		date.wDayOfWeek = 0;
		return true;
	}
	return false;
}

// same info may be in IFD0 or EXIF group, search prio: first IFD0 then EXIF
uint8* findTagInIFDs(uint8* pIFD0, uint8* pLastIFD0,
	uint8* pEXIFIFD, uint8* pLastEXIF,
	uint16 tagID, bool bLittleEndian) {

	uint8* pTag = FindTag(pIFD0, pLastIFD0, tagID, bLittleEndian);
	if (pTag == NULL && pEXIFIFD != NULL && pLastEXIF != NULL) {
		pTag = FindTag(pEXIFIFD, pLastEXIF, tagID, bLittleEndian);
	}
	return pTag;
}

CEXIFReader::CEXIFReader(void* pApp1Block, EImageFormat eImageFormat)
	: m_exposureTime{ 0, 0 },
	m_acqDate{ 0 },
	m_dateTime{ 0 }
{
	m_bFlashFired = false;
	m_bFlashFlagPresent = false;
	m_dFocalLength = m_dExposureBias = m_dFNumber = UNKNOWN_DOUBLE_VALUE;
	m_nISOSpeed = 0;
	m_nImageOrientation = 0;
	m_pTagOrientation = NULL;
	m_bLittleEndian = true;
	m_nThumbWidth = -1;
	m_nThumbHeight = -1;
	m_pLastIFD0 = NULL;
	m_pIFD1 = NULL;
	m_pLastIFD1 = NULL;
	m_bHasJPEGCompressedThumbnail = false;
	m_nJPEGThumbStreamLen = 0;
	m_pLatitude = NULL;
	m_pLongitude = NULL;
	m_dAltitude = UNKNOWN_DOUBLE_VALUE;
	m_dExposureBias = m_dFNumber = UNKNOWN_DOUBLE_VALUE;
	m_focalLengthEquivalent = 0;

	m_pApp1 = (uint8*)pApp1Block;
	
	// test call into Exiv2 lib
	CString lensInfo = CString(Exiv2Parser::GetLensModel(m_pApp1).c_str());

	// APP1 marker
	if (m_pApp1[0] != 0xFF || m_pApp1[1] != 0xE1) {
		return;
	}
	int nApp1Size = m_pApp1[2]*256 + m_pApp1[3] + 2;

	// Read TIFF header
	uint8* pTIFFHeader = m_pApp1 + 10;
	bool bLittleEndian;
	if (*(short*)pTIFFHeader == EXIF::TIFF_LITTLE_ENDIAN) {
		bLittleEndian = true;
	} else if (*(short*)pTIFFHeader == EXIF::TIFF_BIG_ENDIAN) {
		bLittleEndian = false;
	} else {
		return;
	}
	m_bLittleEndian = bLittleEndian;

	uint8* pIFD0 = pTIFFHeader + ReadUInt(pTIFFHeader + 4, bLittleEndian);
	if (pIFD0 - m_pApp1 >= nApp1Size) {
		return;
	}

	// Get IFD0 group start and end pointer
	uint16 nNumTags = ReadUShort(pIFD0, bLittleEndian);
	pIFD0 += 2;
	uint8* pLastIFD0 = pIFD0 + nNumTags*12;
	if (pLastIFD0 - m_pApp1 + 4 >= nApp1Size) {
		return;
	}
	uint32 nOffsetIFD1 = ReadUInt(pLastIFD0, bLittleEndian);
	m_pLastIFD0 = pLastIFD0;

	// Get EXIF group start and end pointer
	uint8* pTagEXIFIFD = FindTag(pIFD0, pLastIFD0, EXIF::TAG_EXIF_IFD, bLittleEndian);
	bool existsExifGroup = (pTagEXIFIFD != NULL);

	uint8* pEXIFIFD = NULL;
	uint8* pLastEXIF = NULL;

	if (existsExifGroup) {
		uint32 nOffsetEXIF = ReadLongTag(pTagEXIFIFD, bLittleEndian);
		if (nOffsetEXIF == 0) {
			return;
		}
		pEXIFIFD = pTIFFHeader + nOffsetEXIF;
		if (pEXIFIFD - m_pApp1 >= nApp1Size) {
			return;
		}
		nNumTags = ReadUShort(pEXIFIFD, bLittleEndian);
		pEXIFIFD += 2;
		pLastEXIF = pEXIFIFD + nNumTags * 12;
		if (pLastEXIF - m_pApp1 >= nApp1Size) {
			return;
		}
	}

	// image orientation
	uint8* pTagOrientation = NULL;
	// orientation tags must be ignored for JXL and HEIF/AVIF
	if (eImageFormat != IF_JXL && eImageFormat != IF_HEIF && eImageFormat != IF_AVIF) {
		pTagOrientation = FindTag(pIFD0, pLastIFD0, EXIF::TAG_ORIENTATION, bLittleEndian);
	}
	if (pTagOrientation != NULL) {
		m_nImageOrientation = ReadShortTag(pTagOrientation, bLittleEndian);
	}
	m_pTagOrientation = pTagOrientation;

	uint8* pTagModel = FindTag(pIFD0, pLastIFD0, EXIF::TAG_MODEL, bLittleEndian);
	ReadStringTag(m_sModel, pTagModel, pTIFFHeader, bLittleEndian);

	uint8* pTagImageDesc = FindTag(pIFD0, pLastIFD0, EXIF::TAG_IMAGE_DESCRIPTION, bLittleEndian);
	ReadStringTag(m_sImageDescription, pTagImageDesc, pTIFFHeader, bLittleEndian, true);
	if (_tcsicmp(m_sImageDescription, _T("OLYMPUS DIGITAL CAMERA")) == 0) {
		m_sImageDescription = _T("");
	}

	// Add the manufacturer name if not contained in model name
	if (!m_sModel.IsEmpty()) {
		CString sMake;
		uint8* pTagMake = FindTag(pIFD0, pLastIFD0, EXIF::TAG_MAKE, bLittleEndian);
		ReadStringTag(sMake, pTagMake, pTIFFHeader, bLittleEndian);
		if (!sMake.IsEmpty()) {
			int nSpace = sMake.Find(_T(' '));
			m_sMake = nSpace > 0 ? sMake.Left(nSpace) : sMake;
			if (m_sModel.Find(m_sMake) == -1) {
				m_sModel = m_sMake + _T(" ") + m_sModel;
			}
		}
	}

	uint8* pTagSoftware = FindTag(pIFD0, pLastIFD0, EXIF::TAG_SOFTWARE, bLittleEndian);
	ReadStringTag(m_sSoftware, pTagSoftware, pTIFFHeader, bLittleEndian);

	uint8* pTagModDate = FindTag(pIFD0, pLastIFD0, EXIF::TAG_DATE_TIME, bLittleEndian);
	CString sModDate;
	ReadStringTag(sModDate, pTagModDate, pTIFFHeader, bLittleEndian);
	ParseDateString(m_dateTime, sModDate);

	uint8* pTagGPSIFD = FindTag(pIFD0, pLastIFD0, EXIF::TAG_GPS_IFD, bLittleEndian);
	if (pTagGPSIFD != NULL) {
		ReadGPSData(pTIFFHeader, pTagGPSIFD, nApp1Size, bLittleEndian);
	}

	uint8* pTagAcquisitionDate = FindTag(pEXIFIFD, pLastEXIF, EXIF::TAG_DATE_TIME_ORIGINAL, bLittleEndian);
	CString sAcqDate;
	ReadStringTag(sAcqDate, pTagAcquisitionDate, pTIFFHeader, bLittleEndian);
	ParseDateString(m_acqDate, sAcqDate);

	uint8* pTagExposureTime = findTagInIFDs(pIFD0, pLastIFD0, pEXIFIFD, pLastEXIF,
		EXIF::TAG_EXPOSURE_TIME, bLittleEndian);
	ReadRationalTag(m_exposureTime, pTagExposureTime, pTIFFHeader, bLittleEndian);

	uint8* pTagExposureBias = FindTag(pEXIFIFD, pLastEXIF, EXIF::TAG_EXPOSURE_BIAS, bLittleEndian);
	m_dExposureBias = ReadDoubleTag(pTagExposureBias, pTIFFHeader, bLittleEndian);

	uint8* pTagFlash = FindTag(pEXIFIFD, pLastEXIF, EXIF::TAG_FLASH, bLittleEndian);
	uint16 nFlash = ReadShortTag(pTagFlash, bLittleEndian);
	m_bFlashFired = (nFlash & 1) != 0;
	m_bFlashFlagPresent = pTagFlash != NULL;

	uint8* pTagFocalLength = findTagInIFDs(pIFD0, pLastIFD0, pEXIFIFD, pLastEXIF,
		EXIF::TAG_FOCAL_LENGTH, bLittleEndian);
	ReadRationalTag(m_focalLength, pTagFocalLength, pTIFFHeader, bLittleEndian);

	uint8* pTagFNumber = findTagInIFDs(pIFD0, pLastIFD0, pEXIFIFD, pLastEXIF,
		EXIF::TAG_F_NUMBER, bLittleEndian);
	m_dFNumber = ReadDoubleTag(pTagFNumber, pTIFFHeader, bLittleEndian);

	uint8* pTagISOSpeed = findTagInIFDs(pIFD0, pLastIFD0, pEXIFIFD, pLastEXIF,
		EXIF::TAG_ISO_SPEED_RATINGS, bLittleEndian);
	uint8* pTagISOSpeed2 = FindTag(pEXIFIFD, pLastEXIF, EXIF::TAG_ISO_SPEED_EXIF_V3, bLittleEndian);	// EXIF 3.0 spec

	m_nISOSpeed = (pTagISOSpeed != NULL) ? ReadShortTag(pTagISOSpeed, bLittleEndian) :
		ReadLongTag(pTagISOSpeed2, bLittleEndian);

	uint8* pTagUserComment = FindTag(pEXIFIFD, pLastEXIF, EXIF::TAG_USER_COMMENT, bLittleEndian);
	ReadUserCommentTag(m_sUserComment, pTagUserComment, pTIFFHeader, bLittleEndian);
	// Samsung Galaxy puts this useless comment into each JPEG, just ignore
	if (m_sUserComment == "User comments") {	
		m_sUserComment = "";
	}

	uint8* pTagFocalLengthEquivalent = findTagInIFDs(pIFD0, pLastIFD0, pEXIFIFD, pLastEXIF,
		EXIF::TAG_FOCAL_LENGHT_35MM, bLittleEndian);
	m_focalLengthEquivalent = ReadShortTag(pTagFocalLengthEquivalent, bLittleEndian);

	uint8* pTagLensModel = FindTag(pEXIFIFD, pLastEXIF, EXIF::TAG_LENS_MODEL, bLittleEndian);
	ReadStringTag(m_sLensModel, pTagLensModel, pTIFFHeader, bLittleEndian);

	uint8* pTagLensInfo = FindTag(pEXIFIFD, pLastEXIF, EXIF::TAG_LENS_INFO, bLittleEndian);
	for (int i = 0; i < 4; i++) {
		ReadRationalTag(m_lensInfo[i], pTagLensInfo, pTIFFHeader, i, bLittleEndian);
	}

	// Read IFD1 (thumbnail image)

	// https://exiv2.org/tags.html
	// uint8* pTagXPComment = FindTag(pIFD0, pLastIFD0, EXIF::TAG_XP_COMMENT, bLittleEndian);  // this is the XPComment tag to resolve this issue https://github.com/sylikc/jpegview/issues/72 , but I'm not sure how to decode it


	if (nOffsetIFD1 != 0) {
		m_pIFD1 = pTIFFHeader + nOffsetIFD1;
		if (m_pIFD1 - m_pApp1 >= nApp1Size || m_pIFD1 - m_pApp1 < 0) {
			return;
		}
		nNumTags = ReadUShort(m_pIFD1, bLittleEndian);
		m_pIFD1 += 2;
		uint8* pLastIFD1 = m_pIFD1 + nNumTags*12;
		if (pLastIFD1 - m_pApp1 >= nApp1Size) {
			return;
		}
		m_pLastIFD1 = pLastIFD1;
		uint8* pTagCompression = FindTag(m_pIFD1, pLastIFD1, EXIF::TAG_COMPRESSION, bLittleEndian);
		if (pTagCompression == NULL) {
			return;
		}
		if (ReadShortTag(pTagCompression, bLittleEndian) == 6) {
			// compressed
			uint8* pTagOffsetSOI = FindTag(m_pIFD1, pLastIFD1, EXIF::TAG_JPEG_INTERCHANGE, bLittleEndian);
			uint8* pTagJPEGLen = FindTag(m_pIFD1, pLastIFD1, EXIF::TAG_JPEG_INTERCHANGE_LEN, bLittleEndian);
			if (pTagOffsetSOI != NULL && pTagJPEGLen != NULL) {
				uint32 nOffsetSOI = ReadLongTag(pTagOffsetSOI, bLittleEndian);
				uint32 nJPEGBytes = ReadLongTag(pTagJPEGLen, bLittleEndian);
				uint8* pSOI = pTIFFHeader + nOffsetSOI;
				uint8* pSOF = (uint8*) Helpers::FindJPEGMarker(pSOI, nJPEGBytes, 0xC0);
				if (pSOF != NULL) {
					m_nThumbWidth = pSOF[7]*256 + pSOF[8];
					m_nThumbHeight = pSOF[5]*256 + pSOF[6];
					m_nJPEGThumbStreamLen = nJPEGBytes;
					m_bHasJPEGCompressedThumbnail = true;
				}
			}
		} else {
			// uncompressed
			uint8* pTagThumbWidth = FindTag(m_pIFD1, pLastIFD1, EXIF::TAG_IMAGE_WIDTH, bLittleEndian);
			uint8* pTagThumbHeight = FindTag(m_pIFD1, pLastIFD1, EXIF::TAG_IMAGE_HEIGHT, bLittleEndian);
			if (pTagThumbWidth != NULL && pTagThumbHeight != NULL) {
				m_nThumbWidth = ReadShortOrLongTag(pTagThumbWidth, bLittleEndian);
				m_nThumbHeight = ReadShortOrLongTag(pTagThumbHeight, bLittleEndian);
			}
		}
	}
}

CEXIFReader::~CEXIFReader(void) {
	delete m_pLatitude;
	delete m_pLongitude;
}

void CEXIFReader::WriteImageOrientation(int nOrientation) {
	if (m_pTagOrientation != NULL && ImageOrientationPresent()) {
		WriteShortTag(m_pTagOrientation, nOrientation, m_bLittleEndian);
	}
}

void CEXIFReader::DeleteThumbnail() {
	if (m_pLastIFD0 != NULL) {
		*m_pLastIFD0 = 0;
	}
}

void CEXIFReader::UpdateJPEGThumbnail(unsigned char* pJPEGStream, int nStreamLen, int nEXIFBlockLenCorrection, CSize sizeThumb) {
	if (!m_bHasJPEGCompressedThumbnail) {
		return;
	}
	uint8* pTagOffsetSOI = FindTag(m_pIFD1, m_pLastIFD1, EXIF::TAG_JPEG_INTERCHANGE, m_bLittleEndian);
	uint32 nOffsetSOI = ReadLongTag(pTagOffsetSOI, m_bLittleEndian);
	uint8* pTIFFHeader = m_pApp1 + 10;
	uint8* pSOI = pTIFFHeader + nOffsetSOI;
	memcpy(pSOI + 2, pJPEGStream, nStreamLen);

	uint8* pTagJPEGBytes = FindTag(m_pIFD1, m_pLastIFD1, EXIF::TAG_JPEG_INTERCHANGE_LEN, m_bLittleEndian);
	WriteLongTag(pTagJPEGBytes, nStreamLen + 2, m_bLittleEndian);
	int nNewApp1Size = m_pApp1[2]*256 + m_pApp1[3] + nEXIFBlockLenCorrection;
	m_pApp1[2] = nNewApp1Size >> 8;
	m_pApp1[3] = nNewApp1Size & 0xFF;
}

void CEXIFReader::ReadGPSData(uint8* pTIFFHeader, uint8* pTagGPSIFD, int nApp1Size, bool bLittleEndian) {
	uint32 nOffsetGPS = ReadLongTag(pTagGPSIFD, bLittleEndian);
	if (nOffsetGPS == 0) {
		return;
	}
	uint8* pGPSIFD = pTIFFHeader + nOffsetGPS;
	if (pGPSIFD - m_pApp1 >= nApp1Size) {
		return;
	}
	int nNumTags = ReadUShort(pGPSIFD, bLittleEndian);
	pGPSIFD += 2;
	uint8* pLastGPS = pGPSIFD + nNumTags * 12;
	if (pLastGPS - m_pApp1 >= nApp1Size) {
		return;
	}

	uint8* pTagLatitudeRef = FindTag(pGPSIFD, pLastGPS, EXIF::TAG_GPS_LATITUDE_REF, bLittleEndian);
	if (pTagLatitudeRef == NULL)
		return;
	CString latitudeRef;
	ReadStringTag(latitudeRef, pTagLatitudeRef, pTIFFHeader, bLittleEndian, false, 2);

	uint8* pTagLatitude = FindTag(pGPSIFD, pLastGPS, EXIF::TAG_GPS_LATITUDE, bLittleEndian);
	m_pLatitude = ReadGPSCoordinate(pTIFFHeader, pTagLatitude, latitudeRef, bLittleEndian);

	uint8* pTagLongitudeRef = FindTag(pGPSIFD, pLastGPS, EXIF::TAG_GPS_LONGITUDE_REF, bLittleEndian);
	if (pTagLongitudeRef == NULL)
		return;
	CString longitudeRef;
	ReadStringTag(longitudeRef, pTagLongitudeRef, pTIFFHeader, bLittleEndian, false, 2);

	uint8* pTagLongitude = FindTag(pGPSIFD, pLastGPS, EXIF::TAG_GPS_LONGITUDE, bLittleEndian);
	m_pLongitude = ReadGPSCoordinate(pTIFFHeader, pTagLongitude, longitudeRef, bLittleEndian);

	uint8* pTagAltitude = FindTag(pGPSIFD, pLastGPS, EXIF::TAG_GPS_ALTITUDE, bLittleEndian);
	if (pTagAltitude != NULL) {
		m_dAltitude = ReadDoubleTag(pTagAltitude, pTIFFHeader, bLittleEndian);
		uint8* pTagAltitudeRef = FindTag(pGPSIFD, pLastGPS, EXIF::TAG_GPS_ALTITUDE_REF, bLittleEndian);
		if (pTagAltitudeRef != NULL && *(pTagAltitudeRef + 8) == 1) {
			m_dAltitude *= -1;
		}
	}
}

GPSCoordinate* CEXIFReader::ReadGPSCoordinate(uint8* pTIFFHeader, uint8* pTagLatOrLong, LPCTSTR reference, bool bLittleEndian) {
	if (pTagLatOrLong == NULL)
		return NULL;
	double dDeg = ReadDoubleTag(pTagLatOrLong, pTIFFHeader, 0, bLittleEndian);
	double dMin = ReadDoubleTag(pTagLatOrLong, pTIFFHeader, 1, bLittleEndian);
	double dSec = ReadDoubleTag(pTagLatOrLong, pTIFFHeader, 2, bLittleEndian);
	if (dDeg == UNKNOWN_DOUBLE_VALUE || dMin == UNKNOWN_DOUBLE_VALUE || dSec == UNKNOWN_DOUBLE_VALUE)
		return NULL;

	return new GPSCoordinate(reference, dDeg, dMin, dSec);
}


double CEXIFReader::CalcFocalLengthEquiv(double focalLength) {
	return EXIFHelpers::CalcFocalLengthEquivalent(m_sMake, m_sModel, focalLength);
}

// Print conversion for lens info
// array [0..3] of rational values: (min focal, max focal, min F, max F)
// returns: string in the form "12-20mm f/3.8-4.5" or "50mm f/1.4"

CString CEXIFReader::GetLensInfo() {
	CString focalRange;

	bool isEmpty = true;
	for (int i = 0; i < 4; i++) {
		isEmpty = isEmpty && m_lensInfo[i].IsEmpty();
	}
	if (isEmpty) {
		return _T("");
	}

	double minFocal = m_lensInfo[0];
	double maxFocal = m_lensInfo[1];
	if (abs(maxFocal - minFocal) < 1E-3) {
		focalRange.Format(_T("%.0fmm"), minFocal);
	}
	else {
		focalRange.Format(_T("%.0f-%.0fmm"), minFocal, maxFocal);
	}

	CString fNumberRange;
	if (m_lensInfo[2].IsEmpty() && m_lensInfo[3].IsEmpty()) {
		fNumberRange = _T("");
	}
	else {
		double minFNumber = m_lensInfo[2];
		double maxFNumber = m_lensInfo[3];
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
