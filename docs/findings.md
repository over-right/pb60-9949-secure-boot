# Findings and evidence

## Why one PB60 accepted PK deletion and the other did not

I tested on my Windows 11 Crestron UC-Engine, an ASUS PB60 with Intel B360 and BIOS 9949. I also own a PB60 with Crestron BIOS 9948 that I use as an Android TV box. I had previously removed its Platform Key through BIOS Setup.

The 9949 unit was in **Standard mode**, with Secure Boot enforcing and PK installed. The 9948 unit was in **Custom mode**, with PK absent. Historical records in the 9948 NVRAM contained Custom mode alongside a valid PK, suggesting Custom was already set before PK removal. The records do not establish who set Custom or how. Both inspected BIOS builds hide the mode selector. Their relevant Setup and variable-policy code did not show a new PK-deletion restriction introduced by 9949. That is a comparison of the inspected paths, not a claim that the releases are identical.

The Crestron firmware forces the Secure Boot enable setting on in initialization, and its OS Type callback also writes the enable setting on. Neither observed path selects Custom. This accounts for **Other OS** and the enable toggle failing to stick on the test unit. Disabling Factory Key Provision does not remove an already-enrolled PK.

## The variable and the two separate operations

The GUID-qualified variable is `SecureBootSetup`, `7B59104A-C00D-4158-87FF-F04D6396A915`, attributes `0x7`, seven bytes. In the inspected 9949 firmware, zero-based byte 0 is the enable setting, byte 1 is Standard (`00`) or Custom (`01`), and byte 2 is factory-key provisioning. The guarded helper expects the full observed baseline and keeps the other six bytes intact.

The existing Setup mode setter uses ordinary `SetVariable`. The relevant SMM variable handler has a late-boot write lock; a variable can remain readable from Windows while the firmware rejects changing it there. The preboot application used the ordinary firmware service and let the firmware decide whether to accept it. It did not patch firmware, override a lock, or fabricate authorization.

The inspected reduced-authentication key-update path separately checks Custom mode, boot phase, and an AMI `SystemAccess` result with value `00` and attributes `0x2`. A signed iPXE probe did not find `SystemAccess` by name. The dedicated trusted read-only helper then called `GetVariable` directly for its known GUID and returned `EFI_NOT_FOUND` in the external boot session. That does not describe what happens inside the native BIOS Setup session. On the actual machine, the mode write succeeded from the external EFI app despite that read-only observation, and the native BIOS subsequently accepted PK-only deletion. No external PK-delete call was used.

## What was actually tested

- **Recovery groundwork:** matching 16 MiB software reads of each unit's main SPI flash, including descriptor, Ethernet configuration, Intel ME and BIOS. The earlier AMI Windows export did not cover the full main flash. Backups were verified but external recovery and restoration were not tested. Neither Crestron downgrade nor retail ASUS cross-flash was tried.
- **Boot media:** the first live USB layout failed on the hardware. Direct boot from a FAT32 EFI partition worked for the Linux collection and later EFI tools. A signed iPXE probe also worked and saved an early preboot log to a local receiver; neither iPXE nor a second PC is required by the final procedure.
- **Trust:** the unsigned reader was rejected offline when tried directly, via Ubuntu shim and via signed GRUB; chaining from signed iPXE did not work either. Tests using copies of captured trust data showed that shim rejected a wrong hash and accepted the exact image hash. On hardware, the signed chain and manually approved per-image hashes let both helpers run while Secure Boot remained enabled.
- **Write guards:** disposable Secure Boot VM cases covered a missing marker, a nonmatching baseline, cancellation, successful write/readback, and a journal blocking a repeat. Virtual PK, KEK, `db`, and `dbx` were checked for changes. VM tests established helper behavior; actual firmware acceptance was measured separately on the PB60.

## Measured hardware result

| Stage | `SecureBootSetup` | Secure Boot | UEFI mode | PK |
|---|---|---|---|---|
| Initial verified state | `01 00 00 00 00 00 00` (Standard) | On | User | Present |
| After the one-attempt helper; verified again from Windows | `01 01 00 00 00 00 00` (Custom) | On | User | Present before the later BIOS operation |
| After native BIOS PK deletion and a Windows boot | Not queried in the post-delete inventory | Off | Setup | Undefined |

The write journal recorded `SetVariable=0x0000000000000000`, a successful immediate readback with attributes `0x7`, and `PB60_WRITE_VERIFIED`. The final Windows inventory found that **the sizes and SHA-256 hashes of KEK, `db`, and `dbx` all matched** their pre-deletion values. Only PK was removed among the four inventoried databases. This verifies the resulting state after reboot; it does not guarantee that a future key restore, provisioning event, or update will leave it in that state.

The local checkout was rebuilt with the reference toolchain and both ordinary file SHA-256 values matched the actual tested EFI images. Firmware dumps, raw databases, device identifiers, and private system paths are deliberately excluded from this publication.
