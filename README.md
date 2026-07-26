# MU Online Client Sources

[![MinGW Build](https://github.com/sven-n/MuMain/actions/workflows/mingw-build.yml/badge.svg?branch=main)](https://github.com/sven-n/MuMain/actions/workflows/mingw-build.yml)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/sven-n/MuMain)

This is my special fork of the Season 5.2 client sources [uploaded by Luois](https://github.com/LouisEmulator/Main5.2).

The ultimate goal is to clean it up and make it compatible and feature complete
to Season 6 Episode 3.

What I have done so far:
  * 🔥 The framerate has been increased.
    * By default, it uses V-Sync without fps limit. If V-Sync is not
    available, it limits to 60 fps.
    * The options menu includes a checkbox to reduce effects to achieve higher frame rates.
    * Chat commands:
      * Change FPS-Limit: `$fps <value>`
      * V-Sync: `$vsync on` / `$vsync off`
      * Show simple FPS counter: `$fpscounter on` / `$fpscounter off`
      * Show detailed performance overlay (FPS stats, percentiles, frame graph): `$details on` / `$details off`
  * 🔥 Optimized some OpenGL calls by using vertex arrays. This should result in
    a better frame rate when many players and objects are visible.
  * 🔥 Added inventory and vault extensions.
  * 🔥 The master skill tree system was upgraded to Season 6
  * 🔥 Unicode support: The client works with UTF-16LE instead of ANSI in memory.
    All strings and char arrays have been changed to use wide characters.
    Strings coming from files and the network are handled as UTF-8.
  * 🔥 Replaced the network stack with MUnique.OpenMU.Network to make it easier to
    apply changes. This repository includes a C# .NET 10 client library which is built
    with Native AOT.
  * 🔥 The network protocol has been adapted for Season 6 Episode 3 - there is probably
    still some work to do, but it connects to [OpenMU](https://github.com/MUnique/OpenMU)
    and is playable. Additionally, the protocol has been extended so it's not standard
    anymore.
    * Damage, Exp etc. can exceed 16 bit now.
    * Improved item serialization
    * Improved appearance serialization
    * Added monster health status bar after attack
  * 🔥 Significant changes from Qubit have been incorporated, such as
    * Rage Fighter class
    * Visual bug when Dark Lord walks with Raven
    * Item equipping with right mouse click
    * Glow for red, blue and black fenrir
    * Additional screen resolutions
  * 🔥 Incorporated MU Helper UI and logic - there's some work to do but core functionality is usable
  * Added custom MU Online HUD systems used by the local server, including Account Guard, Azoth, Game Settings and the account-wide Jewel Bank. The Jewel Bank button opens a native window, parses `#JWB1|auto|counts`, and uses assets from `Data/Interface/JewelBank`.
    * Jewel Bank now lists account-wide jewels in a native table with 5 jewels per page, common jewels first, 3D item previews, and per-row `Guardar`/`Sacar` image buttons.
    * `Jewel of Full` keeps the original full behavior: +15, skill, luck, option +28 and all available Excellent options.
  * 🔥 Auto-reconnect system
  * Removed if-defs for Rage Fighter class as we are targeting Season 6, so Rage
    Fighter should always be included.
  * Some minor bug fixes, e.g.:
    * Storm Crow item labels
    * Ancient set labels
  * The code has been refactored. A lot of magic values have been replaced by
    enums and constants.
  * 🔥 New Translation system (see [docs/translation-system.md](docs/translation-system.md))

What needs to be done for Season 6:
  * Lucky Items

## How to build & run

### Requirements
* **CMake** 3.25 or newer (bundled with Visual Studio and CLion)
* **.NET SDK 10.0** or newer (for building the Client Library)
* **Visual Studio 2022+** with C++ and C# workloads, **CLion**, or **Rider** (see IDE-specific instructions below)
* A compatible server: [OpenMU](https://github.com/MUnique/OpenMU)

### First Time Setup - Initialize Submodules

The project uses three git submodules under `src/ThirdParty/`:

- `SDL` - windowing, input and audio backend (required for all builds)
- `SDL_mixer` - audio mixer (required for all builds)
- `imgui` - in-game editor UI (only needed when built with `-DENABLE_EDITOR=ON`, independent of Debug/Release)

CMake initializes these automatically on first configure. If that fails for any reason, run from the repository root:

```bash
git submodule update --init
```

### Build Configurations

There are two orthogonal choices: **editor on/off** (configure-time, picked via preset) and **Debug/Release** (build-time).

#### Editor builds (`windows-x86-mueditor` / `windows-x64-mueditor`)
- Configure preset sets `ENABLE_EDITOR=ON`
- Includes the in-game MU Editor (ImGui-based); the `imgui` submodule must be initialized
- Press **F12** in-game to toggle the editor
- Start with `--editor` flag to launch with editor enabled
- Preprocessor define: `_EDITOR`

#### Standard builds (`windows-x86` / `windows-x64`)
- Configure preset sets `ENABLE_EDITOR=OFF`; no editor code is compiled in and the `imgui` submodule is not initialized
- Zero editor overhead

Either configuration can be built as Debug or Release via the corresponding build preset (`*-debug` or `*-release`).

### Building with CMake and MinGW-w64 (Linux)

The repository also contains a CMake setup to cross-compile the Windows client
from Linux using a MinGW-w64 toolchain.

**Prerequisites**

  * A working MinGW-w64 toolchain (for example `i686-w64-mingw32-g++`).
  * A MinGW-w64 build of libjpeg-turbo which provides a `libturbojpeg` library
    (static or import library) on the library search path of your toolchain.
  * Standard Windows / OpenGL libraries shipped with MinGW-w64 (e.g. `opengl32`,
    `glu32`, `winmm`, `imm32`, `ws2_32`, etc.).

**Example build commands on Linux**

From the repository root:

```sh
cmake -S . -B build-mingw \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-i686.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw -j$(nproc)
```

If the linker reports `cannot find -lturbojpeg`, install a MinGW-w64 build of
libjpeg-turbo (providing `libturbojpeg.a` / `libturbojpeg.dll.a`) or adjust the
`target_link_libraries` entry in `src/CMakeLists.txt` to match the
name of the library available on your system.

---





### Building the Project

The project uses **CMake** as its build system. The `.NET Client Library` is automatically built by CMake when you build the main project - no manual publishing required!

#### Option 1: Visual Studio 2022+ (Recommended)

1. **Open the project:**
   - File → Open → Folder
   - Select the root `MuMain` folder (not `src`)

2. **Wait for CMake to configure** (automatically happens, check Output window)

3. **Select build configuration:**
   - Use the dropdown to select `x86-Debug` or `x86-Release`

4. **Build:**
   - Build → Build All
   - Or press `Ctrl+Shift+B`

5. **Run/Debug:**
   - Select `Main.exe` as startup item
   - Press `F5` to debug or `Ctrl+F5` to run
   - Working directory is automatically set to `src/bin`

**Note:** The working directory is pre-configured in `.vs/launch.vs.json`. If it's not working, ensure you opened the root `MuMain` folder, not a subfolder.

#### Option 2: CLion

1. **Open the project:**
   - File → Open
   - Select the root `MuMain` folder

2. **Wait for CMake to configure** (automatically happens)

3. **Configure working directory:**
   - Run → Edit Configurations
   - Select `Main`
   - Set "Working directory" to the build output directory (e.g. `cmake-build-debug/src/Debug`)
   - The post-build step copies all game assets there automatically

4. **Build and Run:**
   - Click the hammer icon to build
   - Click the play icon to run

#### Option 3: Rider (CMake via Command Line + Rider for Development)

Rider doesn't have full CMake support for C++ projects, so you need to generate a Visual Studio solution first:

1. **Generate the solution** (one-time setup):
   ```bash
   cmake -B build -G "Visual Studio 17 2022" -A Win32
   ```
   *(Adjust the generator version based on your installed Visual Studio)*

2. **Open in Rider:**
   - File → Open
   - Select `build/MuMain.sln`

3. **Build and Run:**
   - Build → Build Solution
   - Run → Run 'Main'

**Important:** When you modify `CMakeLists.txt`, you must manually regenerate the solution by running the cmake command again.

#### Option 4: Command Line Build (Windows)

Using CMakePresets.json with Ninja (same as IDEs, much faster than MSBuild):

```powershell
# Configure x86 build (first time only, or when CMakeLists.txt changes)
cmake --preset windows-x86

# Build Debug
cmake --build --preset windows-x86-debug

# Build Release
cmake --build --preset windows-x86-release

# For x64 builds, use windows-x64 presets instead
cmake --preset windows-x64
cmake --build --preset windows-x64-debug
```

**Note:** Ninja Multi-Config allows switching between Debug and Release without reconfiguring. Assets are automatically copied to the build output directory during compilation.

**To start fresh (clean build):**
```powershell
Remove-Item -Recurse -Force out
```

## MU Online custom notes - 2026-06-23 - Custom jewels

- The client now recognizes custom jewels in group 14, numbers 200 through 211. Until final art is provided, these items reuse the Jewel of Soul model and texture.
- Custom jewel names: Jewel of Grade VI, Jewel of Grade IX, Jewel of Greater Option, Jewel of Excellent Change, Jewel of Luck, Jewel of Skill, Jewel of Grade XV, Jewel of Full, Jewel of Socket, Jewel of Armored, Jewel of Ancient, Jewel of Excellent.
- Armored items use durability 255 as the client-side visual marker and show durability as infinity in the tooltip.
- Release build installed to `C:\Mu Online Client\Main.exe` on 2026-06-23. SHA256: `33A3E3B0A8FE980605B7B8087DF3FEBDBF23A45C5EFD555F58EB1EB509D72DF8`.
- Server-side consume logic and the live OpenMU database were updated separately in `/opt/muonline`.

**Run the executable:**
```powershell
# x86 Debug
./out/build/windows-x86/src/Debug/Main.exe

# x86 Release
./out/build/windows-x86/src/Release/Main.exe
```

#### Option 5: Command Line Build (Linux) !Not Working Yet!
For Linux builds, you'll need to add Linux presets to CMakePresets.json. Example workflow:

```bash
# Configure for Debug with Ninja (recommended)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_EDITOR=OFF

# Build
cmake --build build

# To switch to Release, reconfigure:
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_EDITOR=OFF
cmake --build build
```

**To start fresh (clean build):**
```bash
rm -rf build
```

**Run the executable:**
```bash
./build/src/Main
```

---

### Running the Client

It supports the common starting parameters `/u` and `/p`, example: `main.exe connect /u192.168.0.20 /p55902`.
The [OpenMU launcher](https://github.com/MUnique/OpenMU/releases/download/v0.8.17/MUnique.OpenMU.ClientLauncher_0.8.17.zip)
will work as well. By default, it connects to localhost and port `44406`.
The client identifies itself with Version `2.04d` and serial `k1Pk2jcET48mxL3b`.

## Documentation

- [Camera system](docs/camera-system.md) - modes, switching (F9), config,
  frustum culling, `$details` overlay, and the gameplay behaviour changes
  from the 3D camera rework.
- [DevEditor](docs/dev-editor.md) - the in-game tuning UI (F12, debug
  builds only).
- [Options window and config](docs/options-window.md) - runtime
  resolution / windowed toggle, slider rounding, and what the options
  window stores in `config.ini`.
- [Build guide](docs/build/README.md) - platform-specific build notes.
- [Translation system](docs/translation-system.md) - how the .resx ->
  generated C++ accessors pipeline works, how to add a string or a locale,
  runtime locale switching, and observer hooks for cached UI strings.

## MU Online PT Custom Notes

### 2026-06-19 - Configuracoes do Jogo

The HUD has a fourth top-right custom button for the native "Configuracoes do
Jogo" window. It persists client performance toggles in `config.ini` under
`[Game Performance]`: `HideWorldObjects`, `DisableHeavyEffects`,
`ReduceCharacterGlow`, `HideMountsPets`, and `SimplifyOtherPlayers`.

Button assets for the installed client live in
`C:\Mu Online Client\Data\Interface\Custom\game_settings_button.jpg` and
`C:\Mu Online Client\Data\Interface\Custom\game_settings_button_hover.jpg`;
their `.OZJ` companions must exist too because the runtime loader opens the
packed files.

2026-06-19 follow-up: keep these button images at `64x64`; larger JPEG sizes
are padded by the loader and render visually smaller in the HUD slot.

2026-06-19 follow-up 2: the same window now syncs performance flags with the
OpenMU server through `/gamesettings save <flags>` and hidden marker
`#GPS1|<flags>`. Server-side persistence lives in
`custom."GamePerformanceSettings"` by account. Current flags include
`HideWings`; hiding mounts/pets now also blocks the global mount pool; and
`SimplifyOtherPlayers` renders other players as base bodies without gear,
wings, pets, mounts or heavy effects.

2026-06-19 follow-up 3: `SimplifyOtherPlayers` must render with a clean,
solid object state. Personal Guard visuals can keep `EnableShadow` and blend
state set while marching; `RenderSimplifiedPlayerCharacter` now temporarily
forces shadow/blend/alpha off for the body pass so the simplified character
does not become a shadow/ghost.

2026-06-19 follow-up 4: the top-right custom HUD buttons now load from
`.tga` paths, causing the runtime to use the matching `.OZT` files with real
alpha. The old `.jpg/.OZJ` assets flatten transparent corners to black, so the
button family in `Data\Interface\Custom` must keep `.tga/.OZT` companions for
store, personal guard, character configuration, game settings, and switch
character buttons.

2026-06-20 follow-up: `DisableHeavyEffects` now also suppresses and clears
joint/sprite/particle/effect buffers when the mode is applied or while it is
active. This prevents old effect trails from rendering without the rest of the
effect pipeline after character selection or map reloads.

2026-06-20 follow-up: `DisableHeavyEffects` also skips the terrain grass render
pass, so the "Efeitos pesados" performance mode removes map grass without
changing the base terrain, collision, or pathing.

2026-06-21 follow-up: the top-right custom HUD buttons now also treat
`INTERFACE_COMMAND` and `INTERFACE_QUICK_COMMAND` as blocking foreground
windows. This keeps the custom buttons behind/hidden for the Command Window,
matching the Inventory behavior.

2026-06-21 follow-up: invalid minimap `.bmd` files no longer show a modal error
or destroy the client window. The minimap loader logs the corruption and
disables minimap data for that map, letting the player continue in maps such as
Peace Swamp/World57 even when `Minimap_World57_Eng.bmd` fails checksum.

2026-06-21 follow-up: the loading screen wallpaper now replaces the four
legacy `Data\Interface\LSBg01.OZJ` through `LSBg04.OZJ` mosaic assets. The
source image was resized to the loader's internal `800x600` canvas and split
as `400x512`, `400x512`, `400x88`, and `400x88` blocks so the existing
`LoadingScene` code can keep loading `Interface\LSBg01.JPG` through
`LSBg04.JPG` without a client rebuild.

2026-06-21 follow-up: the local `Data\Local\ServerList.bmd` now contains five
server groups named `Free`, `Gold`, `Platinum`, `Emerald`, and `Diamond`.
MuMain groups server ids by
`MAX_SERVER_PER_GROUP` (`20`) and this client build only inserts server slot
`1` for each group, so the OpenMU game server ids must be `0`, `20`, `40`,
`60`, and `80` to render five visible buttons in the server selection screen.

2026-06-21 follow-up 2: the VPS firewall must expose every visible game-server
port, not only the first one. For the five current VIP worlds, `44406/tcp`
serves the ConnectServer list/redirect flow and `55902/tcp` through
`55906/tcp` must be reachable externally; otherwise the list renders but
clicking a VIP server fails when the client opens the redirected GameServer
socket.

2026-06-21 follow-up 3: the old Helheim crowding warning in
`ServerSelWin.cpp` has been removed from the server selection screen. The
server-list asset also uses empty descriptions, so no legacy Helheim text is
shown beside the VIP group buttons.

## Contributing

### Coding rules

All code changes - by humans and AI assistants - should follow
[`docs/CODING_RULES.md`](docs/CODING_RULES.md). Read it before opening a PR.

AI coding assistants (Claude Code, Cursor, Codex, etc.) should also read
[`AGENTS.md`](AGENTS.md), which points at the same rules and at the build guide.

## Credits

  * Webzen
  * Louis
  * Qubit (tuservermu.com.ve)
  * Community members of RaGEZONE and tuservermu.com.ve for posting fixes
  * [Nitoy](https://github.com/nitoygo) for the MU Helper

## 2026-06-22 - Play do personagem substitui acesso antigo ao MU Helper
- Removidos os atalhos antigos do MU Helper no client: Home nao liga mais o helper direto e Z nao abre mais a janela antiga de configuracao.
- Adicionado botao `P` abaixo de `Configuracao do Personagem` no HUD superior direito. Ele liga/desliga a automacao do personagem lider usando a Configuracao do Personagem como fonte.
- O Play reaproveita o executor do MU Helper apenas como motor local: carrega alcance padrao de caca/coleta, coleta Zen/joias/ancient/excellent/itens comuns conforme flags, usa potion/reparo e tenta usar a skill atual do personagem antes de cair no ataque basico.
- Build x86 Release compilada e instalada em `C:\Mu Online Client\Main.exe`. Backup criado em `C:\Mu Online\backups\client\Main_20260622-105229_before_leader_play_button.exe`. SHA256 instalado: `11499AB1A447FF961C9BB5A118D743E11592E8851F361656068B794A24B72F14`.
## 2026-06-23 - Client: lista de teleporte mostra level/custo 1

- Ajustado `src/source/Network/MoveCommandData.cpp` no MuMain para normalizar toda entrada carregada de `MoveReq*.bmd` com `iReqLevel = 1` e `iReqZen = 1`.
- A janela de teleporte do client agora fica coerente com a regra do OpenMU: todos os mapas aparecem com level minimo `1` e custo `1 Zen`.
- Build client validado com `cmake --build ... --target Main --config Release` usando ambiente x86 do Visual Studio.
- `Main.exe` instalado em `C:\Mu Online Client\Main.exe`, SHA256 `E64C48DA90D213624F0BD1811D72E03D7C7BAADE237D745BDDB874707C08CBE8`.
- Backup anterior: `C:\Mu Online\backups\client\Main_20260623-224838_before_warp_list_level_cost.exe`.

## 2026-06-25 - Modelos 3D das joias custom

- Convertidos os OBJ de `C:\Mu Online\assets\Modelos\Joias` para BMD estatico do MuMain e normalizados para a escala da `Jewel of Soul` (`~22` unidades de altura).
- Adicionados em `src/bin/Data/Item`: `CJGradeVI`, `CJGradeIX`, `CJGreater`, `CJExChange`, `CJLuck`, `CJSkill`, `CJGradeXV`, `CJFull`, `CJSocket`, `CJArmored`, `CJAncient` e `CJExcellent`.
- As texturas do modelo usam nomes curtos `cj_*.tga/.OZT` em `Data\Item`; o loader continua referenciando `.tga`, mas carrega `.OZT` automaticamente.
- `ZzzOpenData.cpp` agora carrega os itens custom `14/200..211` com seus modelos proprios, em vez de reaproveitar `Jewel02.bmd`.
- Build Release x86 validado e instalado em `C:\Mu Online Client\Main.exe`; SHA256 `87CA1A0EBBBCA6458DF39916524848321B61D486825667C89F0D4A8FC3B55566`.
- Backup anterior do client: `C:\Mu Online Client\backup\custom-jewels-20260625-173308`.
- Ajuste visual posterior: os 12 BMDs `CJ*.bmd` foram reescalados de `~21.902` para `30.000` unidades de altura para preencher melhor a caixa do inventario. Nao exigiu rebuild do `Main.exe`; backup dos BMDs anteriores em `C:\Mu Online Client\backup\custom-jewel-scale-20260625-184609`.

## 2026-06-26 - Tooltip e uso das joias custom

- `ZzzInventory.cpp` agora mostra uma linha de efeito nas joias custom `14/200..211`, explicando o que cada uma faz ao passar o mouse.
- As joias custom recebem a cor especial de item e exibem a dica `Arraste sobre um item compativel` no tooltip.
- Itens com durabilidade `255` continuam sendo tratados como marcador visual de durabilidade infinita e aparecem como `Durability: ∞`.
- Build Release x86 instalado em `C:\Mu Online Client\Main.exe`; SHA256 `2B55C2351FB2EBD9305601E5F36D8702E41695A8E0DAD083BCD25803628D1DB4`.
- Backup anterior do client: `C:\Mu Online Client\backups\custom-jewel-tooltip-20260625-205841\Main.exe`.
- A regra de consumo fica no OpenMU em `/opt/muonline/core/current/OpenMU/src/GameLogic/PlayerActions/ItemConsumeActions/CustomJewelConsumeHandlerPlugIn.cs`; o runtime foi atualizado separadamente para permitir uso tambem em itens equipados.
- Follow-up servidor: `Jewel of Excellent`, `Jewel of Excellent Change` e `Jewel of Full` agora usam fallback global de excellent option quando o item nao tem lista propria, escolhendo apenas a familia coerente do alvo. Runtime final `MUnique.OpenMU.GameLogic.dll` SHA256 `ac11044b856f319cf85bc6f239966be898385def24c629368057bf413ef3d6d8`; backup anterior em `/opt/muonline/backups/runtime/custom-jewels-excellent-family-20260626-030300`.

### Follow-up 2026-06-26 - Client permite aplicar joias custom

- Corrigido o fluxo de drag/use do inventario: `NewUIInventoryActionController.cpp` agora reconhece as joias custom `ITEM_POTION + 200..211` como joias aplicaveis.
- `NewUIInventoryCtrl.cpp` passou a marcar alvos validos para joias custom usando uma regra permissiva de client (`IsCustomJewelTargetType`); o servidor continua sendo a autoridade final para compatibilidade.
- `ZzzInventory.h/.cpp` expõem `IsCustomJewelItemType` e `IsCustomJewelTargetType`, e `IsJewelItem` tambem reconhece as joias custom.
- Build Release x86 validado e instalado em `C:\Mu Online Client\Main.exe`; SHA256 `F0CBB4CDE5C86E1DC93E88825F1CDCFC6CB4531AEF4A974E9650B5A08680D4C0`.
- Backups do client: `C:\Mu Online Client\backups\custom-jewel-use-20260626-001738\Main.exe` e `C:\Mu Online Client\backups\custom-jewel-use-final-20260626-004055\Main.exe`.

### Follow-up 2026-06-26 - Banco de Joias em tabela

- Banco de Joias agora renderiza 22 joias account-wide em tabela paginada, 5 por pagina, com joias comuns primeiro e joias custom depois.
- Cada linha mostra o modelo 3D da joia ja carregado pelo client, contador com os digitos do Azoth e botoes visuais individuais `Guardar`/`Sacar`.
- `Jewel of Full` voltou ao nome original e ao comportamento full: +15, skill, luck, option +28 e todas as excellent options aplicaveis.
- Build Release x86 validado e instalado em `C:\Mu Online Client\Main.exe`; SHA256 `C149C36C8B46EB66A76713F218139ADF45D99480AABF7C8653C8944DD0A4B824`.
- Backup anterior do client: `C:\Mu Online Client\backups\jewel-bank-table-full-restore-20260626-171255\Main.exe`.

### Follow-up 2026-06-26 - Banco de Joias: coluna 3D e botao Sacar

- Botao `Sacar` do Banco de Joias regenerado a partir dos PNGs finais fornecidos (`sacar.png`/`sacar_hover.png`) e publicado como `Data\Interface\JewelBank\sacar.tga/.OZT` e `sacar_hover.tga/.OZT`.
- A tabela do Banco de Joias ganhou uma coluna `3D` antes do nome da joia, com area reservada para o modelo real renderizado pelo client.
- Build Release x86 validado e instalado em `C:\Mu Online Client\Main.exe`; SHA256 `EB7645B63AFAF86A8CE22D5262A28742C999B78E68A8FC08C4369808D7A3C60D`.
- Backup anterior do client: `C:\Mu Online Client\backups\jewel-bank-sacar-3d-column-final-20260626-193028\Main.exe`.
- Backup dos assets antigos: `C:\Mu Online Client\backups\jewel-bank-sacar-asset-20260626-183244`.

### Follow-up 2026-06-26 - Banco de Joias: textura Sacar e slot 3D

- Corrigida a causa real do botao `Sacar`: os IDs `31982/31983` usados pelo Banco de Joias colidiam com os botoes da selecao de personagem em `CharSelMainWin.cpp`, fazendo o HUD renderizar a arte cinza errada. A faixa custom do HUD foi movida para `BITMAP_INTERFACE_TEXTURE_END - 80`.
- Assets `sacar.png`/`sacar_hover.png` regenerados novamente para `sacar.tga/.OZT` e `sacar_hover.tga/.OZT` no source e no client instalado.
- A coluna `3D` deixou de usar um retangulo preenchido grande: agora usa slot pequeno com contorno fino, e os modelos das joias sao renderizados pelo `Render3D()` do HUD, como acontece nos inventarios.
- Build Release x86 validado e instalado em `C:\Mu Online Client\Main.exe`; SHA256 `34DFFB9DA562247AE153C5C402607F8EEB8693A81EE47283DAABE7E2DD16D4FF`.
- Backups: `C:\Mu Online Client\backups\jewel-bank-sacar-idfix-3dslot-20260626-215523\Main.exe` e `C:\Mu Online Client\backups\jewel-bank-sacar-idfix-assets-20260626-214312`.

### Follow-up 2026-06-27 - Precos NPC Amy e joias Azoth

- Ajuste operacional no OpenMU: `Potion Girl Amy` de Lorencia (`127,86`, solicitado como `128,86`) usa a politica de compra NPC com `npcBuyPriceDivisor = 1000`, reduzindo potes/consumiveis da loja para a escala economica custom.
- Loja Azoth de joias em Lorencia (`Wandering Merchant Harold`, storage `0a501000-0000-0000-0000-000000000015`): `Jewel of Socket` ajustada para `10000` e `Jewel of Full` para `15000`.
- Patch SQL publicado no servidor em `/opt/muonline/patches/openmu/npc-amy-potion-and-jewel-prices-20260627.sql`; config ativa em `/opt/muonline/configs/economy/global.economy.json` SHA256 `43BEBA38DC928EA3E5AC4390B258925908828CA69420668420855649648A10D1`.
- Backup remoto antes da alteracao: `/opt/muonline/backups/npc-amy-prices-20260627-132214`.
- OpenMU reiniciado e ouvindo `44406`, `55902` e servidores `55903..55909`.
