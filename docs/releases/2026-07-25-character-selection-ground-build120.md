# Character selection ground and public editor isolation

Date: 2026-07-25
Status: visual approved and publicly released

## Approved World75 object

- Logical scene: `WD_74NEW_CHARACTER_SCENE`.
- Physical data folder: `Data/World75` and `Data/Object75`.
- Object type: `82`.
- Model file: `Object83.bmd`.
- Internal model name: `character_ground.smd`.
- Texture: `ground.OZJ`.
- Position: `11310.7, 12342.0, 80.0`.
- Rotation: `4.0, 0.0, 131.0`.
- Scale: `2.600`.
- Rendering uses the native World75 object pipeline. No custom OBJ renderer is used.

Approved asset hashes:

- `Object83.bmd`: `87e72393bb3f53f9d2aafafa33dca2ab0f8799424a61fcda0d0509bf8a8ba612`.
- `ground.OZJ`: `775691c39622d2ae9cd3593b2d869b0ba61223bf27590aadbfa1c75a70da0044`.
- `EncTerrain75.obj`: `943ccf832af0686c9725eeb06b9ad3729ea837f394c0ae6ac67fcf1c8b63fcdf`.

## Public release

- Client version: `0.1.69`.
- Client build: `120`.
- Package: `client-0.1.69-120.mupkg`.
- `Main.exe` SHA-256: `c874f8b660a1a57e1e914d847b463f570831d9cc953a087802751d445c75fbeb`.
- Package SHA-256: `622a5824063ca381b84946c206e51f51bf552c6b1d52d2ba2499d50ae6921b37`.
- Launcher manifest SHA-256: `34be8d94bd477a7a98993e73bfad4d9b5890f95d7dea7652201fb1818c32793a`.
- Anti-cheat version: `20260725.10`.
- Anti-cheat build id: `season6-20260725-character-ground-approved-no-editor-build120`.
- Anti-cheat manifest SHA-256: `cae2d7fe2d8fb73bf6d9152a95b73bdb31c537be21cecbf4ac82e4e45f2ac23a`.
- Anti-cheat policy SHA-256: `48822f809b64ba8910face4dddd88978a0512e9731bd3a0a8407339f273610d3`.
- Public verifier result: package signature valid, 13,350 anti-cheat files checked, zero missing and zero mismatched.

## MU Editor isolation

- The player release is compiled with `ENABLE_EDITOR=OFF`; `_EDITOR` code is not linked into the public executable.
- The administrative editor remains a separate local build under `windows-x86-mueditor`.
- `Tools/Release/Assert-NoEditorBinary.ps1` scans the staged executable before a package can be created.
- The guard blocks known editor markers, including `--character-map-editor` and the World75 editor window title.
- The public anti-cheat policy authorizes only the exact build 120 `Main.exe` hash above.
- Command-line arguments alone are not treated as a security boundary.

## Repository policy

`Main.exe`, DLL files, BMD/OZJ/OBJ assets, `.mupkg` packages, private keys, and other heavy binaries are not committed. This document records the approved transform and hashes needed to identify the deployed assets.