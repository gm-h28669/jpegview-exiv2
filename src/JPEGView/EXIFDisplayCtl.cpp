#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "EXIFDisplayCtl.h"
#include "EXIFDisplay.h"
#include "RawMetadata.h"
#include "SettingsProvider.h"
#include "HelpersGUI.h"
#include "NLS.h"
#include "EXIFHelpers.h"

#undef SHOW_FILENAME
#define SHOW_COMPACT

constexpr const TCHAR HIGHLIGHT_FONT[] = _T("\"Consolas\" 12.0 bold");
constexpr const TCHAR ITEM_SEPARATOR[] = _T(" • ");
constexpr const int SPLIT_LINE_LENGTH = 30;

static int GetFileNameHeight(HDC dc) {
	CSize size;
	HelpersGUI::SelectDefaultFileNameFont(dc);
	::GetTextExtentPoint32(dc, _T("("), 1, &size);
	return size.cy;
}

static CString FormatGeoCoordinates(GPSCoordinate* latitude, GPSCoordinate* longitude) {
	const int BUFF_SIZE = 96;
	TCHAR buff[BUFF_SIZE];
	_stprintf_s(buff, BUFF_SIZE, _T("%.0f° %.0f' %.0f'' %s / %.0f° %.0f' %.0f'' %s"),
		latitude->Degrees, latitude->Minutes, latitude->Seconds, latitude->RelativePos,
		longitude->Degrees, longitude->Minutes, longitude->Seconds, longitude->RelativePos);
	return CString(buff);
}

static CString GenerateMapURL(GPSCoordinate* latitude, GPSCoordinate* longitude) {
	double lng = longitude->Degrees + longitude->Minutes / 60 + longitude->Seconds / (60 * 60);
	if (_tcsicmp(longitude->RelativePos, _T("W")) == 0)
		lng = -lng;

	double lat = latitude->Degrees + latitude->Minutes / 60 + latitude->Seconds / (60 * 60);
	if (_tcsicmp(latitude->RelativePos, _T("S")) == 0)
		lat = -lat;

	CString mapProvider = CSettingsProvider::This().GPSMapProvider();

	const int BUFF_SIZE = 32;
	TCHAR buffLat[BUFF_SIZE];
	TCHAR buffLng[BUFF_SIZE];
	_stprintf_s(buffLat, BUFF_SIZE, _T("%.5f"), lat);
	_stprintf_s(buffLng, BUFF_SIZE, _T("%.5f"), lng);

	mapProvider.Replace(_T("{lat}"), buffLat);
	mapProvider.Replace(_T("{lng}"), buffLng);

	return mapProvider;
}

static bool IsEmpty(LPCTSTR comment) {
	if (comment == NULL || comment[0] == _T('\0') ||
		(std::basic_string<TCHAR>(comment).find_first_not_of(_T(" \t\n\r\f\v")) == std::basic_string<TCHAR>::npos)) {
		return true;  // The comment is considered empty
	}
	return false; // The comment has content
}

static CString JoinWithSeparator(const std::vector<CString>& strings, const CString& separator) {
	CString result;

	for (size_t i = 0; i < strings.size(); ++i) {
		if (!strings[i].IsEmpty()) {
			if (!result.IsEmpty()) {
				result += separator; // Add the separator before the next string
			}
			result += strings[i]; // Add the current non-empty string
		}
	}

	return result;
}

static CString GetFullCameraName (CString& make, CString& model) {
	if (make.IsEmpty()) {
		return model;
	}
	return make + _T(" ") + model;
}

CEXIFDisplayCtl::CEXIFDisplayCtl(CMainDlg* mainDlg, CPanel* imageProcessingPanel) : CPanelController(mainDlg, false) {
	_visible = CSettingsProvider::This().ShowFileInfo();
	_fileNameHeight = 0;
	_imageProcessingPanel = imageProcessingPanel;
	m_pPanel = _exifDisplay = new CEXIFDisplay(mainDlg->m_hWnd, this);
	_exifDisplay->GetControl<CButtonCtrl*>(CEXIFDisplay::ID_btnShowHideHistogram)->SetButtonPressedHandler(&OnShowHistogram, this);
	CButtonCtrl* pCloseBtn = _exifDisplay->GetControl<CButtonCtrl*>(CEXIFDisplay::ID_btnClose);
	pCloseBtn->SetButtonPressedHandler(&OnClose, this);
	pCloseBtn->SetShow(false);
	_exifDisplay->SetShowHistogram(CSettingsProvider::This().ShowHistogram());
}

CEXIFDisplayCtl::~CEXIFDisplayCtl() {
	delete _exifDisplay;
	_exifDisplay = NULL;
}

bool CEXIFDisplayCtl::IsVisible() { 
	return CurrentImage() != NULL && _visible; 
}

void CEXIFDisplayCtl::SetVisible(bool visible) {
	if (_visible != visible) {
		_visible = visible;
		InvalidateMainDlg();
	}
}

void CEXIFDisplayCtl::SetActive(bool active) {
	SetVisible(active);
}

void CEXIFDisplayCtl::AfterNewImageLoaded() {
	_exifDisplay->ClearTexts();
	_exifDisplay->SetHistogram(NULL);
}

void CEXIFDisplayCtl::OnPrePaintMainDlg(HDC hPaintDC) {
	if (m_pMainDlg->IsShowFileName() && _fileNameHeight == 0) {
		_fileNameHeight = GetFileNameHeight(hPaintDC);
	}
	_exifDisplay->SetPosition(CPoint(_imageProcessingPanel->PanelRect().left, m_pMainDlg->IsShowFileName() ? _fileNameHeight + 6 : 0));
	FillEXIFDataDisplay();
	if (CurrentImage() != NULL && _exifDisplay->GetShowHistogram()) {
		_exifDisplay->SetHistogram(CurrentImage()->GetProcessedHistogram());
	}
}

void CEXIFDisplayCtl::DisplayImageSizeInfo(int width, int height) {
	double megaPixels = (static_cast<double>(width) * height) / 1'000'000.0;
	_exifDisplay->AddLine(_T("%sMP%s%sx%s"),
		_T(""),
		_exifDisplay->FormatNumber(megaPixels, 1),
		ITEM_SEPARATOR,
		_exifDisplay->FormatNumber(static_cast<double>(width), 0),
		_exifDisplay->FormatNumber(static_cast<double>(height), 0));
}

void CEXIFDisplayCtl::DisplayDateTakenOrLastModifiedTime(bool hasDateTaken, SYSTEMTIME dateTaken, const FILETIME* pFileModifiedTime) {
	if (hasDateTaken) {
		_exifDisplay->AddLine(CNLS::GetString(_T("Acquisition date:")), dateTaken);
	}
	else {
		if (pFileModifiedTime != NULL) {
			_exifDisplay->AddLine(CNLS::GetString(_T("Modification date:")), *pFileModifiedTime);
		}
	}
}

void CEXIFDisplayCtl::DisplayLocationInfo(GPSCoordinate* latitude, GPSCoordinate* longitude, bool hasAltitude, double altitude, 
	LPCTSTR descriptionLocation, LPCTSTR descricptionAltitude, bool useBoldFont) {
	CString gpsLocation = FormatGeoCoordinates(latitude, longitude);
	_exifDisplay->SetGPSLocation(gpsLocation, GenerateMapURL(latitude, longitude), useBoldFont);
	_exifDisplay->AddLine(descriptionLocation, gpsLocation, true);

	if (hasAltitude) {
		CString sAltitude = _exifDisplay->FormatNumber(altitude, 0);
		if (useBoldFont) {
			_exifDisplay->AddFont(HIGHLIGHT_FONT);
		}
		_exifDisplay->AddLine(_T("↑ %sm"), descricptionAltitude, sAltitude);
	}
}

void CEXIFDisplayCtl::FillEXIFDataDisplay() {
	_exifDisplay->ClearTexts();
	_exifDisplay->SetHistogram(NULL);

	CString prefix, fileTitle;
	const CFileList* fileList = m_pMainDlg->GetFileList();

#ifdef SHOW_FILENAME
	// show filename and file index (if option "Show Filename" is enabled this is duplicate info)
	LPCTSTR currentFileName = m_pMainDlg->CurrentFileName(true);

	if (CurrentImage()->IsClipboardImage()) {
		prefix = currentFileName;
	} else if (fileList->Current() != NULL) {
		prefix.Format(_T("[%d/%d]"), fileList->CurrentIndex() + 1, fileList->Size());
		fileTitle = currentFileName;
		fileTitle += Helpers::GetMultiframeIndex(m_pMainDlg->GetCurrentImage());
	}
#endif

	LPCTSTR comment = NULL;
	_exifDisplay->AddPrefix(prefix);
	_exifDisplay->AddTitle(fileTitle);

	if (!CurrentImage()->IsClipboardImage()) {
		CEXIFReader* exifReader = CurrentImage()->GetEXIFReader();
		CRawMetadata* rawMeta = CurrentImage()->GetRawMetadata();
		if (exifReader != NULL) {
			if (exifReader->HasUserComment()) {
				comment = exifReader->GetUserComment();
			}
			else if (exifReader->HasDescription()) {
				comment = exifReader->GetDescription();
			}

#ifdef SHOW_COMPACT
			_exifDisplay->AddFont(HIGHLIGHT_FONT);

			// display date taken (if missing display last modified time)
			if (exifReader->HasDateTaken()) {
				_exifDisplay->AddLine(_T(""), exifReader->GetDateTaken());
			}
			else {
				const FILETIME* fileDateTime = fileList->CurrentModificationTime();
				if (fileDateTime != NULL) {
					_exifDisplay->AddLine(_T(""), *fileDateTime);
				}
			}

			// show exposure time, aperture, ISO speed, exposure bias, focal length, 35mm focal length equivalent and 
			// flash indication in one line
			CString exposureTime = exifReader->HasExposureTime()
				? CString(_exifDisplay->FormatNumber(exifReader->GetExposureTime())) + _T("s") : _T("");
			CString aperture = (exifReader->HasApertureValue())
				? _T("f/") + CString(_exifDisplay->FormatNumber(exifReader->GetApertureValue(), 1)) : _T("");
			CString isoSpeed = exifReader->HasISOSpeed()
				? _T("ISO ") + CString(_exifDisplay->FormatNumber(exifReader->GetISOSpeed())) : _T("");
			CString exposureBias = exifReader->HasExposureBias()
				? _T("EV") + CString(_exifDisplay->FormatNumber(exifReader->HasExposureBias(), 1, true)) : _T("");
			CString focalLength = exifReader->HasFocalLength()
				? _exifDisplay->FormatNumber(exifReader->GetFocalLength()) : _T("");
			double focalLengthEquivalent = GetFocalLenghtEquiv(exifReader);
			if (focalLengthEquivalent > 0.0) {
				focalLength += _T("➞") + CString(_exifDisplay->FormatNumber(focalLengthEquivalent)) + _T("mm");
			}
			else if (exifReader->HasFocalLength()) {
				focalLength += _T("mm");
			}
			CString flashFired = exifReader->GetFlashFired() ? _T("⚡") : _T("");

			std::vector<CString> parts = { exposureTime, aperture, isoSpeed, exposureBias, focalLength, flashFired };
			CString result = JoinWithSeparator(parts, ITEM_SEPARATOR);
			if (!result.IsEmpty()) {
				_exifDisplay->AddLine(_T("%s"), _T(""), result);
			}

			// display image size: width x height and size in Megapixels
			int imgWidth = CurrentImage()->OrigWidth();
			int imgHeight = CurrentImage()->OrigHeight();
			DisplayImageSizeInfo(imgWidth, imgHeight);
			_exifDisplay->AddEmptyLine();

			// get camera make and model
			CString make = exifReader->GetMake();
			CString model = exifReader->GetModel();
			CString camera = GetFullCameraName(make, model);

			// get lens name: try to get it from Exiv2, if not found then use lens info to build
			// a suitable string
			CString lens = exifReader->HasLensName()
				? exifReader->GetLensName() : _T("");

			// if no lens name then use lens info if available
			// lens info contains information about zoom range and min/max aperture
			if (lens.IsEmpty()) {
				lens = exifReader->HasLensInfo()
					? exifReader->GetLensInfo() : _T("");
			}

			if ((camera.GetLength() + lens.GetLength()) < SPLIT_LINE_LENGTH) {
				// show camera name and lens name on one line
				parts = { camera, lens };
				result = JoinWithSeparator(parts, ITEM_SEPARATOR);
				if (!result.IsEmpty()) {
					_exifDisplay->AddLine(_T("%s"), _T(""), result);
				}
			}
			else {
				// show camera name and lens name on separate lines
				_exifDisplay->AddLine(_T("%s"), _T(""), camera);
				_exifDisplay->AddLine(_T("%s"), _T(""), lens);
			}

			// display GPS location if available and also as a URL to open the location in a map application (Google Maps, Bing Maps, etc. depending on user settings)
			if (exifReader->HasGPSLocation()) {
				GPSCoordinate* latitude = exifReader->GetGPSLatitude();
				GPSCoordinate* longitude = exifReader->GetGPSLongitude();
				bool hasAltidue = exifReader->HasAltitude();
				double altitude = hasAltidue ? exifReader->GetGPSAltitude() : 0.0;
				DisplayLocationInfo(latitude, longitude, hasAltidue, altitude, _T(""), _T(""), true);
			}
#else
			// display date taken (if missing display last modified time)
			bool hasDateTaken = exifReader->HasDateTaken();
			SYSTEMTIME dateTaken = exifReader->GetDateTaken();
			const FILETIME * pFileModifiedTime = fileList->CurrentModificationTime();
			DisplayDateTakenOrLastModifiedTime(hasDateTaken, dateTaken, pFileModifiedTime);

			// display basic exposure information
			if (exifReader->HasExposureTime()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("Exposure time (s):")), exifReader->GetExposureTime());
			}
			if (exifReader->HasApertureValue()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("F-Number:")), exifReader->GetApertureValue(), 1);
			}
			if (exifReader->HasISOSpeed()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("ISO Speed:")), (int)exifReader->GetISOSpeed());
			}			
			if (exifReader->HasExposureBias()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("Exposure bias (EV):")), exifReader->GetExposureBias(), 2);
			}
			if (exifReader->HasFocalLength()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("Focal length (mm):")), exifReader->GetFocalLength(), 1);
			}
			double focalLengthEquivalent = GetFocalLenghtEquiv(exifReader);
			if (focalLengthEquivalent > 0.0) {
				_exifDisplay->AddLine(CNLS::GetString(_T("35mm Eq. focal length (mm):")), focalLengthEquivalent, 1);
			}
			if (exifReader->GetFlashFired()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("Flash fired:")), exifReader->GetFlashFired() ? CNLS::GetString(_T("yes")) : CNLS::GetString(_T("no")));
			}

			// display image size
			_exifDisplay->AddLine(CNLS::GetString(_T("Image width:")), CurrentImage()->OrigWidth());
			_exifDisplay->AddLine(CNLS::GetString(_T("Image height:")), CurrentImage()->OrigHeight());

			// display camera name
			CString make = exifReader->GetMake();
			CString model = exifReader->GetModel();
			CString camera = GetFullCameraName(make, model);
			if (!camera.IsEmpty()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("Camera model:")), camera);
			}

			// display software used for creating or editing
			if (exifReader->HasSoftware()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("Software:")), exifReader->GetSoftware());
			}

			// display GPS location if available
			if (exifReader->HasGPSLocation()) {
				GPSCoordinate* latitude = exifReader->GetGPSLatitude();
				GPSCoordinate* longitude = exifReader->GetGPSLongitude();
				bool hasAltidue = exifReader->HasAltitude();
				double altitude = hasAltidue ? exifReader->GetGPSAltitude() : 0.0;
				DisplayLocationInfo(latitude, longitude, hasAltidue, altitude, 
					CNLS::GetString(_T("Location:")), CNLS::GetString(_T("Altitude (m):")));
			}
#endif
		}
		else if (rawMeta != NULL) {
#ifdef SHOW_COMPACT
			_exifDisplay->AddFont(HIGHLIGHT_FONT);

			// display date taken (if missing display last modified time)
			if (rawMeta->HasDateTaken()) {
				_exifDisplay->AddLine(_T(""), rawMeta->GetDateTaken());
			}
			else {
				const FILETIME* fileDateTime = fileList->CurrentModificationTime();
				if (fileDateTime != NULL) {
					_exifDisplay->AddLine(_T(""), *fileDateTime);
				}
			}

			// show main shooting inforamtion in one line: exposure time, aperture, ISO speed, focal length, flash indication
			Rational exposureTimeAsRatio;
			if (rawMeta->HasExposureTime()) {
				double exposureTime = rawMeta->GetExposureTime();
				exposureTimeAsRatio = (exposureTime < 1.0)
					? Rational(1, Helpers::RoundToInt(1.0 / exposureTime)) : Rational(Helpers::RoundToInt(exposureTime), 1);
			}
			CString shutterSpeed = rawMeta->HasExposureTime()
				? CString(_exifDisplay->FormatNumber(exposureTimeAsRatio)) + _T("s") : _T("");
			CString aperture = (rawMeta->HasApertureValue())
				? _T("f/") + CString(_exifDisplay->FormatNumber(rawMeta->GetApertureValue(), 1)) : _T("");
			CString isoSpeed = rawMeta->HasISOSpeed()
				? _T("ISO ") + CString(_exifDisplay->FormatNumber(rawMeta->GetISOSpeed())) : _T("");
			CString focalLength = rawMeta->HasFocalLength()
				? CString(_exifDisplay->FormatNumber(rawMeta->GetFocalLength())) + _T("mm") : _T("");
			CString flashFired = rawMeta->GetFlashFired() ? _T("⚡") : _T("");

			std::vector<CString> parts = { shutterSpeed, aperture, isoSpeed, focalLength, flashFired };
			CString result = JoinWithSeparator(parts, ITEM_SEPARATOR);
			if (!result.IsEmpty()) {
				_exifDisplay->AddLine(_T("%s"), _T(""), result);
			}

			// display image size: width x height and size in Megapixels
			int imgWidth = rawMeta->GetWidth();
			int imgHeight = rawMeta->GetHeight();
			double imgMegaPixels = (double)(imgWidth * imgHeight) / 1000000.0;
			DisplayImageSizeInfo(imgWidth, imgHeight);
			_exifDisplay->AddEmptyLine();

			// show camera name
			CString make = rawMeta->GetMake();
			CString model = rawMeta->GetModel();
			CString camera = GetFullCameraName(make, model);
			_exifDisplay->AddLine(_T("%s"), _T(""), camera);

			// display GPS location if available and also as a URL to open the location in a map application (Google Maps, Bing Maps, etc. depending on user settings)
			if (rawMeta->HasGPSLocation()) {
				GPSCoordinate* latitude = rawMeta->GetGPSLatitude();
				GPSCoordinate* longitude = rawMeta->GetGPSLongitude();
				bool hasAltidue = rawMeta->HasAltitude();
				double altitude = hasAltidue ? rawMeta->GetGPSAltitude() : 0.0;

				DisplayLocationInfo(latitude, longitude, hasAltidue, altitude, _T(""), _T(""), true);
			}
#else
			// display date taken (if missing display last modified time)
			bool hasDateTaken = rawMeta->HasDateTaken();
			SYSTEMTIME dateTaken = rawMeta->GetDateTaken();
			const FILETIME* pFileModifiedTime = fileList->CurrentModificationTime();
			DisplayDateTakenOrLastModifiedTime(hasDateTaken, dateTaken, pFileModifiedTime);

			// display basic exposure information
			if (rawMeta->HasExposureTime()) {
				double exposureTime = rawMeta->GetExposureTime();
				Rational shutterSpeed = (exposureTime < 1.0) ? Rational(1, Helpers::RoundToInt(1.0 / exposureTime)) : Rational(Helpers::RoundToInt(exposureTime), 1);
				_exifDisplay->AddLine(CNLS::GetString(_T("Exposure time (s):")), shutterSpeed);
			}
			if (rawMeta->HasApertureValue()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("F-Number:")), rawMeta->GetApertureValue(), 1);
			}
			if (rawMeta->HasISOSpeed()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("ISO Speed:")), (int)rawMeta->GetISOSpeed());
			}
			if (rawMeta->HasFocalLength()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("Focal length (mm):")), rawMeta->GetFocalLength(), 1);
			}
			double focalLengthEquivalent = EXIFHelpers::CalcFocalLengthEquivalent(rawMeta->GetMake(), rawMeta->GetModel(), rawMeta->GetFocalLength());
			if (focalLengthEquivalent > 0.0) {
				_exifDisplay->AddLine(CNLS::GetString(_T("35mm equiv focal length (mm):")), focalLengthEquivalent, 1);
			}
			if (rawMeta->GetFlashFired()) {
				_exifDisplay->AddLine(CNLS::GetString(_T("Flash fired:")), CNLS::GetString(_T("yes")));
			}

			// display image size
			_exifDisplay->AddLine(CNLS::GetString(_T("Image width:")), rawMeta->GetWidth());
			_exifDisplay->AddLine(CNLS::GetString(_T("Image height:")), rawMeta->GetHeight());

			// display camera name
			CString make = rawMeta->GetMake();
			CString model = rawMeta->GetModel();
			CString camera = GetFullCameraName(make, model);
			_exifDisplay->AddLine(_T("%s"), CNLS::GetString(_T("Camera model:")), camera);

			// display GPS location if available
			if (rawMeta->HasGPSLocation()) {
				GPSCoordinate* latitude = rawMeta->GetGPSLatitude();
				GPSCoordinate* longitude = rawMeta->GetGPSLongitude();
				bool hasAltidue = rawMeta->HasAltitude();
				double altitude = hasAltidue ? rawMeta->GetGPSAltitude() : 0.0;

				DisplayLocationInfo(latitude, longitude, hasAltidue, altitude,
					CNLS::GetString(_T("Location:")), CNLS::GetString(_T("Altitude (m):")));
			}
#endif
		}
		else {
			const FILETIME* fileDateTime = fileList->CurrentModificationTime();
			if (fileDateTime != NULL) {
				_exifDisplay->AddLine(CNLS::GetString(_T("Modification date:")), *fileDateTime);
			}
		}
	}

	if (IsEmpty(comment)) {
		comment = CurrentImage()->GetJPEGComment();
	}

	if (CSettingsProvider::This().ShowJPEGComments() && !IsEmpty(comment)) {
		_exifDisplay->SetComment(comment);
	}
}

bool CEXIFDisplayCtl::OnMouseMove(int x, int y) {
	bool handled = CPanelController::OnMouseMove(x, y);
	bool mouseOver = _exifDisplay->PanelRect().PtInRect(CPoint(x, y));
	_exifDisplay->GetControl<CButtonCtrl*>(CEXIFDisplay::ID_btnClose)->SetShow(mouseOver);
	return handled;
}

void CEXIFDisplayCtl::OnShowHistogram(void* context, int parameter, CButtonCtrl & sender) {
	CEXIFDisplayCtl* pThis = (CEXIFDisplayCtl*)context;
	pThis->_exifDisplay->SetShowHistogram(!pThis->_exifDisplay->GetShowHistogram());
	pThis->_exifDisplay->RequestRepositioning();
	pThis->InvalidateMainDlg();
}

void CEXIFDisplayCtl::OnClose(void* context, int parameter, CButtonCtrl & sender) {
	CEXIFDisplayCtl* pThis = (CEXIFDisplayCtl*)context;
	pThis->m_pMainDlg->ExecuteCommand(IDM_SHOW_FILEINFO);
}

double CEXIFDisplayCtl::GetFocalLenghtEquiv(CEXIFReader* exifReader) {
	if (exifReader->HasFocalLengthEquivalent()) {
		return (double)exifReader->GetFocalLengthEquivalent();
	}
	else if (exifReader->HasFocalLength()) {
		// try to calculate using crop factor
		return exifReader->CalcFocalLengthEquiv(exifReader->GetFocalLength());
	}
	else {
		return 0.0;
	}
}
