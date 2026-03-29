#pragma once
#include <string>
#include <atlstr.h>

namespace HelpersICM {
	CString GetIccProfilePath(HDC hdc);
	void ShowIcmInfo(HDC hdc, const CString msg);
	CString GetCurrentMonitorDeviceName(HWND hWnd);
}