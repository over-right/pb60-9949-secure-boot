# Reproducing the tested sequence

I verified this procedure on my ASUS PB60-based Crestron UC-Engine running BIOS 9949. I verified the firmware backup by comparing repeated reads. Restoration from that backup remains untested. Keep a backup of important data, the existing EFI media files, and your machine's own firmware before testing. On encrypted Windows systems, make sure you have the recovery key before changing Secure Boot. Do not clear the TPM.

## 1. Build and prepare a USB

Build with an x64 LLVM-MinGW toolchain. The reference toolchain was the [2026-10-06 x86-64 UCRT release](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006). The scripts require no firmware image, third-party driver, or admin rights to compile the helpers.

```powershell
./build.ps1 -ToolchainBin 'C:\path\to\llvm-mingw\bin' -VerifyReference
```

The optional reference check compares ordinary file SHA-256, not the PE image hash used by Secure Boot. A different compiler or rebuilt binary may produce a different digest. Review the source before trusting a build. The tested writer's file SHA-256 is `a6b92895582aec910a8f0139a03d850a6ccd5b1f3513decb4b8d51afcde92b45`; its PE image SHA-256 is `55181e171cf877ce379f03620438c8b49c40ea367f6397b0bf18e6260c14f23d`.

Use a spare FAT32 UEFI USB. Identify it by size and device before copying. Obtain `bootx64.efi` (shim), `grubx64.efi`, and `mmx64.efi` (MokManager) together from the same verified, signed amd64 Ubuntu media. The hardware run used the same signed files previously booted on that unit. Compatibility of other Ubuntu builds depends on the machine's allowed signatures, revocations and SBAT. See [Ubuntu's Secure Boot documentation](https://documentation.ubuntu.com/security/security-features/platform-protections/secure-boot/).

```text
USB root/
  PB60_DIAG_MEDIA.ID
  PB60_CUSTOM.ID
  EFI/BOOT/bootx64.efi        # original signed shim
  EFI/BOOT/grubx64.efi        # original signed GRUB
  EFI/BOOT/mmx64.efi          # matching signed MokManager
  EFI/BOOT/PB60DIAG.EFI       # local read-only image
  EFI/BOOT/PB60MODE.EFI       # local one-attempt writer
  EFI/BOOT/grub.cfg
  EFI/ubuntu/grub.cfg
  boot/grub/grub.cfg
```

Copy the same [`boot/grub.cfg`](../boot/grub.cfg) to the three listed paths. With `$usb` explicitly set to the verified **USB root**, create the marker files with the exact ASCII bytes and CRLF ending:

```powershell
[IO.File]::WriteAllText((Join-Path $usb 'PB60_DIAG_MEDIA.ID'), "PB60_READ_ONLY_DIAGNOSTIC_V1`r`n", [Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $usb 'PB60_CUSTOM.ID'), "PB60_CUSTOM_MODE_V1`r`n", [Text.Encoding]::ASCII)
```

The GRUB menu waits for a choice and defaults to the reader. There is no automatic write. It does not require a network. If the USB contains `PB60_CUSTOM.TXT` from an earlier attempt, preserve it and stop; that journal intentionally blocks another write.

## 2. Verify the baseline, then change only the mode byte

1. Use Windows **Shift + Restart → Use a device → UEFI USB Device** where available. The hardware test used the one-time boot path without permanently changing boot order. See [Windows Recovery Environment](https://support.microsoft.com/en-us/windows/experience/backup-recovery/windows-recovery-environment).
2. Optional but useful: choose **PB60: Enroll one helper hash** → **Enroll hash from disk** and select **`EFI/BOOT/PB60DIAG.EFI`**. If prompted, press a key to open MOK management. On the next USB boot choose **PB60: Run read-only diagnostic**. Inspect the numbered `PB60EFI-*.TXT` on the USB: expect `SecureBoot=01`, `SetupMode=00`, and `SecureBootSetup` attributes `0x7` and data `01000000000000`. Stop on a mismatch. `SystemAccess` was not found in the external boot session; do not create or fake it.
3. To authorize the separate writer, return to **Enroll one helper hash** → **Enroll hash from disk**, and select only **`EFI/BOOT/PB60MODE.EFI`** from the intended USB. Do not select **Enroll key from disk** or change signature-validation settings. A shim MOK hash authorizes that image through shim; it does not insert a PK or firmware `db` entry. [Upstream MokManager](https://github.com/rhboot/shim/blob/main/MokManager.c)
4. On the next USB boot choose **PB60: Attempt one Custom-mode write**. Type uppercase **`CUSTOM`** and press Enter only if you approve the setting change. Anything else cancels. Return to Windows after the attempt. Check `PB60_CUSTOM.TXT`: the successful run reported zero status for `SetVariable` and `GetVariable`, `Readback data=01010000000000`, and `PB60_WRITE_VERIFIED`. An incomplete journal means the outcome is unknown; check read-only and do not retry. GRUB may show `image not loaded` after a helper has already returned; the journal is the evidence.
5. Verify the persisted setting before touching PK. On the tested Windows machine a direct read of `SecureBootSetup` returned `01010000000000` with attributes `0x7`. Alternatively, reboot to the already-trusted **read-only** USB helper and check the new numbered log. Secure Boot should still be enabled at this stage.

**Keyboard note from my PB60 test.** Caps Lock and Shift seemed to cancel earlier attempts. On the successful attempt I typed the word and pressed Enter, and I don't know what case the firmware passed to the helper. The tested helper compares the UEFI input against uppercase `CUSTOM` followed by Enter. The journal confirms the write and readback but does not record the keystrokes.

## 3. Remove the Platform Key only

Record the pre-deletion state and, if you want proof of which databases changed, hash the byte arrays returned by `Get-SecureBootUEFI` for `PK`, `KEK`, `db`, and `dbx` from Administrator PowerShell. Never publish the raw databases or private recovery keys.

1. Unplug the SanDisk, restart, tap **Delete**, and check **Access Level: Administrator**. In the tested BIOS the route was **Boot → Secure Boot → Key Management**. Leave **Factory Key Provision** disabled.
2. Select the **PK / Platform Key** row, then the action that explicitly deletes **only that key**. Do not use **Reset To Setup Mode**, **Clear Secure Boot Keys**, or **Restore Factory Keys**. In the inspected 9949 forms, the first of those is an all-database deletion operation. If the action is missing or returns an error, stop.
3. Save/exit and boot Windows normally. Administrator PowerShell should report `Confirm-SecureBootUEFI` as `False`, `Get-SecureBootUEFI -Name PK` as undefined, and `SetupMode` byte `01`. Check that the `KEK`, `db`, and `dbx` sizes and SHA-256 hashes are unchanged. A future key restore or update can change this state.

The diagnostic hashes remain separate from the firmware key databases. If cleanup is desired, a [supported `mokutil` build](https://github.com/lcp/mokutil/blob/master/man/mokutil.1) can queue removal of each exact image hash (`--delete-hash`) for physical confirmation in MokManager. Do not reset all MOK entries. Cleanup was not part of the measured hardware result.
