"""The build tools of a source build: what is missing on this PC is downloaded into deps/ from where it is published.

tools/deps.json pins every download (URL, size, hash); nothing of it is in git and nothing comes from an UltraTears
release. Each part is used from the PC when it is already there, else installed into deps/:
  - Python 3.14 with lz4, capstone and unicorn: the Python running setup.py when it is 3.14+ and has them, else
    deps/python (python.org's NuGet package, the wheels from PyPI unpacked into it; no pip, no compiler);
  - clang, lld, ninja and the libraries the port links (MSYS2 CLANG64): deps/clang64, ULTRS_CLANG64 or
    C:/msys64/clang64 when it has them, else deps/clang64 from repo.msys2.org;
  - the shader translator ultrs-shaders.exe: built from this source tree (tools/build_xlate.py) whenever deps/shaders
    holds none made from it (deps/shaders/source.txt), with the .NET 10 SDK on PATH or else one downloaded into
    deps/dotnet from Microsoft. A builder package carries a translator stamped with its own source.
deps/installed.json records what was installed from which pins: a changed pin installs that part again.
ULTRS_NO_SYSTEM_TOOLS=1 (testing what a clean PC gets): nothing installed on this PC is used.
"""
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import urllib.parse
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
MANIFEST = ROOT / 'tools/deps.json'
PACKAGES = ('lz4', 'capstone', 'unicorn')
# what the build needs from a CLANG64 folder (bin/..): programs, headers, libraries
CLANG64_NEEDS = ('bin/clang.exe', 'bin/clang++.exe', 'bin/ld.lld.exe', 'bin/ninja.exe', 'bin/llvm-windres.exe',
                 'include/SDL3/SDL.h', 'include/opus/opus.h', 'include/vpx/vpx_decoder.h', 'include/vulkan/vulkan.h',
                 'include/zstd.h', 'lib/libSDL3.a', 'lib/libopus.a', 'lib/libvpx.a')
SYSTEM = not os.environ.get('ULTRS_NO_SYSTEM_TOOLS')
XLATE_SOURCES = ('third_party/shader_translator', 'third_party/maxwell_shader', 'tools/build_xlate.py')


class DepsError(Exception):
    pass


def say(msg):
    print(msg, flush=True)


def manifest():
    return json.loads(MANIFEST.read_text(encoding='utf-8'))


def key_of(obj):   # what a part was installed from
    return hashlib.sha256(json.dumps(obj, sort_keys=True).encode()).hexdigest()[:16]


def installed(home):
    try:
        return json.loads((home / 'deps/installed.json').read_text())
    except (OSError, ValueError):
        return {}


def mark(home, part, key):
    s = installed(home)
    s[part] = key
    (home / 'deps').mkdir(exist_ok=True)
    (home / 'deps/installed.json').write_text(json.dumps(s, indent=1))


def wipe(home, sub):
    """Remove deps/<sub> of home before reinstalling it (only that folder, never anything outside deps/)."""
    d = (home / 'deps' / sub).resolve()
    if d.parent != (home / 'deps').resolve() or not d.exists():
        return
    shutil.rmtree(d)


# ---------------------------------------------------------------- downloads
def fetch(item, cache):
    """item's file in cache, downloaded unless a good copy is there; its size and hash checked."""
    name = urllib.parse.unquote(item['url'].rsplit('/', 1)[1])
    dst = cache / name
    if dst.is_file() and good(dst, item):
        return dst
    cache.mkdir(parents=True, exist_ok=True)
    part = dst.with_name(dst.name + '.part')
    req = urllib.request.Request(item['url'], headers={'User-Agent': 'UltraTears-setup'})
    try:
        with urllib.request.urlopen(req, timeout=120) as r, open(part, 'wb') as out:
            total = int(r.headers.get('Content-Length', 0)) or item.get('size', 0)
            done, shown = 0, -1
            while True:
                block = r.read(1 << 20)
                if not block:
                    break
                out.write(block)
                done += len(block)
                if total >= 32 << 20 and done // (64 << 20) != shown:
                    shown = done // (64 << 20)
                    say(f'    {name}: {done / 2**20:.0f} / {total / 2**20:.0f} MiB')
    except Exception as e:
        part.unlink(missing_ok=True)
        raise DepsError(f'could not download {item["url"]}: {e}') from e
    if not good(part, item):
        part.unlink(missing_ok=True)
        raise DepsError(f'{name} from {item["url"]} is not the pinned file (size or hash differs)')
    part.replace(dst)
    return dst


def good(path, item):
    if item.get('size') and path.stat().st_size != item['size']:
        return False
    algo = 'sha512' if 'sha512' in item else 'sha256'
    h = hashlib.new(algo)
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest().lower() == item[algo].lower()


# ---------------------------------------------------------------- Python
def python_ok(exe):
    """exe is Python 3.14+ with the build's packages."""
    probe = 'import sys; import ' + ', '.join(PACKAGES) + '; sys.exit(0 if sys.version_info >= (3, 14) else 1)'
    try:
        return subprocess.run([str(exe), '-c', probe], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                              env=dict(os.environ, PYTHONNOUSERSITE='1')).returncode == 0
    except OSError:
        return False


def python_for(home):
    """The Python the build runs on: deps/python when installed from the current pins, else this one when it will do,
    else None (install_python must run first)."""
    m = manifest()
    own = home / 'deps/python/python.exe'
    if own.is_file() and installed(home).get('python') in (key_of([m['python'], m['wheels']]), 'package'):
        return own
    if SYSTEM and python_ok(sys.executable):
        return pathlib.Path(sys.executable)
    return None


def install_python(home):
    """deps/python: python.org's interpreter with the wheels unpacked into it. Runs on any Python 3."""
    m = manifest()
    own = (home / 'deps/python').resolve()
    if pathlib.Path(sys.executable).resolve().is_relative_to(own):
        raise DepsError('deps\\python is outdated but is the Python running setup: start setup.py with another Python')
    cache = home / 'deps/downloads'
    say(f'  Python {m["python"]["version"]} with {", ".join(PACKAGES)} -> deps\\python')
    files = [fetch(m['python'], cache)] + [fetch(w, cache) for w in m['wheels']]
    wipe(home, 'python')
    with zipfile.ZipFile(files[0]) as z:   # the NuGet package: the interpreter is under tools/
        for info in z.infolist():
            if not info.filename.startswith('tools/') or info.is_dir():
                continue
            rel = pathlib.PurePosixPath(info.filename[len('tools/'):])
            if '..' in rel.parts:
                raise DepsError(f'unsafe path in {files[0].name}: {info.filename}')
            out = own / rel
            out.parent.mkdir(parents=True, exist_ok=True)
            with z.open(info) as src, open(out, 'wb') as dst:
                shutil.copyfileobj(src, dst)
    site = own / 'Lib/site-packages'
    for whl in files[1:]:   # a wheel is a zip of what goes into site-packages
        with zipfile.ZipFile(whl) as z:
            for info in z.infolist():
                rel = pathlib.PurePosixPath(info.filename)
                if '..' in rel.parts or rel.is_absolute():
                    raise DepsError(f'unsafe path in {whl.name}: {info.filename}')
            z.extractall(site)
    if not python_ok(own / 'python.exe'):
        raise DepsError('the installed deps\\python cannot import the build packages')
    mark(home, 'python', key_of([m['python'], m['wheels']]))
    for f in files:
        f.unlink(missing_ok=True)


# ---------------------------------------------------------------- CLANG64
def clang64_ok(prefix):
    return prefix is not None and all((prefix / n).is_file() for n in CLANG64_NEEDS)


def clang64_for(home):
    """The CLANG64 folder to build with (its bin/..), or None."""
    m = manifest()
    own = home / 'deps/clang64'
    if installed(home).get('clang64') in (key_of(m['msys2']), 'package') and (own / 'bin/clang.exe').is_file():
        return own
    for c in (os.environ.get('ULTRS_CLANG64'), 'C:/msys64/clang64/bin') if SYSTEM else ():
        if c and clang64_ok(pathlib.Path(c).parent):
            return pathlib.Path(c).parent
    return None


def install_clang64(home):
    """deps/clang64 from the MSYS2 packages (needs Python 3.14: tarfile reads .tar.zst)."""
    import tarfile
    m = manifest()
    cache = home / 'deps/downloads'
    total = sum(p['size'] for p in m['msys2'])
    say(f'  clang / lld / ninja and the libraries ({len(m["msys2"])} MSYS2 packages, {total / 2**20:.0f} MiB) -> deps\\clang64')
    files = [fetch(p, cache) for p in m['msys2']]
    wipe(home, 'clang64')
    own = home / 'deps/clang64'
    own.mkdir(parents=True)
    def where(name):   # clang64/<rel> -> its place in deps/clang64 (None: package metadata, .PKGINFO etc.)
        if not name.startswith('clang64/') or name == 'clang64/':
            return None
        rel = pathlib.PurePosixPath(name[len('clang64/'):])
        if rel.is_absolute() or '..' in rel.parts:
            raise DepsError(f'unsafe path in an MSYS2 package: {name}')
        return own / rel
    for f in files:
        with tarfile.open(f, 'r:zst') as t:
            for info in t:
                dst = where(info.name)
                if dst is None:
                    continue
                if info.isdir():
                    dst.mkdir(parents=True, exist_ok=True)
                elif info.isfile():
                    dst.parent.mkdir(parents=True, exist_ok=True)
                    with t.extractfile(info) as src, open(dst, 'wb') as out:
                        shutil.copyfileobj(src, out)
                elif info.islnk() or info.issym():   # hard links (clang.exe / clang++.exe ...): a copy of the target
                    target = where(info.linkname) if info.islnk() else dst.parent / info.linkname
                    if target is None or not target.resolve().is_relative_to(own.resolve()):
                        raise DepsError(f'unsafe link in {f.name}: {info.name} -> {info.linkname}')
                    if target.is_file():
                        dst.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(target, dst)
    if not clang64_ok(own):
        raise DepsError('the installed deps\\clang64 lacks part of the toolchain')
    mark(home, 'clang64', key_of(m['msys2']))
    for f in files:
        f.unlink(missing_ok=True)


# ---------------------------------------------------------------- shader translator
def xlate_stamp(root=ROOT):
    """The sources the translator is built from (tools/build_xlate.py writes this next to it)."""
    h = hashlib.sha256()
    files = []
    for s in XLATE_SOURCES:
        p = root / s
        files += [p] if p.is_file() else [f for f in p.rglob('*') if f.is_file() and not {'bin', 'obj'} & set(f.relative_to(p).parts)]
    for f in sorted(files):
        h.update(f.relative_to(root).as_posix().encode() + b'\0' + f.read_bytes().replace(b'\r\n', b'\n'))
    return h.hexdigest()[:16]


def xlate_ok(home):
    exe, stamp = home / 'deps/shaders/ultrs-shaders.exe', home / 'deps/shaders/source.txt'
    return exe.is_file() and stamp.is_file() and stamp.read_text().strip() == xlate_stamp(home)


def system_dotnet():
    if not SYSTEM:
        return None
    try:
        r = subprocess.run(['dotnet', '--list-sdks'], capture_output=True, text=True)
    except OSError:
        return None
    return 'dotnet' if r.returncode == 0 and any(l.split('.')[0].strip().isdigit() and int(l.split('.')[0]) >= 10
                                                for l in r.stdout.splitlines()) else None


def build_xlate(home):
    m = manifest()
    dotnet = system_dotnet()
    if dotnet is None:
        own = home / 'deps/dotnet'
        if installed(home).get('dotnet') != key_of(m['dotnet']) or not (own / 'dotnet.exe').is_file():
            say(f'  .NET SDK {m["dotnet"]["version"]} (to build the shader translator) -> deps\\dotnet')
            z = fetch(m['dotnet'], home / 'deps/downloads')
            wipe(home, 'dotnet')
            with zipfile.ZipFile(z) as zf:
                zf.extractall(own)
            mark(home, 'dotnet', key_of(m['dotnet']))
            z.unlink(missing_ok=True)
        dotnet = str(own / 'dotnet.exe')
    say('  building the shader translator from this source -> deps\\shaders')
    rc = subprocess.run([sys.executable, str(home / 'tools/build_xlate.py'), '--dotnet', dotnet, '--out', str(home / 'deps/shaders'),
                         '--cache', str(home / 'deps/dotnet-cache')], cwd=home).returncode
    if rc or not xlate_ok(home):
        raise DepsError('building the shader translator failed; the lines above say why')


# ---------------------------------------------------------------- entry points
def ensure_tools(home):
    """Run on the build's Python (3.14+): CLANG64 and the translator, installed when missing."""
    if clang64_for(home) is None:
        install_clang64(home)
    if not xlate_ok(home):
        build_xlate(home)


def ready(home):
    return python_for(home) is not None and clang64_for(home) is not None and xlate_ok(home)
