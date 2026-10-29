# Current problems

These findings come from static inspection of commit `c67078d`. They are source-level defects or missing capabilities, not reproduced runtime incidents. No build or destructive disk test was performed for this documentation. Priorities below are suggested repair order: **P1** covers operation-blocking defects, data integrity, and unsafe lifecycle behavior; **P2** covers robustness and maintainability.

## P1: Disk preparation violates the synchronous API contract

**Location:** [HandleDisk.cpp](../WindowsCloneDisk/HandleDisk.cpp), `PrepareDiskForWrite` and `PrepareDiskForUse`.

The disk-attribute and layout-deletion calls pass `nullptr` for both `lpBytesReturned` and `lpOverlapped`. Microsoft's [DeviceIoControl contract](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol) requires a non-null `lpBytesReturned` for synchronous calls, including operations without output data. These calls violate that contract and can prevent preparation or restoration from working; exact runtime behavior has not been verified.

**Repair:** pass a valid local `DWORD` to each synchronous call, check each result, and preserve the error immediately. Treat failure to restore the target's state as a separate reported failure.

## P1: A smaller target produces a truncated disk image

**Location:** [WindowsCloneDisk.cpp](../WindowsCloneDisk/WindowsCloneDisk.cpp), clone command and `CopyFileThreadProc`.

The interface permits a smaller destination after a warning. The worker copies `min(source capacity, target capacity)` bytes without changing partition boundaries or filesystem metadata. Partitions can refer beyond the target's end, and required data or backup metadata can be omitted. This is not filesystem-aware migration.

**Repair:** reject smaller targets before any modification. If resizing is ever added, implement it as a distinct, filesystem-aware operation with its own validation.

## P1: Copy outcome and progress are unreliable

**Location:** [WindowsCloneDisk.cpp](../WindowsCloneDisk/WindowsCloneDisk.cpp), `CopyFileThreadProc` and `ShowErrorMessage`.

- A failed `ReadFile` exits the loop without setting `isOK` to false. There is no final assertion that `writtenTotal == copySize`.
- The Boolean result of `WriteFile` is ignored; only the byte count is compared.
- Final status relies on `GetLastError()` after subsequent UI, disk-restoration, and handle-cleanup calls. That value is not a durable operation result and may be stale or replaced.
- `PrepareDiskForUse` is unchecked. The operation can report success even when restoration fails.
- The progress bar and text are set to 100% on every exit path, including failures.
- If system message formatting fails, there is no fallback error dialog containing the numeric error and failing operation.

**Repair:** record a structured result at the failure site, explicitly check every read/write and the final byte total, preserve restoration errors separately, and show success/100% only after all required completion steps pass.

## P1: Source contents can change during the copy

**Location:** [HandleDisk.cpp](../WindowsCloneDisk/HandleDisk.cpp), `OpenDisk`; [WindowsCloneDisk.cpp](../WindowsCloneDisk/WindowsCloneDisk.cpp), `CopyFileThreadProc`.

The source is opened with shared read/write access and remains online. There is no source-volume lock, dismount, snapshot, or enforced offline-source requirement. Copying a mounted disk can combine data and metadata from different moments, producing an inconsistent filesystem even if every I/O call succeeds. The code also does not explicitly identify or exclude the running system disk as a target.

**Repair:** define and enforce an initial scope such as offline, disposable source disks; identify protected/system targets before writing. Supporting live sources requires an explicit consistency design rather than just raw reads.

## P1: Destructive preparation happens before full worker validation

**Location:** [WindowsCloneDisk.cpp](../WindowsCloneDisk/WindowsCloneDisk.cpp), `CopyFileThreadProc`; [HandleDisk.cpp](../WindowsCloneDisk/HandleDisk.cpp), preparation helpers.

The worker offlines the target and deletes its layout before querying both geometries and allocating its buffer. Failure during those later steps can leave a target changed without any successful copy. The previous offline state is not saved, and restoration simply attempts to clear it. Cleanup does not restore deleted layout metadata or overwritten bytes.

The UI's confirmation does not name the selected target, and disk selection relies on an earlier WMI list plus disk numbers. There is no stable-identity check against the device opened by the worker after hot-plug changes. The action button's label, **Clone Disk to Source**, also contradicts the actual source-to-target direction.

**Repair:** open and revalidate device identities, capacity, sector compatibility, and all resources before modifying the target. Show source and target details in the final confirmation, correct the button label, and explicitly track target state and partial-copy outcomes.

## P1: Worker lifetime is not coordinated with the window

**Location:** [WindowsCloneDisk.cpp](../WindowsCloneDisk/WindowsCloneDisk.cpp), clone command, `WM_DESTROY`, and `CopyFileThreadProc`.

`CreateThread` failure is tested against `INVALID_HANDLE_VALUE`, although this API returns `NULL` on failure. A failed launch can therefore be treated as successful and leak its parameter block. A successful thread handle is never retained or closed.

The action button is disabled only after the worker begins, leaving a launch race. Refresh, selection changes, and exit remain enabled. Closing the main window posts quit without waiting for the copy or restoring the target; process exit can interrupt writes. The worker uses synchronous calls to UI controls without a defined window-lifetime protocol.

**Repair:** claim a single active job on the UI thread before launch, test for `NULL`, retain the thread/job state, and implement coordinated close/cancel behavior. Send progress/results to the UI thread through application messages. Avoid simply blocking the UI thread while a worker may be waiting on synchronous UI calls.

## P1: Sector compatibility and disk-layout adaptation are absent

**Location:** [HandleDisk.cpp](../WindowsCloneDisk/HandleDisk.cpp), geometry helpers; [WindowsCloneDisk.cpp](../WindowsCloneDisk/WindowsCloneDisk.cpp), buffer setup and copy sizing.

Geometry `BytesPerSector` is used as the alignment value. Even `GetPhysicalSectorSize`, which is currently unused, reads geometry rather than querying physical-sector alignment. The code does not distinguish logical and physical sector sizes or validate that all transfer lengths, including the remainder, satisfy both devices' requirements. Microsoft's [unbuffered I/O documentation](https://learn.microsoft.com/en-us/windows/win32/fileio/file-buffering) describes separate transfer-size and buffer-alignment constraints.

Raw metadata copied between devices with different logical sector sizes may describe the wrong byte locations. A larger target receives the source bytes without relocating the backup GPT to the new end, growing partitions, or resolving duplicate disk identifiers. The extra target space is not explicitly cleared. There is no explicit flush or read-back verification stage.

**Repair:** initially require matching logical sector sizes and query physical alignment separately. Validate the complete transfer plan before writing. Define supported partition layouts and larger-target behavior, then add explicit completion/verification checks. Do not infer mountability or bootability from a byte count alone.

## P2: Enumeration and resource ownership need hardening

**Location:** [HandleDisk.cpp](../WindowsCloneDisk/HandleDisk.cpp), enumeration helpers; [WindowsCloneDisk.cpp](../WindowsCloneDisk/WindowsCloneDisk.cpp), initialization, copying, and error formatting.

| Finding | Consequence | Suggested repair |
| --- | --- | --- |
| Heap allocation failure in `GetLocalDiskCBEntries` overwrites `*pPtr` with null but restores the old count only | Old buffer leaks and the documented unchanged-on-failure guarantee is broken | Allocate into a temporary pointer and commit pointer/count together |
| Count and fill use separate WMI queries; the fill result is not reconciled with the allocated count | Disk removal can leave uninitialized entries exposed to the UI; additions can be missed | Build one validated snapshot with the actual filled count |
| WMI property return codes and variant types are unchecked; `Index` is read through the 16-bit `uiVal` member | Invalid or truncated device records can be constructed | Check HRESULTs/types and extract the full-width disk index |
| Captions are copied into a fixed 64-character array without a truncation policy | Unexpected/long captions are not handled robustly | Use bounded, validated strings and an explicit display limit |
| `CoSetProxyBlanket`'s result is overwritten; WMI waits use `WBEM_INFINITE` on the UI thread | Discovery failures are obscured and startup/refresh can block the interface | Check each stage and move bounded discovery off the UI thread |
| WMI enumerator is never released; the created font and final disk array are not freed | Resources accumulate, especially across refreshes | Use scoped COM, GDI, and heap ownership |
| Disk handles are closed only if both opens succeed | If just one open fails, the other handle leaks | Own each successful handle immediately |
| `ShowErrorMessage` frees a `FormatMessageW` allocation with `HeapFree` | Deallocation does not follow the API contract | Use `LocalFree`, as required by [FormatMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-formatmessagew) |

## P2: Validation and maintenance gaps

The checkout contains no automated tests or CI workflow, no mockable disk-I/O boundary, and no documented Windows/device compatibility results. Copy logic and UI behavior are intertwined. There is no persistent diagnostic log, throughput/ETA display, or explicit failed/cancelled job state. The fixed-size interface has no deliberate DPI/layout handling, and the About dialog still carries a 2022 copyright while the license carries 2026.

After fixing the P1 problems, extract the copy engine and test meaningful failure cases: read errors, short/failed writes, early end-of-input, failed restoration, failed thread launch, and controlled shutdown. Use disposable virtual disks for integration checks of capacity and sector-size behavior, compare copied bytes independently, and ensure no test targets an actual user disk. Add build checks for the configurations the project intends to support.
