# Third-party code in this repository

Every piece keeps its license; the license file sits next to the code. All are compatible with the port's
GPL-3.0-or-later.

| Folder | What | From | License |
|---|---|---|---|
| `maxwell_shader/` | The Ryujinx 1.3.2 shader translator, unchanged apart from the patches below (Ryujinx Team and contributors) (`Ryujinx.Graphics.Shader`, `Ryujinx.Common`, `Spv.Generator`) | MacNativeGames `vendor/ryujinx` @b75be8c (Ryujinx 1.3.2, unmodified there), with the patches of `shader_translator/patches/` applied here (0001-0004 MacNativeGames', 0005-0006 ours) | MIT (`maxwell_shader/LICENSE.txt`) |
| `imgui/` | Dear ImGui 1.91.9b (Omar Cornut and contributors): the core and the SDL3 / SDL_Renderer3 backends, unchanged; the launcher's interface | https://github.com/ocornut/imgui tag v1.91.9b (f5befd2) | MIT (`imgui/LICENSE.txt`) |
| `nanosvg/` | NanoSVG (Mikko Mononen): `nanosvg.h`, `nanosvgrast.h`, unchanged; the launcher's icons | https://github.com/memononen/nanosvg @239e102 | zlib (`nanosvg/LICENSE.txt`) |
| `stb/` | `stb_image.h` 2.30 (Sean Barrett), unchanged; the launcher's PNG images | https://github.com/nothings/stb @003501f | MIT or public domain, the player's choice (`stb/LICENSE`) |
| `shader_translator/` | MacNativeGames' `totk_xlate`: the service around that translator that turns the game's NVN shaders into SPIR-V; built as `ultrs-shaders.exe` by `tools/build_xlate.py` | MacNativeGames `games/totk/tools/totk_xlate` @b75be8c, without its macOS build script | MIT (`shader_translator/LICENSE`) |

Elsewhere in the tree: `src/gfx/tex/bc7enc/` (bc7enc and rgbcx, Rich Geldreich, MIT) and the files ported from
MacNativeGames (MIT), LASRecomp (MIT) and B2recomp (GPL-3.0), each named in its header comment;
`tools/shaders/inventory.py` (MacNativeGames, MIT). The launcher's fonts in `assets/launcher/fonts/` (compiled into
the launcher by `tools/build_launcher.py`): Inter (The Inter Project Authors) and Montserrat Alternates (The Montserrat
Project Authors), both SIL Open Font License 1.1, their license texts next to them (`OFL-*.txt`).
