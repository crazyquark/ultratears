"""Where the build's tools are (tools/deps_install.py chooses and installs them).

CLANG64: a folder with clang, clang++, lld, ninja and the static libraries the port links (SDL3, opus, vpx, zstd,
iconv, libc++): deps/clang64/bin when installed (the builder package's trimmed MSYS2 CLANG64, or the MSYS2 packages a
source build downloads), else ULTRS_CLANG64, else C:/msys64/clang64/bin. XLATE: the shader translator ultrs-shaders.exe
(deps/shaders, built from this source when missing or stale; tools/build_xlate.py).
"""
import os
import pathlib

import deps_install

ROOT = pathlib.Path(__file__).resolve().parent.parent


def _clang64():
    prefix = deps_install.clang64_for(ROOT)
    return prefix / 'bin' if prefix else None


CLANG64 = _clang64()
XLATE = next((p for p in (ROOT / 'deps/shaders/ultrs-shaders.exe', ROOT / 'dev-local/build/shaders/ultrs-shaders.exe') if p.is_file()),
             None)


def env():
    """The environment for the build's commands: CLANG64 first on PATH."""
    if CLANG64 is None:
        raise SystemExit('clang not found: unpack the builder package (deps/clang64) or install MSYS2 CLANG64 '
                         '(see BUILDING.md), or set ULTRS_CLANG64 to its bin folder')
    return dict(os.environ, PATH=str(CLANG64) + os.pathsep + os.environ.get('PATH', ''))
