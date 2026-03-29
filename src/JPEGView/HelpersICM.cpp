#include "stdafx.h"
#include "HelpersICM.h"
#include "Logging.h"
#include <string>
#include <vector>

namespace HelpersICM {
	CString GetIccProfilePath(HDC hdc) {
		DWORD len = 0;

		// get ICC profile for display
		GetICMProfile(hdc, &len, NULL);	// call first time to get length of path
		std::vector<wchar_t> path(len + 1);
		CString profilePath;
		if (GetICMProfile(hdc, &len, path.data())) {		// call second time to get device path
			profilePath = path.data();
		}
		return profilePath;
	}

	void ShowIcmInfo(HDC hdc, const CString msg) {
#ifdef DEBUG
		int status = SetICMMode(hdc, ICM_QUERY);
		CString icmMode = (status == ICM_OFF) ? TEXT("off") : TEXT("on");
		CString icmProfilePath = GetIccProfilePath(hdc);
		LOG_INFO_WIDE(msg + TEXT(": Color management: ") + icmMode + TEXT(". Profile: ") + icmProfilePath);
#endif
	}

	CString GetCurrentMonitorDeviceName(HWND hWnd) {
		HMONITOR currentMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFOEX activeMonitorInfo;
		activeMonitorInfo.cbSize = sizeof(MONITORINFOEX);
		GetMonitorInfo(currentMonitor, (LPMONITORINFOEX)&activeMonitorInfo);

		CString deviceName = activeMonitorInfo.szDevice;
		LOG_INFO_WIDE(TEXT("GetMonitorInfo Name=") + deviceName);
		return deviceName;
	}
}