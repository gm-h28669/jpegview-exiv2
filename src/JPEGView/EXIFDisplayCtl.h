#pragma once

#include "PanelController.h"
#include "EXIFReader.h"

class CEXIFDisplay;

// Implements functionality of the EXIF display panel
class CEXIFDisplayCtl : public CPanelController
{
public:
	CEXIFDisplayCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel);
	virtual ~CEXIFDisplayCtl();

	virtual float DimFactor() { return 0.5f; }

	virtual bool IsVisible();
	virtual bool IsActive() { return _visible; }

	virtual void SetVisible(bool bVisible);
	virtual void SetActive(bool bActive);

	virtual void AfterNewImageLoaded();

	virtual bool OnMouseMove(int nX, int nY);
	virtual void OnPrePaintMainDlg(HDC hPaintDC);

private:
	bool _visible;
	CEXIFDisplay* _exifDisplay;
	CPanel* _imageProcessingPanel;
	int _fileNameHeight;

	void DisplayImageSizeInfo(int width, int height);
	void DisplayDateTakenOrLastModifiedTime(bool hasDateTaken, SYSTEMTIME dateTaken, const FILETIME* fileModifiedTime);
	void DisplayLocationInfo(GPSCoordinate* latitude, GPSCoordinate* longitude, bool hasAltitude, double altitude, 
		LPCTSTR descriptionLocation, LPCTSTR descricptionAltitudebool, bool useBoldFont = false);
	void FillEXIFDataDisplay();
	static void OnShowHistogram(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnClose(void* pContext, int nParameter, CButtonCtrl & sender);
	static double GetFocalLenghtEquiv(CEXIFReader* pEXIFReader);
};