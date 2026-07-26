# Character slots and store button

Date: 2026-07-26
Status: functionally approved and publicly released

## Account policy

- Every account starts with 10 unlocked character slots.
- One additional slot costs EUR 5.00.
- The account limit is 200 character slots.
- Purchases are fulfilled only after the existing website payment validation.
- Fulfillment is idempotent and updates the slot capacity of the purchasing account.

## Character selection

- The list renders all unlocked account slots through paged character data.
- The row after the last unlocked slot is `Comprar Slot`.
- The store destination is:
  `https://muonline.pt/store?tab=server&server_category=utilitye`.
- The purchase row captures mouse-down and mouse-up before list selection,
  pagination and scrollbar processing.
- The browser is opened through the real Windows `ShellExecuteW` loaded from
  `shell32.dll`; the compatibility stub is not used.
- The public client remains compiled without the MU Editor.

## Website and server

- Product key: `character-slot`.
- Price: EUR 5.00.
- Store category accepts the public `utilitye` URL alias.
- OpenMU persists `MaximumCharacterSlots` per account.
- Database constraints enforce values from 10 through 200.
- The signed website-to-server bridge applies one slot per validated purchase.

## Approved public release

- Client version: `0.1.73`.
- Client build: `124`.
- Package: `client-0.1.73-124.mupkg`.
- `Main.exe` SHA-256:
  `268f8e52dd617a48e83867e7e51ec8176231d9da8e577fc03048d286e768ba5a`.
- Package SHA-256:
  `2c3cda2ae471e401430ab841e9b602cc5ca7c430158bced011c4a1de3c2b4d0f`.
- Launcher manifest SHA-256:
  `983ef90c8686bbab4b2828dc1c1e54370f8045fb7e1e882cd060830b65b228e6`.
- Anti-cheat version: `20260726.4`.
- Anti-cheat build id:
  `season6-20260726-character-slot-click-capture-build124`.
- Anti-cheat manifest SHA-256:
  `cd368bb576a3853c1790a2463d5c0f5d77e2d47ed45e63e891c8b7af6f8e7930`.
- Anti-cheat policy SHA-256:
  `b11fe1009467696874a5f1c145aff5e2febdce902a8ddabfa3c77ec54f429ef5`.
- Public verification: launcher signature valid; anti-cheat signature valid;
  13,350 files checked, zero missing and zero mismatched.
- The installed test client received the release through the official launcher
  and its `Main.exe` hash matched the public release.

## Versioned server records

- Staged source:
  `/opt/muonline/client-release/source-build124`.
- Source backup:
  `/opt/muonline/backups/20260726-character-slot-click-capture-build124-client-source`.
- Anti-cheat backup:
  `/opt/muonline/backups/20260726-character-slot-click-capture-build124-anticheat`.

## Repository policy

`Main.exe`, DLL files, BMD/OZJ/OBJ assets, `.mupkg` packages, private keys and
other heavy binaries are not committed. This record keeps the behavior,
versions and hashes required to identify the approved deployment.
