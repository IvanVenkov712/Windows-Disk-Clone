// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "targetver.h"
// Windows Header Files
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#define STRICT_TYPED_ITEMIDS
#include <shlobj.h>
#include <objbase.h>      // For COM headers
#include <shobjidl.h>     // for IFileDialogEvents and IFileDialogControlEvents
#include <shlwapi.h>
#include <knownfolders.h> // for KnownFolder APIs/datatypes/function headers
#include <propvarutil.h>  // for PROPVAR-related functions
#include <propkey.h>      // for the Property key APIs/datatypes
#include <propidl.h>      // for the Property System APIs
#include <shtypes.h>      // for COMDLG_FILTERSPEC
#include <WbemCli.h>
#include <comutil.h>
#include <string.h>
#include <stdlib.h>