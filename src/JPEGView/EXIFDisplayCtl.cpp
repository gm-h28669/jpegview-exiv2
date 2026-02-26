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

static int GetFileNameHeight(HDC dc) {
	CSize size;
	HelpersGUI::SelectDefaultFileNameFont(dc);
	::GetTextExtentPoint32(dc, _T("("), 1, &size);
	return size.cy;
}

static CString CreateGPSString(GPSCoordinate* latitude, GPSCoordinate* longitude) {
	const int BUFF_SIZE = 96;
	TCHAR buff[BUFF_SIZE];
	_stprintf_s(buff, BUFF_SIZE, _T("%.0f° %.0f' %.0f'' %s / %.0f° %.0f' %.0f'' %s"),
		latitude->Degrees, latitude->Minutes, latitude->Seconds, latitude->GetReference(),
		longitude->Degrees, longitude->Minutes, longitude->Seconds, longitude->GetReference());
	return CString(buff);
}

static CString CreateGPSURL(GPSCoordinate* latitude, GPSCoordinate* longitude) {
	double lng = longitude->Degrees + longitude->Minutes / 60 + longitude->Seconds / (60 * 60);
	if (_tcsicmp(longitude->GetReference(), _T("W")) == 0)
		lng = -lng;

	double lat = latitude->Degrees + latitude->Minutes / 60 + latitude->Seconds / (60 * 60);
	if (_tcsicmp(latitude->GetReference(), _T("S")) == 0)
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

CEXIFDisplayCtl::CEXIFDisplayCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) : CPanelController(pMainDlg, false) {
	m_bVisible = CSettingsProvider::This().ShowFileInfo();
	m_nFileNameHeight = 0;
	m_pImageProcPanel = pImageProcPanel;
	m_pPanel = m_pEXIFDisplay = new CEXIFDisplay(pMainDlg->m_hWnd, this);
	m_pEXIFDisplay->GetControl<CButtonCtrl*>(CEXIFDisplay::ID_btnShowHideHistogram)->SetButtonPressedHandler(&OnShowHistogram, this);
	CButtonCtrl* pCloseBtn = m_pEXIFDisplay->GetControl<CButtonCtrl*>(CEXIFDisplay::ID_btnClose);
	pCloseBtn->SetButtonPressedHandler(&OnClose, this);
	pCloseBtn->SetShow(false);
	m_pEXIFDisplay->SetShowHistogram(CSettingsProvider::This().ShowHistogram());
}

CEXIFDisplayCtl::~CEXIFDisplayCtl() {
	delete m_pEXIFDisplay;
	m_pEXIFDisplay = NULL;
}

bool CEXIFDisplayCtl::IsVisible() { 
	return CurrentImage() != NULL && m_bVisible; 
}

void CEXIFDisplayCtl::SetVisible(bool bVisible) {
	if (m_bVisible != bVisible) {
		m_bVisible = bVisible;
		InvalidateMainDlg();
	}
}

void CEXIFDisplayCtl::SetActive(bool bActive) {
	SetVisible(bActive);
}

void CEXIFDisplayCtl::AfterNewImageLoaded() {
	m_pEXIFDisplay->ClearTexts();
	m_pEXIFDisplay->SetHistogram(NULL);
}

void CEXIFDisplayCtl::OnPrePaintMainDlg(HDC hPaintDC) {
	if (m_pMainDlg->IsShowFileName() && m_nFileNameHeight == 0) {
		m_nFileNameHeight = GetFileNameHeight(hPaintDC);
	}
	m_pEXIFDisplay->SetPosition(CPoint(m_pImageProcPanel->PanelRect().left, m_pMainDlg->IsShowFileName() ? m_nFileNameHeight + 6 : 0));
	FillEXIFDataDisplay();
	if (CurrentImage() != NULL && m_pEXIFDisplay->GetShowHistogram()) {
		m_pEXIFDisplay->SetHistogram(CurrentImage()->GetProcessedHistogram());
	}
}

void CEXIFDisplayCtl::FillEXIFDataDisplay() {
	m_pEXIFDisplay->ClearTexts();

	m_pEXIFDisplay->SetHistogram(NULL);

	CString sPrefix, sFileTitle;
	const CFileList* pFileList = m_pMainDlg->GetFileList();

#ifdef SHOW_FILENAME
	// show filename and file index (if option "Show Filename" is enabled this is duplicate info)
	LPCTSTR sCurrentFileName = m_pMainDlg->CurrentFileName(true);

	if (CurrentImage()->IsClipboardImage()) {
		sPrefix = sCurrentFileName;
	} else if (pFileList->Current() != NULL) {
		sPrefix.Format(_T("[%d/%d]"), pFileList->CurrentIndex() + 1, pFileList->Size());
		sFileTitle = sCurrentFileName;
		sFileTitle += Helpers::GetMultiframeIndex(m_pMainDlg->GetCurrentImage());
	}
#endif

	LPCTSTR sComment = NULL;
	m_pEXIFDisplay->AddPrefix(sPrefix);
	m_pEXIFDisplay->AddTitle(sFileTitle);

#ifndef SHOW_COMPACT
	m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Image width:")), CurrentImage()->OrigWidth());
	m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Image height:")), CurrentImage()->OrigHeight());
#endif

	if (!CurrentImage()->IsClipboardImage()) {
		CEXIFReader* pEXIFReader = CurrentImage()->GetEXIFReader();
		CRawMetadata* pRawMetaData = CurrentImage()->GetRawMetadata();
		if (pEXIFReader != NULL) {
			sComment = pEXIFReader->GetUserComment();
			if (sComment == NULL || sComment[0] == 0 || ((std::wstring)sComment).find_first_not_of(L" \t\n\r\f\v", 0) == std::wstring::npos) {
				sComment = pEXIFReader->GetImageDescription();
			}

#ifdef SHOW_COMPACT
			// display date taken (if missing display last modified time)
			if (pEXIFReader->GetAcquisitionTimePresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("")), pEXIFReader->GetAcquisitionTime());
			}
			else {
				const FILETIME* pFileTime = pFileList->CurrentModificationTime();
				if (pFileTime != NULL) {
					m_pEXIFDisplay->AddLine(CNLS::GetString(_T("")), *pFileTime);
				}
			}

			// show exposure time, aperture, ISO speed, exposure bias, focal length, 35mm focal length equivalent and 
			// flash indication in one line
			CString sExposureTime = pEXIFReader->GetExposureTimePresent()
				? m_pEXIFDisplay->FormatNumber(pEXIFReader->GetExposureTime()) : _T("");

			CString sFNumber = (pEXIFReader->GetFNumberPresent())
				? m_pEXIFDisplay->FormatNumber(pEXIFReader->GetFNumber(), 1) : _T("");

			CString sISOSpeed = pEXIFReader->GetISOSpeedPresent()
				? m_pEXIFDisplay->FormatNumber(pEXIFReader->GetISOSpeed()) : _T("");

			CString sFocalLength = pEXIFReader->GetFocalLengthPresent()
				? m_pEXIFDisplay->FormatNumber(pEXIFReader->GetFocalLength()) : _T("");

			double focalLengthEquiv = GetFocalLenghtEquiv(pEXIFReader);
			CString sFocalLengthEquiv = focalLengthEquiv > 0.0
				? _T("➞") + CString(m_pEXIFDisplay->FormatNumber(focalLengthEquiv)) : _T("");

			double exposureBias = pEXIFReader->GetExposureBiasPresent()
				? pEXIFReader->GetExposureBias() : 0.0;
			CString sExposureBias = exposureBias != 0.0
				? _T(" EV") + CString(m_pEXIFDisplay->FormatNumber(exposureBias, 1, true)) + _T(" •") : _T("");

			CString sFlashFired = pEXIFReader->GetFlashFiredPresent() && pEXIFReader->GetFlashFired() ? _T("⚡") : _T("");

			CString cameraInfoFormat = _T("%ss • f/%s • ISO %s •%s %s%smm %s");
			m_pEXIFDisplay->AddLine(cameraInfoFormat, _T(""), sExposureTime, sFNumber, sISOSpeed, sExposureBias, sFocalLength, sFocalLengthEquiv, sFlashFired);

			// display camera make and model
			if (pEXIFReader->GetCameraModelPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("")), pEXIFReader->GetCameraModel());
			}

			// display lens info 
			CString sLensInfo = pEXIFReader->GetLensInfo();
			if (!sLensInfo.IsEmpty()) {
				m_pEXIFDisplay->AddLine(_T(""), sLensInfo);
			}

			// display image size: widthx height and size in Megapixels
			int width = CurrentImage()->OrigWidth();
			int height = CurrentImage()->OrigHeight();
			double megaPixels = (double)(width * height) / 1000000.0;

			TCHAR* sWidth; TCHAR* sHeight; TCHAR* sMegaPixels;
			sWidth = m_pEXIFDisplay->FormatNumber(width);
			sHeight = m_pEXIFDisplay->FormatNumber(height);
			sMegaPixels = m_pEXIFDisplay->FormatNumber(megaPixels, 1);
			m_pEXIFDisplay->AddLine(_T("%sMP • %sx%s"), CNLS::GetString(_T("")), sMegaPixels, sWidth, sHeight);

			if (pEXIFReader->IsGPSInformationPresent()) {
				m_pEXIFDisplay->AddEmptyLine();
				CString sGPSLocation = CreateGPSString(pEXIFReader->GetGPSLatitude(), pEXIFReader->GetGPSLongitude());
				m_pEXIFDisplay->SetGPSLocation(sGPSLocation, CreateGPSURL(pEXIFReader->GetGPSLatitude(), pEXIFReader->GetGPSLongitude()));
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("")), sGPSLocation, true);
				if (pEXIFReader->IsGPSAltitudePresent()) {
					CString sAltitude = m_pEXIFDisplay->FormatNumber(pEXIFReader->GetGPSAltitude(), 0);
					m_pEXIFDisplay->AddLine(_T("Alt: %sm"), CNLS::GetString(_T("")), sAltitude);
				}
			}
#else
			if (pEXIFReader->GetCameraModelPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Camera model:")), pEXIFReader->GetCameraModel());
			}
			if (pEXIFReader->GetExposureTimePresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Exposure time (s):")), pEXIFReader->GetExposureTime());
			}
			if (pEXIFReader->GetExposureBiasPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Exposure bias (EV):")), pEXIFReader->GetExposureBias(), 2);
			}
			if (pEXIFReader->GetFlashFiredPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Flash fired:")), pEXIFReader->GetFlashFired() ? CNLS::GetString(_T("yes")) : CNLS::GetString(_T("no")));
			}
			if (pEXIFReader->GetFocalLengthPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Focal length (mm):")), pEXIFReader->GetFocalLength(), 1);
			}
			if (pEXIFReader->GetFNumberPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("F-Number:")), pEXIFReader->GetFNumber(), 1);
			}
			if (pEXIFReader->GetISOSpeedPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("ISO Speed:")), (int)pEXIFReader->GetISOSpeed());
			}
			if (pEXIFReader->GetSoftwarePresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Software:")), pEXIFReader->GetSoftware());
			}

			if (pEXIFReader->IsGPSInformationPresent()) {
				CString sGPSLocation = CreateGPSString(pEXIFReader->GetGPSLatitude(), pEXIFReader->GetGPSLongitude());
				m_pEXIFDisplay->SetGPSLocation(sGPSLocation, CreateGPSURL(pEXIFReader->GetGPSLatitude(), pEXIFReader->GetGPSLongitude()));
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Location:")), sGPSLocation, true);
				if (pEXIFReader->IsGPSAltitudePresent()) {
					m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Altitude (m):")), pEXIFReader->GetGPSAltitude(), 0);
				}
			}
#endif
		}
		else if (pRawMetaData != NULL) {
#ifdef SHOW_COMPACT
			// show exposure time, aperture, ISO speed, focal length, and flash indication in one line
			double exposureTime = pRawMetaData->GetExposureTime();
			Rational rational = (exposureTime < 1.0) 
				? Rational(1, Helpers::RoundToInt(1.0 / exposureTime)) : Rational(Helpers::RoundToInt(exposureTime), 1);
			CString sExposureTime = pRawMetaData->GetExposureTime() > 0.0
				? m_pEXIFDisplay->FormatNumber(rational) : _T("");

			CString sFNumber = pRawMetaData->GetAperture() > 0.0
				? m_pEXIFDisplay->FormatNumber(pRawMetaData->GetAperture(), 1) : _T("");

			CString sISOSpeed = pRawMetaData->GetIsoSpeed() > 0.0
				? m_pEXIFDisplay->FormatNumber(pRawMetaData->GetIsoSpeed()) : _T("");

			CString sFocalLength = pRawMetaData->GetFocalLength() > 0.0
				? m_pEXIFDisplay->FormatNumber(pRawMetaData->GetFocalLength(), 1) : _T("");

			CString sFlashFired = pRawMetaData->IsFlashFired() ? _T("⚡") : _T("");

			CString cameraInfoFormat = _T("%ss • f/%s • ISO %s • %smm %s");
			m_pEXIFDisplay->AddLine(cameraInfoFormat, _T(""), sExposureTime, sFNumber, sISOSpeed, sFocalLength, sFlashFired);

			if (pRawMetaData->GetManufacturer()[0] != 0) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("")), CString(pRawMetaData->GetManufacturer()) + _T(" ") + pRawMetaData->GetModel());
			}

			// display date taken (if missing display last modified time)
			if (pRawMetaData->GetAcquisitionTime().wYear > 1985) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("")), pRawMetaData->GetAcquisitionTime());
			}
			else {
				const FILETIME* pFileTime = pFileList->CurrentModificationTime();
				if (pFileTime != NULL) {
					m_pEXIFDisplay->AddLine(CNLS::GetString(_T("")), *pFileTime);
				}
			}

			// display image size: widthx height and size in Megapixels
			int width = pRawMetaData->GetWidth();
			int height = pRawMetaData->GetHeight();
			double megaPixels = (double)(width * height) / 1000000.0;

			TCHAR* sWidth; TCHAR* sHeight; TCHAR* sMegaPixels;
			sWidth = m_pEXIFDisplay->FormatNumber(width);
			sHeight = m_pEXIFDisplay->FormatNumber(height);
			sMegaPixels = m_pEXIFDisplay->FormatNumber(megaPixels, 1);
			m_pEXIFDisplay->AddLine(_T("%sMP • %sx%s"), CNLS::GetString(_T("")), sMegaPixels, sWidth, sHeight);

			if (pRawMetaData->IsGPSInformationPresent()) {
				m_pEXIFDisplay->AddEmptyLine();
				CString sGPSLocation = CreateGPSString(pRawMetaData->GetGPSLatitude(), pRawMetaData->GetGPSLongitude());
				m_pEXIFDisplay->SetGPSLocation(sGPSLocation, CreateGPSURL(pRawMetaData->GetGPSLatitude(), pRawMetaData->GetGPSLongitude()));
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("")), sGPSLocation, true);
				if (pRawMetaData->IsGPSAltitudePresent()) {
					CString sAltitude = m_pEXIFDisplay->FormatNumber(pRawMetaData->GetGPSAltitude(), 0);
					m_pEXIFDisplay->AddLine(_T("Alt: %sm"), CNLS::GetString(_T("")), sAltitude);
				}
			}
#else
			if (pRawMetaData->GetAcquisitionTime().wYear > 1985) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Acquisition date:")), pRawMetaData->GetAcquisitionTime());
			}
			else {
				const FILETIME* pFileTime = pFileList->CurrentModificationTime();
				if (pFileTime != NULL) {
					m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Modification date:")), *pFileTime);
				}
			}
			if (pRawMetaData->IsGPSInformationPresent()) {
				CString sGPSLocation = CreateGPSString(pRawMetaData->GetGPSLatitude(), pRawMetaData->GetGPSLongitude());
				m_pEXIFDisplay->SetGPSLocation(sGPSLocation, CreateGPSURL(pRawMetaData->GetGPSLatitude(), pRawMetaData->GetGPSLongitude()));
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Location:")), sGPSLocation, true);
				if (pRawMetaData->IsGPSAltitudePresent()) {
					m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Altitude (m):")), pRawMetaData->GetGPSAltitude(), 0);
				}
			}
			if (pRawMetaData->GetManufacturer()[0] != 0) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Camera model:")), CString(pRawMetaData->GetManufacturer()) + _T(" ") + pRawMetaData->GetModel());
			}
			if (pRawMetaData->GetExposureTime() > 0.0) {
				double exposureTime = pRawMetaData->GetExposureTime();
				Rational rational = (exposureTime < 1.0) ? Rational(1, Helpers::RoundToInt(1.0 / exposureTime)) : Rational(Helpers::RoundToInt(exposureTime), 1);
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Exposure time (s):")), rational);
			}
			if (pRawMetaData->IsFlashFired()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Flash fired:")), CNLS::GetString(_T("yes")));
			}
			if (pRawMetaData->GetFocalLength() > 0.0) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Focal length (mm):")), pRawMetaData->GetFocalLength(), 1);
			}
			if (pRawMetaData->GetAperture() > 0.0) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("F-Number:")), pRawMetaData->GetAperture(), 1);
			}
			if (pRawMetaData->GetIsoSpeed() > 0.0) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("ISO Speed:")), (int)pRawMetaData->GetIsoSpeed());
			}
#endif
		}
		else {
			const FILETIME* pFileTime = pFileList->CurrentModificationTime();
			if (pFileTime != NULL) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Modification date:")), *pFileTime);
			}
		}
	}

	if (sComment == NULL || sComment[0] == 0 || ((std::wstring)sComment).find_first_not_of(L" \t\n\r\f\v", 0) == std::wstring::npos) {
		sComment = CurrentImage()->GetJPEGComment();
	}
	if (CSettingsProvider::This().ShowJPEGComments() && sComment != NULL && sComment[0] != 0) {
		m_pEXIFDisplay->SetComment(sComment);
	}
}

bool CEXIFDisplayCtl::OnMouseMove(int nX, int nY) {
	bool bHandled = CPanelController::OnMouseMove(nX, nY);
	bool bMouseOver = m_pEXIFDisplay->PanelRect().PtInRect(CPoint(nX, nY));
	m_pEXIFDisplay->GetControl<CButtonCtrl*>(CEXIFDisplay::ID_btnClose)->SetShow(bMouseOver);
	return bHandled;
}

void CEXIFDisplayCtl::OnShowHistogram(void* pContext, int nParameter, CButtonCtrl & sender) {
	CEXIFDisplayCtl* pThis = (CEXIFDisplayCtl*)pContext;
	pThis->m_pEXIFDisplay->SetShowHistogram(!pThis->m_pEXIFDisplay->GetShowHistogram());
	pThis->m_pEXIFDisplay->RequestRepositioning();
	pThis->InvalidateMainDlg();
}

void CEXIFDisplayCtl::OnClose(void* pContext, int nParameter, CButtonCtrl & sender) {
	CEXIFDisplayCtl* pThis = (CEXIFDisplayCtl*)pContext;
	pThis->m_pMainDlg->ExecuteCommand(IDM_SHOW_FILEINFO);
}

double CEXIFDisplayCtl::GetFocalLenghtEquiv(CEXIFReader* pEXIFReader) {
	if (pEXIFReader->GetFocalLengthEquivalentPresent()) {
		return (double)pEXIFReader->GetFocalLengthEquivalent();
	}
	else if (pEXIFReader->GetFocalLengthPresent()) {
		// try to calculate using crop factor
		return pEXIFReader->CalcFocalLengthEquiv(pEXIFReader->GetFocalLength());
	}
	else {
		return 0.0;
	}
}

