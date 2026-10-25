import argparse
import hashlib
import json
import msvcrt
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile

SOURCE_COMMIT = "29e619e767cde9045a75c29bc9a8278ae7b3a98b"
SOURCE_URL = "https://github.com/FFmpeg/FFmpeg/archive/" + SOURCE_COMMIT + ".tar.gz"
SOURCE_HASH = "8e4699bcb6a311772e8f513cc72ce27328891b82a598b82e7cff3dfe580875a0"
MAKE_URL = "https://repo.msys2.org/msys/x86_64/make-4.4.1-3-x86_64.pkg.tar.zst"
MAKE_HASH = "af0bdba17f06fe037f0194069adaa31a8fe45f1a11381501896aea1fae37bd5d"
NASM_URL = "https://www.nasm.us/pub/nasm/releasebuilds/3.02/win64/nasm-3.02-win64.zip"
NASM_HASH = "161d0bfaff53c2f9e9f3e69fd0672323ebabafd1268976a5cec11be92a19aee7"
COMPONENTS = {"avformat": 62, "avcodec": 62, "avutil": 60, "swscale": 9, "swresample": 6}
CONFIGURE = [
    "--toolchain=msvc", "--arch=x86_64", "--target-os=win64", "--enable-shared", "--disable-static",
    "--disable-everything", "--disable-autodetect", "--disable-programs", "--disable-doc",
    "--disable-network", "--disable-avdevice", "--disable-avfilter", "--enable-demuxer=bink",
    "--enable-decoder=bink,binkaudio_rdft,binkaudio_dct", "--enable-protocol=file", "--disable-debug",
    "--disable-vulkan", "--disable-d3d11va", "--disable-dxva2", "--disable-mediafoundation",
    "--disable-cuda-llvm", "--disable-hwaccels",
]


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def download(path, url, expected):
    if path.exists() and digest(path) == expected:
        return
    temporary = path.with_suffix(path.suffix + ".download")
    print("Downloading " + url, flush=True)
    with urllib.request.urlopen(url, timeout=120) as response, temporary.open("wb") as output:
        shutil.copyfileobj(response, output)
    if digest(temporary) != expected:
        raise RuntimeError("SHA256 mismatch for " + url)
    temporary.replace(path)


def compiler_environment(work):
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    installation = subprocess.check_output([
        str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "-property", "installationPath",
    ], text=True).strip()
    if not installation:
        raise RuntimeError("Visual C++ x64 tools were not found")
    command = work / "compiler-env.cmd"
    developer_command = Path(installation) / "Common7/Tools/VsDevCmd.bat"
    command.write_text('@echo off\ncall "' + str(developer_command).replace('%', '%%') + '" -no_logo -arch=amd64 -host_arch=amd64 >nul\nif errorlevel 1 exit /b 1\nset\n')
    result = subprocess.run(["cmd.exe", "/d", "/c", str(command)], capture_output=True, text=True, check=True)
    environment = dict(os.environ)
    for line in result.stdout.splitlines():
        if "=" not in line or line.startswith("="):
            continue
        key, value = line.split("=", 1)
        if key.lower() == "path":
            for existing in list(environment):
                if existing.lower() == "path":
                    del environment[existing]
            key = "PATH"
        environment[key] = value
    return environment


def valid_sdk(sdk, signature):
    manifest_path = sdk / "arcanum-build.json"
    if not manifest_path.exists():
        return False
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if manifest["signature"] != signature:
            return False
        for relative, expected in manifest["files"].items():
            path = sdk / relative
            if not path.is_file() or digest(path) != expected["sha256"]:
                return False
        for component, major in COMPONENTS.items():
            if not (sdk / "lib" / (component + ".lib")).exists():
                return False
        return (sdk / "include/libavcodec/avcodec.h").exists()
    except (ValueError, KeyError, OSError):
        return False


def build(options):
    work = options.work_directory.resolve()
    sdk = options.output_directory.resolve()
    work.mkdir(parents=True, exist_ok=True)
    with (work / "build.lock").open("a+b") as lock:
        lock.seek(0)
        if not lock.read(1):
            lock.write(b"0")
            lock.flush()
        deadline = time.monotonic() + 1800
        while True:
            try:
                lock.seek(0)
                msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
                break
            except OSError:
                if time.monotonic() >= deadline:
                    raise RuntimeError("Timed out waiting for the FFmpeg build lock")
                time.sleep(1)
        environment = compiler_environment(work)
        recipe = {"source": SOURCE_COMMIT, "source_sha256": SOURCE_HASH, "configure": CONFIGURE,
            "compiler": environment["VCToolsVersion"], "make_sha256": MAKE_HASH, "nasm_sha256": NASM_HASH,
            "builder_sha256": digest(Path(__file__))}
        signature = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
        if valid_sdk(sdk, signature):
            print("Minimal FFmpeg SDK is up to date: " + str(sdk), flush=True)
            return
        git = Path(shutil.which("git") or "")
        bash = git.parent.parent / "bin/bash.exe"
        if not bash.exists():
            raise RuntimeError("Git for Windows bash.exe was not found beside git.exe")
        downloads = work / "downloads"
        downloads.mkdir(exist_ok=True)
        source_archive = downloads / "ffmpeg.tar.gz"
        make_archive = downloads / "make.pkg.tar.zst"
        nasm_archive = downloads / "nasm.zip"
        download(source_archive, SOURCE_URL, SOURCE_HASH)
        download(make_archive, MAKE_URL, MAKE_HASH)
        download(nasm_archive, NASM_URL, NASM_HASH)
        tools = work / "tools"
        tools.mkdir(exist_ok=True)
        subprocess.run([str(options.cmake), "-E", "tar", "xf", str(make_archive), "--", "usr/bin/make.exe"], cwd=tools, check=True)
        with zipfile.ZipFile(nasm_archive) as archive:
            archive.extractall(tools)
        source_parent = work / "source"
        source_parent.mkdir(exist_ok=True)
        source = source_parent / ("FFmpeg-" + SOURCE_COMMIT)
        if not (source / "configure").exists():
            subprocess.run([str(options.cmake), "-E", "tar", "xf", str(source_archive)], cwd=source_parent, check=True)
        directory = work / ("build-" + signature[:12])
        directory.mkdir(exist_ok=True)
        sdk.mkdir(parents=True, exist_ok=True)
        environment.update(ARCANUM_FFMPEG_MSVC=str(Path(environment["VCToolsInstallDir"]) / "bin/Hostx64/x64"),
            ARCANUM_FFMPEG_SOURCE=str(source), ARCANUM_FFMPEG_BUILD=str(directory), ARCANUM_FFMPEG_SDK=str(sdk))
        environment["PATH"] = str(tools / "usr/bin") + ";" + str(tools / "nasm-3.02") + ";" + str(git.parent.parent / "usr/bin") + ";" + environment["PATH"]
        script = '\n'.join([
            'set -eu',
            'export PATH="$(cygpath -u "$ARCANUM_FFMPEG_MSVC"):$PATH"',
            'cd "$(cygpath -u "$ARCANUM_FFMPEG_BUILD")"',
            '"$(cygpath -u "$ARCANUM_FFMPEG_SOURCE")/configure" ' + ' '.join(CONFIGURE)
                + ' --prefix="$(cygpath -m "$ARCANUM_FFMPEG_SDK")" --shlibdir="$(cygpath -m "$ARCANUM_FFMPEG_SDK")/bin"',
            'make -j' + str(options.jobs),
            'make install',
        ])
        log_path = work / "build.log"
        print("Building Bink-only FFmpeg; log: " + str(log_path), flush=True)
        with log_path.open("wb") as log:
            result = subprocess.run([str(bash), "--noprofile", "--norc", "-c", script], env=environment, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print('\n'.join(log_path.read_text(errors="replace").splitlines()[-30:]), file=sys.stderr)
            raise RuntimeError("FFmpeg compilation failed; see " + str(log_path))
        for component in COMPONENTS:
            import_library = sdk / "bin" / (component + ".lib")
            (sdk / "lib").mkdir(exist_ok=True)
            shutil.copy2(import_library, sdk / "lib" / import_library.name)
        shutil.copy2(source / "COPYING.LGPLv2.1", sdk / "LICENSE.txt")
        shutil.copy2(Path(__file__).resolve().parents[1] / "third_party/ffmpeg/NOTICE.txt", sdk / "NOTICE.txt")
        (sdk / "BUILD-CONFIG.txt").write_text(json.dumps(recipe, indent=2) + "\n", encoding="utf-8")
        files = {}
        for component, major in COMPONENTS.items():
            relative = "bin/" + component + "-" + str(major) + ".dll"
            path = sdk / relative
            files[relative] = {"sha256": digest(path), "bytes": path.stat().st_size}
        for name in ["LICENSE.txt", "NOTICE.txt", "BUILD-CONFIG.txt"]:
            path = sdk / name
            files[name] = {"sha256": digest(path), "bytes": path.stat().st_size}
        manifest = dict(recipe, signature=signature, files=files)
        (sdk / "arcanum-build.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print("Minimal FFmpeg libraries: " + str(sum(files[name]["bytes"] for name in files if name.endswith(".dll"))) + " bytes", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--work-directory", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=8)
    options = parser.parse_args()
    if os.name != "nt" or options.jobs < 1:
        parser.error("This builder requires Windows and a positive job count")
    build(options)


if __name__ == "__main__":
    main()
