#include "StdAfx.h"
#include "EXIFHelpers.h"
#include "JPEGImage.h"
#include "EXIFReader.h"
#include <gdiplus.h>
#include <gdiplusimaging.h>
#include <vector>
#include <map>

namespace EXIFHelpers {

	static void FindFiles(LPCTSTR sDirectory, LPCTSTR sEnding, std::list<CString>& fileList) {
		CFindFile fileFind;
		CString sPattern = CString(sDirectory) + _T('\\') + sEnding;
		if (fileFind.FindFile(sPattern)) {
			fileList.push_back(fileFind.GetFilePath());
			while (fileFind.FindNextFile()) {
				fileList.push_back(fileFind.GetFilePath());
			}
		}
	}

	bool SetModificationDate(LPCTSTR sFileName, const SYSTEMTIME& time) {
		HANDLE hFile = ::CreateFile(sFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			return false;
		}
		FILETIME ft;
		::SystemTimeToFileTime(&time, &ft);  // converts to file time format
		BOOL bOk = ::SetFileTime(hFile, NULL, NULL, &ft);
		::CloseHandle(hFile);
		return bOk;
	}

	bool SetModificationDateToEXIF(LPCTSTR sFileName, CJPEGImage* pImage) {
		if (pImage->GetEXIFReader() == NULL || !pImage->GetEXIFReader()->GetAcquisitionTimePresent()) {
			return false;
		}
		SYSTEMTIME st = pImage->GetEXIFReader()->GetAcquisitionTime();
		// EXIF times are always local times, convert to UTC
		TIME_ZONE_INFORMATION tzi;
		::GetTimeZoneInformation(&tzi);
		::TzSpecificLocalTimeToSystemTime(&tzi, &st, &st);
		return SetModificationDate(sFileName, st);
	}

	bool SetModificationDateToEXIF(LPCTSTR sFileName) {
		bool bSuccess = false;
		Gdiplus::Bitmap* pBitmap = new Gdiplus::Bitmap(sFileName);
		if (pBitmap->GetLastStatus() == Gdiplus::Ok) {
			int nSize = pBitmap->GetPropertyItemSize(PropertyTagExifDTOrig);
			if (nSize > 0) {
				Gdiplus::PropertyItem* pItem = (Gdiplus::PropertyItem*)malloc(nSize);
				if (pBitmap->GetPropertyItem(PropertyTagExifDTOrig, nSize, pItem) == Gdiplus::Ok) {
					SYSTEMTIME time;
					if (CEXIFReader::ParseDateString(time, CString((char*)pItem->value))) {
						delete pBitmap; pBitmap = NULL; // else the file is locked
						TIME_ZONE_INFORMATION tzi;
						::GetTimeZoneInformation(&tzi);
						::TzSpecificLocalTimeToSystemTime(&tzi, &time, &time);
						bSuccess = SetModificationDate(sFileName, time);
					}
				}
				free(pItem);
			}
		}
		delete pBitmap;
		return bSuccess;
	}

	EXIFResult SetModificationDateToEXIFAllFiles(LPCTSTR sDirectory) {
		std::list<CString> listFileNames;
		FindFiles(sDirectory, _T("*.jpg"), listFileNames);
		FindFiles(sDirectory, _T("*.jpeg"), listFileNames);

		int nNumFailed = 0;
		std::list<CString>::iterator iter;
		for (iter = listFileNames.begin(); iter != listFileNames.end(); iter++) {
			if (!SetModificationDateToEXIF(*iter)) {
				nNumFailed++;
			}
		}

		EXIFResult result;
		result.NumberOfSucceededFiles = (int)listFileNames.size() - nNumFailed;
		result.NumberOfFailedFiles = nNumFailed;
		return result;
	}

	// Simple lookup approach to get crop factor from camera make, model and focal length of lens in mm.
	//
	// The code requires adding crop entries to cropData vector, for now it only contains
	// crop factor for few cameras.
	// If a device has more than one lens (e.g. mobile phone with multiple lenses) then 
	// add an entry per focal length of lens in mm

	// make, model and focal length are used as keys to get associated crop factor
	struct CropEntry {
		CString make;
		CString model;
		double focalLength;
		double cropFactor;
	};

	const std::vector<CropEntry> cropData = {
		{ "OLYMPUS", "E-1", 0.0, 2.0 },
		{ "OLYMPUS", "E-3", 0.0, 2.0 },
		{ "OLYMPUS", "E-5", 0.0, 2.0 },
		{ "OLYMPUS", "E-30", 0.0, 2.0 },
		{ "OLYMPUS", "E-300", 0.0, 2.0 },
		{ "OLYMPUS", "E-330", 0.0, 2.0 },
		{ "OLYMPUS", "E-400", 0.0, 2.0 },
		{ "OLYMPUS", "E-410", 0.0, 2.0 },
		{ "OLYMPUS", "E-420", 0.0, 2.0 },
		{ "OLYMPUS", "E-450", 0.0, 2.0 },
		{ "OLYMPUS", "E-500", 0.0, 2.0 },
		{ "OLYMPUS", "E-510", 0.0, 2.0 },
		{ "OLYMPUS", "E-520", 0.0, 2.0 },
		{ "OLYMPUS", "E-600", 0.0, 2.0 },
		{ "OLYMPUS", "E-620", 0.0, 2.0 },
		{ "OLYMPUS", "E-M1", 0.0, 2.0 },
		{ "CANON", "PowerShot S100", 0.0, 4.6 },
		{ "CANON", "EOS 7D", 0.0, 1.6 },
		{ "CANON", "EOS 500D", 0.0, 1.6 },
		{ "PANASONIC", "DMC-FZ5", 0.0, 6.0 },
		{ "PANASONIC", "DMC-FZ200", 0.0, 5.6 },
		{ "PANASONIC", "DMC-FZ300", 0.0, 5.6 },
		{ "SONY", "DSC-RX100", 0.0, 2.7 },
		{ "HUAWEI", "HMA-L29", 2.26, 7.522 },
		{ "HUAWEI", "HMA-L29", 3.84, 6.771 },
		{ "HUAWEI", "HMA-L29", 3.95, 13.671 },
		{ "HUAWEI", "HMA-L29", 4.75, 5.684 },
	};

	// crop map using std::map and std::vector
	using CropMap = std::map<CString, std::vector<CropEntry>>;

	// build crop map grouped by make
	CropMap InitializeCropMap(const std::vector<CropEntry>& data) {
		CropMap mapByMake;
		for (const CropEntry& entry : data) {
			mapByMake[entry.make].push_back(entry);
		}
		return mapByMake;
	}

	// case-insensitive ToUpper helper for CString, preserves original string by creating a copy
	CString ToUpper(const CString& str) {
		CString upper = str;
		upper.MakeUpper();
		return upper;
	}

	// Compute the crop factor (same logic as original)
	double GetCropFactor(const CString make, const CString model, double focalLength) {
		static bool initialized = false;
		static CropMap cropMapData;

		if (!initialized) {
			cropMapData = InitializeCropMap(cropData);
			initialized = true;
		}

		const double epsilon = 0.001;
		double cropFactor = 0.0;

		CString makeUpper = ToUpper(make);
		CString modelUpper = ToUpper(model);

		auto it = cropMapData.find(makeUpper);
		if (it != cropMapData.end()) {
			for (const auto& entry : it->second) {
				CString entryModelUpper = ToUpper(entry.model);
				if (modelUpper.Find(entryModelUpper) >= 0) {
					if (entry.focalLength == 0.0 ||
						fabs(entry.focalLength - focalLength) < epsilon) {
						cropFactor = entry.cropFactor;
						break;
					}
				}
			}
		}

		return cropFactor;
	}

	double CalcFocalLengthEquivalent(const CString make, const CString model, double focalLength) {
		return round(focalLength * GetCropFactor(make, model, focalLength));
	}
}