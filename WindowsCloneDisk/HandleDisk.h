//
//Header, containing some help functions to
// work with physical drives and the definition of
// DISKINFO structure.
//

#pragma once
#include "framework.h"

typedef struct _DISKSIZEINFO
{
	LARGE_INTEGER Size; //The total capacity of the drive
	DWORD SectorSize;   //Required for correct pointer alignment when reading from and writing to raw file.
} DISKSIZEINFO, *PDISKSIZEINFO;
typedef const DISKSIZEINFO* PCDISKSIZEINFO;

typedef struct _CBDISKENTRY
{
	DWORD DiskNumber;
	WCHAR Caption[64];
} CBDISKENTRY, *PCBDISKENTRY;
typedef const CBDISKENTRY* PCCBDISKENTRY;

/// <summary>
///		Tries to allocate memory on the process's heap and fill it with CBDISKENTRY 
///		structures containing the index and the caption of each
///		physical drive installed on the machine.
/// </summary>
/// <param name="pPtr">
///		The address of the pointer to the array which will receive this information.
///		The pointer must be previously allocated on the process heap or NULL.
/// </param>
/// <param name="count">
///		The address of the variable that will receive the count of the available drives
/// </param>
/// <returns>
///		TRUE on success, otherwise FALSE
/// </returns>
/// <comment>
///		If the function fails, the values in *ptr and *count remain unchanged. 
///		On success, the previous buffer at **ptr will be freed.
/// </comment>
BOOL  GetLocalDiskCBEntries(_Inout_ PCBDISKENTRY* pPtr, _Inout_ DWORD* count);

/// <summary>
///		Fills infoArrCount structures with the caption and the index of local machine's drives.
///		Call twice:
///			firstly, to get the count of all the local drives so you can allocate enough
///			memory to recieve information about all drives.
/// </summary>
/// <param name="infoArr">
///		The target pointer to array
/// </param>
/// <param name="infoArrCount">
///		The count of the elements in the array. If this parameter is set to 0, then
///		infoArr isn't used
/// </param>
/// <returns>
///		The count of the local physical drives or 0 on failure
/// </returns>
DWORD FillComboBoxEntries(
	_Out_writes_bytes_opt_(infoArrCount * sizeof(CBDISKENTRY)) PCBDISKENTRY infoArr,
	_In_ DWORD infoArrCount
);


/// <summary>
///		Clears the elements in the ComboBox's list and then adds
///		count formatted elements with the contents of the COMBOBOXDISKENTRY in the following
///		format: "Disk infos[i].DiskNumber - infos[i].Caption".
/// </summary>
/// <param name="hCB">
///		The target ComboBox
/// </param>
/// <returns>
///		TRUE on success, otherwise FALSE
/// </returns>
BOOL CBSetListToCBDISKENTRYList(_In_ HWND hCB, _In_ PCCBDISKENTRY infos, _In_ DWORD count);


/// <summary>
///		Gets the size of the physical sector of the specified device, in bytes.
/// </summary>
/// <param name="hDevice">
///		Handle to the target device
/// </param>
/// <returns>
///		The size of the sector, or 0 on failure
/// </returns>
DWORD GetPhysicalSectorSize(_In_ HANDLE hDevice);

/// <summary>
///		Creates a file handle to the physical drive with the given number 
///		(\\.\PhysicalDrive[diskNumber]) and sets the file pointer to the beginning
///		of the drive. 
/// </summary>
/// <param name="diskNumber">
///		The index of the disk to open.
/// </param>
/// <param name="dwDesiredAccess">
///		The desired access to the disk:
///		GENERIC_READ, GENERIC_WRITE, or both.
/// </param>
/// <returns>
///		A handle to the open disk or INVALID_HANDLE_VALUE on failure.
/// </returns>
HANDLE OpenDisk(_In_ DWORD diskNumber, _In_ DWORD dwDesiredAccess);

/// <summary>
///		Prepares the disk to be overwritten.
///		A successfull call to this function puts the disk in
///		offline state and deletes its partition table so no partition
///		info will be saved on the disk.
/// </summary>
/// <param name="hDisk">
///		A valid handle to the target disk.
/// </param>
/// <returns>
///		TRUE on success, otherwise FALSE.
/// </returns>
BOOL PrepareDiskForWrite(_In_ HANDLE hDisk);

/// <summary>
///		Prepares the disk to be used.
///		A successfull call to this function puts the
///		disk in online state.
/// </summary>
/// <param name="hDisk">
///		A valid handle to the target disk.
/// </param>
/// <returns>
///		TRUE on success, otherwise FALSE.
/// </returns>
BOOL PrepareDiskForUse(_In_ HANDLE hDisk);

/// <summary>
///		Gets the sector size and the capacity of a disk.
/// </summary>
/// <param name="hDisk">
///		The target disk.
/// </param>
/// <param name="pInfo">
///		A pointer to the structure that will recieve the disk information.
/// </param>
/// <returns>
///		TRUE on success, otherwise FALSE.
/// </returns>
BOOL GetDiskSizeInfo(_In_ HANDLE hDisk, _Out_ PDISKSIZEINFO pInfo);

/// <summary>
///		Gets the sector size and the capacity of a disk.
/// </summary>
/// <param name="hDisk">
///		The index of the target disk.
/// </param>
/// <param name="pInfo">
///		A pointer to the structure that will recieve the disk information.
/// </param>
/// <returns>
///		TRUE on success, otherwise FALSE.
/// </returns>
BOOL GetDiskSizeInfoByIndex(_In_ DWORD diskNumber, _Out_ PDISKSIZEINFO pInfo);