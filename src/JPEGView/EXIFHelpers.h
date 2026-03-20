#pragma once

class CJPEGImage;

namespace EXIFHelpers {

	struct EXIFResult {
		int NumberOfSucceededFiles;
		int NumberOfFailedFiles;
	};

	// Sets the modification date of the given file to the given time in UTC (Caution!)
	bool SetFileModifiedDate(LPCTSTR sFileName, const SYSTEMTIME& time);

	// Sets the modification date of the given file to the EXIF date of the given image
	bool SetFileModifiedDateToDateTaken(LPCTSTR sFileName, CJPEGImage* pImage);

	// Sets the modification date of the given file to the EXIF date, using GDI+ to read the EXIF date
	bool SetFileModifiedDateToDateTaken(LPCTSTR sFileName);

	// Sets the modification date of all JPEG files in the given directory to the EXIF date.
	EXIFResult SetFileModifiedDateToDateTakenInAllFiles(LPCTSTR sDirectory);

	// for given camera make and model, return crop factor
	double GetCropFactor(const CString make, const CString model, double focalLength);

	// for given camera make, model and focal length, return crop factor
	double CalcFocalLengthEquivalent(const CString make, const CString model, double focalLength);

	// returns true if source contains search, ignoring case
	bool ContainsCaseInsensitive(CString source, CString search);

	// removes make from model if model contains make to avoid redundant information
	CString RemoveMakeFromModel(const CString& make, const CString& model);
}