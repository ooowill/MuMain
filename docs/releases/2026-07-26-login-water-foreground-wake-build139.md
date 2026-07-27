# Login water foreground wake

Date: 2026-07-26
Status: visually approved and publicly released

## Corrected effect

- Logical scene: `WD_73NEW_LOGIN_SCENE`.
- Physical data folders: `Data/World74` and `Data/Object74`.
- Foreground wake layers: object types `7` and `18`.
- The approved Season 21 models already animate through their BMD bones.
- The custom `BlendMeshTexCoordV` scroll was removed only from types `7` and
  `18`, because it exposed the texture wrap as a bright straight seam.
- Type `1` keeps its existing texture-coordinate animation for the background
  ship effects.
- No model, texture, terrain, camera, server-list or login layout was changed
  by this corrective build.

## Reference comparison

- Reference client: `C:\Mu Online\Mu Online Season 21`.
- Reference world: `Data\World95`.
- Reference objects: `Data\Object95`.
- Object placement and decrypted model payloads matched the current scene.
- The regression was therefore in the additional runtime UV scrolling, not in
  the Season 21 BMD geometry or textures.

## Approved public release

- Client version: `0.1.88`.
- Client build: `139`.
- Package: `client-0.1.88-139.mupkg`.
- Package size: `4,220,241` bytes.
- `Main.exe` SHA-256:
  `71f1842d0ed094caaeca7e3a45d3c14979d52923e73634fbf877e3d08e170b03`.
- Package SHA-256:
  `0cc2fec14a813630a68b4ad64b7a11efcf8a2a0144746f3b3034c5bcd4406421`.
- Launcher manifest SHA-256:
  `8953df652e03867cebdf33c61573219d0ef0ec1e3103a8f43b3b20b0a739bde3`.
- Anti-cheat version: `20260726.19`.
- Anti-cheat build id:
  `season6-20260726-left-wake-reference-build139`.
- Anti-cheat manifest SHA-256:
  `52fc41097c1a6dafe760a3f3dda932341696cd5606bc7bab607f5d274908580d`.
- Anti-cheat policy SHA-256:
  `7c4f3000cd1c7c32982452175f300d72389aa12cccf54e175c35f13f085e981f`.
- Public verification: launcher signature valid; anti-cheat signature valid;
  `13,350` files checked, zero missing and zero mismatched.
- The installed test client received the release through the official launcher
  and the visual correction was approved.

## Release records

- Full staged source:
  `/opt/muonline/client-release/source-build139`.
- Incremental release root:
  `/opt/muonline/client-release/incremental-build139-left-wake-reference`.
- Private signed release:
  `/opt/muonline/client-updates-private/build139-left-wake-reference`.
- Publication backup:
  `/opt/muonline/backups/20260726-left-wake-reference-build139`.

## Repository policy

`Main.exe`, DLL files, `.mupkg` packages, private keys, generated build folders
and other heavy release binaries are not committed. This document records the
approved behavior, release identifiers and hashes.
