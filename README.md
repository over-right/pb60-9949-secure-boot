# Disabling Secure Boot on a Crestron ASUS PB60 (BIOS 9949)

I tested this on my Crestron UC-Engine, an ASUS PB60 running BIOS **9949**. BIOS Setup refused to delete the Platform Key while the firmware was in Standard mode. I **switched the hidden Standard/Custom setting to Custom with a preboot EFI helper, then deleted only the Platform Key from BIOS Setup**. Windows booted normally afterward. I verified that Secure Boot was off and that KEK, `db`, and `dbx` were unchanged. The procedure used the original firmware and a USB drive.

This is a report and source release for a specific machine, not a general ASUS firmware tool. Similar model names or version numbers do not establish compatibility. Use only on hardware you own or administer with permission. The writer makes a persistent firmware setting change; deleting PK then turns off Secure Boot enforcement. Check Windows device encryption and have the recovery key available if protection is active. See [Microsoft's BitLocker recovery guidance](https://learn.microsoft.com/windows/security/operating-system-security/data-protection/bitlocker/preboot-recovery-screen).

- [Download the prebuilt EFI helpers and checksums](https://github.com/over-right/pb60-9949-secure-boot/releases/tag/v0.1.0). The release includes the read-only diagnostic and the Custom-mode helper.
- [Walkthrough](docs/procedure.md): verify the downloads, prepare the USB, enroll each helper's hash through shim, verify Custom, and remove PK only in Setup.
- [Technical findings and evidence](docs/findings.md): comparison with 9948, why the obvious settings failed, trust experiments, test boundaries, and the measured result.
- [Read-only diagnostic](src/PB60-EFI-Diagnostic.c) and [one-attempt Custom-mode helper](src/PB60-Custom-Mode-Helper.c): separate images with separate trust decisions.
- [GRUB menu for the USB](boot/grub.cfg) and [optional source build instructions](docs/procedure.md#optional-build-from-source).

The successful test changed `SecureBootSetup` from `01 00 00 00 00 00 00` to `01 01 00 00 00 00 00`. Secure Boot stayed enabled until the separate PK deletion in BIOS. Do not rerun the writer after an attempt; check its journal and verify the current state instead.

This repository does not distribute firmware dumps, vendor capsules, signed bootloader binaries, private keys, or unredacted machine logs.
