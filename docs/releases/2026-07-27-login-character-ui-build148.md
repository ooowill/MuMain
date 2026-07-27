# Login and character selection final UI

Date: 2026-07-27
Status: visually approved and publicly released

## Approved login panel

- The legacy account/password controls remain removed because authentication uses Google.
- The panel keeps the `MU Online` title and selected server name.
- The localized `Login with Google` button uses the same text-button styling and hover behavior as the other login controls.
- The official Google G icon has transparent background and is delivered in source TGA plus the native OZT runtime format expected by the legacy loader.
- The icon row order compensates for the legacy TGA reader so the G renders upright.

Approved icon hashes:

- `google_g_official_transparent.tga`: `4b8138d30f488592a552654888a84b8641f1581631005255bb38a65291406a21`.
- `google_g_official_transparent.OZT`: `a778ed44cd91081e6554cd34df1fb86b1b4e09764ecc23a55d0b5c358d5d326c`.

## Approved character-selection corner

- The lower-right ornament uses `Interface\deco.tga`, loaded as `BITMAP_LOG_IN + 2` in the character scene.
- Alpha testing is explicitly restored after rendering the translucent bottom bar and before drawing the ornament.
- This removes the opaque black rectangle and prevents fragments from the delete-button atlas from appearing.
- Button positions, character list, scene objects, camera, waterfalls and terrain were not changed by this correction.

## Approved public release

- Client version: `0.1.97`.
- Client build: `148`.
- Package: `client-0.1.97-148.mupkg`.
- Package size: `5,087,840` bytes.
- `Main.exe` SHA-256: `42ab50078f804bb6307c7d52085b914ef2690c47f6774e55aa64e63a1682c3ac`.
- Package SHA-256: `be3f22228e7b268f095b5b3ef9f4f7ea3f719c0d8fe692236cd92f710cfbebc8`.
- Launcher manifest SHA-256: `3c9fa26c33ce68a1f65adb2e075691fe1bc5f051e477c1e39d995b24543736f2`.
- Anti-cheat version: `20260727.3`.
- Anti-cheat build id: `season6-20260727-character-corner-alpha-build148`.
- Anti-cheat manifest SHA-256: `617e23751d63d34c487eb8f6c7f1fe6735dfc392ecdbbceaaf15236ec588a126`.
- Anti-cheat policy SHA-256: `a92f1a16c249799f3ce372b5b7aba5e9788d46e8ff757e642a048abad259515f`.
- Public verification: launcher manifest reports build 148; package is reachable; anti-cheat signature is valid; `13,352` files checked with zero missing and zero mismatched.
- The installed test client received the release through the official launcher and the final visual result was approved.

## Release records

- Full staged source: `/opt/muonline/client-release/source-build148`.
- Incremental release root: `/opt/muonline/client-release/incremental-build148-character-corner-alpha`.
- Private signed release: `/opt/muonline/client-updates-private/build148-character-corner-alpha`.
- Publication backup: `/opt/muonline/backups/20260727-character-corner-alpha-build148`.

## Repository policy

`Main.exe`, DLL files, `.mupkg` packages, private keys, generated build folders, map binaries and other heavy release artifacts are not committed. The small Google source/runtime icon files are retained in the signed full release and identified by their hashes above.