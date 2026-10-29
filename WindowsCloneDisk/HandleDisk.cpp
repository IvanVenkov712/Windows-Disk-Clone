#include "HandleDisk.h"

BOOL GetLocalDiskCBEntries(_Inout_ PCBDISKENTRY* pPtr, _Inout_ DWORD* count)
{
	PCBDISKENTRY oldPtr = *pPtr;
	DWORD oldCount = *count;

	*count = FillComboBoxEntries(nullptr, 0);
	if (*count == 0)
	{
		*count = oldCount;
		return FALSE;
	}
	*pPtr = (PCBDISKENTRY)HeapAlloc(GetProcessHeap(), 0,
		*count * sizeof(CBDISKENTRY));

	if (*pPtr == nullptr)
	{
		*count = oldCount;
		return FALSE;
	}
	if (FillComboBoxEntries(*pPtr, *count) == 0)
	{
		*count = oldCount;
		HeapFree(GetProcessHeap(), 0, *pPtr);
		*pPtr = oldPtr;
		return FALSE;
	}
	HeapFree(GetProcessHeap(), 0, oldPtr);
	return TRUE;
}

DWORD FillComboBoxEntries(
	_Out_writes_bytes_opt_(infoArrCount * sizeof(CBDISKENTRY)) PCBDISKENTRY infoArr, 
	_In_ DWORD infoArrCount
)
{
	DWORD resCount = 0;
	HRESULT hr;
	IWbemLocator* pLoc = nullptr;
	hr = CoCreateInstance(
		CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pLoc)
	);
	if (SUCCEEDED(hr))
	{
		IWbemServices* pSvc = nullptr;
		hr = pLoc->ConnectServer(
			bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr,
			nullptr, 0, nullptr, nullptr, &pSvc
		);
		if (SUCCEEDED(hr))
		{
			hr = CoSetProxyBlanket(
				pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
				nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
				nullptr, EOAC_NONE
			);
			IEnumWbemClassObject* pEnumerator = nullptr;
			hr = pSvc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT * FROM Win32_DiskDrive"),
				WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator
			);

			if (SUCCEEDED(hr))
			{
				IWbemClassObject* pclsObj = nullptr;
				ULONG uReturned = 0;

				while (true)
				{
					hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturned);
					if (uReturned == 0) break;
					pclsObj->Release();
					resCount++;
				}
				IEnumWbemClassObject* pValEnumerator = nullptr;
				hr = pEnumerator->Reset();
				if (SUCCEEDED(hr))
				{
					VARIANT vtProp{};
					for (DWORD n = 0; n < infoArrCount; n++)
					{
						hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturned);
						if (uReturned == 0) break;
						VariantInit(&vtProp);

						pclsObj->Get(L"Index", 0, &vtProp, nullptr, nullptr);
						infoArr[n].DiskNumber = vtProp.uiVal;
						VariantClear(&vtProp);

						pclsObj->Get(L"Caption", 0, &vtProp, nullptr, nullptr);
						wcscpy_s(infoArr[n].Caption, vtProp.bstrVal);
						VariantClear(&vtProp);

						pclsObj->Release();
					}
				}
			}
			pSvc->Release();
		}
		pLoc->Release();
	}
	if (FAILED(hr))
	{
		ZeroMemory(infoArr, infoArrCount * sizeof(CBDISKENTRY));
		return 0;
	}
	return resCount;
}

BOOL CBSetListToCBDISKENTRYList(_In_ HWND hCB, _In_ PCCBDISKENTRY infos, _In_ DWORD count)
{
	SendMessage(hCB, CB_RESETCONTENT, 0, 0);
	WCHAR buff[64]{};
	for (DWORD i = 0; i < count; i++)
	{
		wnsprintfW(buff, ARRAYSIZE(buff), L"Disk %lu - %s", infos[i].DiskNumber, infos[i].Caption);
		SendMessageW(hCB, CB_ADDSTRING, 0, (LPARAM)(buff));
	}
	return TRUE;
}

DWORD GetPhysicalSectorSize(_In_ HANDLE hDevice)
{
	DISK_GEOMETRY geo{};
	DWORD outSize;
	BOOL success = DeviceIoControl(
		hDevice,
		IOCTL_DISK_GET_DRIVE_GEOMETRY,
		nullptr, 0,
		&geo, sizeof(DISK_GEOMETRY),
		&outSize, nullptr
	);
	if (success) return geo.BytesPerSector;
	else return 0;
}

HANDLE OpenDisk(_In_ DWORD diskNumber, _In_ DWORD dwDesiredAccess)
{
	WCHAR path[20];
	wnsprintfW(path, 20, L"\\\\.\\PhysicalDrive%lu", diskNumber);
	HANDLE hDisk = CreateFileW(
		path, dwDesiredAccess,
		FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
		FILE_FLAG_NO_BUFFERING, nullptr
	);
	if (hDisk == INVALID_HANDLE_VALUE)
	{
		return INVALID_HANDLE_VALUE;
	}
	if(SetFilePointer(hDisk, 0, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		CloseHandle(hDisk);
		return INVALID_HANDLE_VALUE;
	}
	return hDisk;
}

BOOL PrepareDiskForWrite(_In_ HANDLE hDisk)
{
	BOOL isOK = TRUE;
	SET_DISK_ATTRIBUTES attribs{};
	attribs.Version = sizeof(SET_DISK_ATTRIBUTES);
	attribs.Persist = TRUE;
	attribs.AttributesMask = DISK_ATTRIBUTE_OFFLINE;
	attribs.Attributes = DISK_ATTRIBUTE_OFFLINE;

	BOOL offline = DeviceIoControl(
		hDisk, IOCTL_DISK_SET_DISK_ATTRIBUTES,
		&attribs, sizeof(SET_DISK_ATTRIBUTES),
		nullptr, 0, nullptr, nullptr
	);
	if ((isOK = offline))
	{
		BOOL emptied = DeviceIoControl(
			hDisk,
			IOCTL_DISK_DELETE_DRIVE_LAYOUT,
			nullptr, 0, nullptr, 0, nullptr, nullptr
		);
		isOK = emptied;
		if (!isOK)
		{
			attribs.Attributes = 0;
			DeviceIoControl(
				hDisk, IOCTL_DISK_SET_DISK_ATTRIBUTES,
				&attribs, sizeof(SET_DISK_ATTRIBUTES),
				nullptr, 0, nullptr, nullptr
			);
		}
	}
	return isOK;
}

BOOL PrepareDiskForUse(_In_ HANDLE hDisk)
{
	SET_DISK_ATTRIBUTES attribs{};
	attribs.Version = sizeof(SET_DISK_ATTRIBUTES);
	attribs.Persist = TRUE;
	attribs.AttributesMask = DISK_ATTRIBUTE_OFFLINE;
	attribs.Attributes = 0;

	return DeviceIoControl(
		hDisk, IOCTL_DISK_SET_DISK_ATTRIBUTES,
		&attribs, sizeof(SET_DISK_ATTRIBUTES),
		nullptr, 0, nullptr, nullptr
	);
}

BOOL GetDiskSizeInfo(_In_ HANDLE hDisk, _Out_ PDISKSIZEINFO pInfo)
{
	DISK_GEOMETRY_EX geoEx{};
	DWORD bytesReturned = 0;
	BOOL success = DeviceIoControl(
		hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
		nullptr, 0, &geoEx, sizeof(DISK_GEOMETRY_EX),
		&bytesReturned, nullptr
	);
	if (success)
	{
		pInfo->Size = geoEx.DiskSize;
		pInfo->SectorSize = geoEx.Geometry.BytesPerSector;
	}
	else
	{
		ZeroMemory(pInfo, sizeof(DISKSIZEINFO));
	}
	return success;
}

BOOL GetDiskSizeInfoByIndex(_In_ DWORD diskNumber, _Out_ PDISKSIZEINFO pInfo)
{
	*pInfo = {};
	HANDLE hDisk = OpenDisk(diskNumber, GENERIC_READ);
	if (hDisk == INVALID_HANDLE_VALUE)
	{
		ZeroMemory(pInfo, sizeof(DISKSIZEINFO));
		return FALSE;
	}
	BOOL result = GetDiskSizeInfo(hDisk, pInfo);
	CloseHandle(hDisk);
	return result;
}