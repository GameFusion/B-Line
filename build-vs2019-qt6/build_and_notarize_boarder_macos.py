#!/usr/bin/env python3
import argparse
import datetime as dt
import importlib.util
import math
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent

PRODUCT_DISPLAY_NAME = "B-Line"
PRODUCT_CODE_NAME = "Boarder"
SOURCE_APP_NAME = f"{PRODUCT_CODE_NAME}.app"
DIST_APP_NAME = f"{PRODUCT_DISPLAY_NAME}.app"
DMG_VOLUME_NAME = f"{PRODUCT_DISPLAY_NAME} Installer"
BUNDLE_ID = "com.stargitstudio.bline"

SIGN_IDENTITY = os.environ.get(
    "BOARDER_SIGN_IDENTITY",
    "Developer ID Application: GameFusion SAS (W9Y52TA3WD)",
)
NOTARY_KEY_PATH = Path(
    os.environ.get(
        "BOARDER_NOTARY_KEY_PATH",
        "/Users/andreascarlen/GameFusion/Applications/GitExplorer/build-vs2019-qt6/AuthKey_A4VWY4L3AA.p8",
    )
).expanduser()
NOTARY_KEY_ID = os.environ.get("BOARDER_NOTARY_KEY_ID", "A4VWY4L3AA")
NOTARY_ISSUER = os.environ.get("BOARDER_NOTARY_ISSUER", "69a6de6f-141f-47e3-e053-5b8c7c11a4d1")
NOTARY_TEAM_ID = os.environ.get("BOARDER_NOTARY_TEAM_ID", "W9Y52TA3WD")
STORAGE_PREFIX = os.environ.get("BOARDER_STORAGE_PREFIX", "Applications/B-Line")


def run(cmd, cwd=None, capture=False):
    print(f"Running: {' '.join(str(part) for part in cmd)}")
    if capture:
        completed = subprocess.run(
            [str(part) for part in cmd],
            cwd=str(cwd) if cwd else None,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if completed.stdout:
            print(completed.stdout, end="")
        return completed.stdout

    process = subprocess.Popen(
        [str(part) for part in cmd],
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    assert process.stdout is not None
    for line in process.stdout:
        print(line, end="")
    return_code = process.wait()
    if return_code != 0:
        raise RuntimeError(f"Command failed with exit code {return_code}: {' '.join(str(part) for part in cmd)}")


def version_key(path_string):
    digits = []
    for part in path_string.replace("Qt_", "").replace("_for_macOS-Release", "").replace("_", ".").split("."):
        if part.isdigit():
            digits.append(int(part))
    return tuple(digits)


def find_qt_root():
    candidates = []
    for key in ("QT_ROOT", "QT_DIR"):
        value = os.environ.get(key)
        if value:
            candidates.append(Path(value).expanduser())

    qt_home = Path.home() / "Qt"
    if qt_home.is_dir():
        candidates.extend(qt_home.glob("*/macos"))

    usable = []
    for candidate in candidates:
        resolved = candidate.resolve()
        if (resolved / "bin" / "qmake").exists() and (resolved / "bin" / "macdeployqt").exists():
            usable.append(resolved)

    if not usable:
        raise FileNotFoundError("No Qt macOS kit found. Set QT_ROOT or QT_DIR.")

    usable = sorted(set(usable), key=lambda path: version_key(path.parent.name), reverse=True)
    return usable[0]


def release_build_dir(qt_root):
    build_slug = f"Qt_{qt_root.parent.name.replace('.', '_')}_for_macOS-Release"
    return SCRIPT_DIR / "build" / build_slug


def source_app_path(qt_root):
    return release_build_dir(qt_root) / SOURCE_APP_NAME


def ensure_release_build(qt_root, jobs):
    build_dir = release_build_dir(qt_root)
    build_dir.mkdir(parents=True, exist_ok=True)
    run(["rm", "-f", ".qmake.stash"], cwd=build_dir)
    run(
        [
            qt_root / "bin" / "qmake",
            "-o",
            "Makefile",
            "../../Boarder.pro",
            "-spec",
            "macx-clang",
            "CONFIG+=release",
        ],
        cwd=build_dir,
    )
    run(["make", f"-j{jobs}"], cwd=build_dir)
    run(["make", "-B", f"{SOURCE_APP_NAME}/Contents/Info.plist"], cwd=build_dir)


def verify_bundle_id(app_path):
    bundle_id = run(
        ["/usr/libexec/PlistBuddy", "-c", "Print :CFBundleIdentifier", app_path / "Contents" / "Info.plist"],
        capture=True,
    ).strip()
    if bundle_id != BUNDLE_ID:
        raise RuntimeError(f"Bundle identifier mismatch. Expected {BUNDLE_ID}, got {bundle_id}")


def remove_stale_qt_bundle_artifacts(app_path):
    frameworks_dir = app_path / "Contents" / "Frameworks"
    plugins_dir = app_path / "Contents" / "PlugIns"
    qt_conf_path = app_path / "Contents" / "Resources" / "qt.conf"

    if frameworks_dir.is_dir():
        for entry in frameworks_dir.iterdir():
            if entry.name.startswith("Qt") and entry.suffix == ".framework":
                shutil.rmtree(entry)

    if plugins_dir.exists():
        shutil.rmtree(plugins_dir)

    if qt_conf_path.exists():
        qt_conf_path.unlink()


def is_macho(file_path):
    try:
        output = run(["file", str(file_path)], capture=True)
        return "Mach-O" in output
    except Exception:
        return False


def sign_path(path):
    run(
        [
            "codesign",
            "--force",
            "--verify",
            "--verbose",
            "--timestamp",
            "--sign",
            SIGN_IDENTITY,
            "--options",
            "runtime",
            str(path),
        ]
    )


def sign_bundle_contents(app_path):
    frameworks_dir = app_path / "Contents" / "Frameworks"
    plugins_dir = app_path / "Contents" / "PlugIns"
    macos_dir = app_path / "Contents" / "MacOS"

    if frameworks_dir.is_dir():
        for file_path in sorted(frameworks_dir.rglob("*")):
            if file_path.is_file() and is_macho(file_path):
                sign_path(file_path)

        for framework in sorted(frameworks_dir.glob("*.framework")):
            sign_path(framework)

    if plugins_dir.is_dir():
        for file_path in sorted(plugins_dir.rglob("*")):
            if file_path.is_file() and is_macho(file_path):
                sign_path(file_path)

    if (app_path / "Contents" / "Resources").is_dir():
        for file_path in sorted((app_path / "Contents" / "Resources").rglob("*")):
            if file_path.is_file() and file_path.suffix == ".dylib":
                sign_path(file_path)

    main_binary = macos_dir / PRODUCT_CODE_NAME
    sign_path(main_binary)

    sign_path(app_path)


def verify_signed_bundle(app_path):
    run(["codesign", "--verify", "--deep", "--strict", "--verbose=2", str(app_path)])
    run(["spctl", "-a", "-vvv", str(app_path)])


def directory_size_bytes(path):
    total = 0
    for root, _, files in os.walk(path):
        for file_name in files:
            file_path = Path(root) / file_name
            try:
                total += file_path.stat().st_size
            except OSError:
                pass
    return total


def estimate_rw_dmg_size_mb(app_dir):
    app_size = directory_size_bytes(app_dir)
    headroom = 256 * 1024 * 1024
    return max(512, math.ceil((app_size + headroom) / (1024 * 1024)))


def attach_rw_dmg(rw_dmg_path):
    output = run(
        [
            "hdiutil",
            "attach",
            "-readwrite",
            "-noverify",
            "-noautoopen",
            str(rw_dmg_path),
        ],
        capture=True,
    )

    device = None
    mount_point = None
    for line in output.splitlines():
        if line.startswith("/dev/") and "/Volumes/" in line:
            fields = [field.strip() for field in line.split("\t") if field.strip()]
            if len(fields) >= 3:
                device = fields[0]
                mount_point = fields[-1]
                break
    if not device or not mount_point:
        raise RuntimeError("Failed to parse hdiutil attach output.")
    return device, Path(mount_point)


def build_dmg(dist_app_path, artifact_stem, output_dir):
    staging_dir = output_dir / f"{artifact_stem}-staging"
    rw_dmg_path = output_dir / f"{artifact_stem}-rw.dmg"
    final_dmg_path = output_dir / f"{artifact_stem}.dmg"

    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    if rw_dmg_path.exists():
        rw_dmg_path.unlink()
    if final_dmg_path.exists():
        final_dmg_path.unlink()

    staging_dir.mkdir(parents=True, exist_ok=True)

    rw_size_mb = estimate_rw_dmg_size_mb(dist_app_path)
    run(
        [
            "hdiutil",
            "create",
            "-size",
            f"{rw_size_mb}m",
            "-volname",
            DMG_VOLUME_NAME,
            "-ov",
            str(rw_dmg_path),
            "-fs",
            "HFS+",
        ]
    )

    device = None
    try:
        device, mount_point = attach_rw_dmg(rw_dmg_path)
        run(["ditto", str(dist_app_path), str(mount_point / DIST_APP_NAME)])
        applications_link = mount_point / "Applications"
        if applications_link.exists() or applications_link.is_symlink():
            applications_link.unlink()
        applications_link.symlink_to("/Applications")
        run(["sync"])
    finally:
        if device:
            run(["hdiutil", "detach", device])

    run(
        [
            "hdiutil",
            "convert",
            str(rw_dmg_path),
            "-format",
            "UDZO",
            "-imagekey",
            "zlib-level=9",
            "-o",
            str(final_dmg_path),
        ]
    )
    rw_dmg_path.unlink(missing_ok=True)
    shutil.rmtree(staging_dir, ignore_errors=True)
    return final_dmg_path


def notarize_and_staple(dmg_path):
    if not NOTARY_KEY_PATH.is_file():
        raise FileNotFoundError(f"Notary key file not found: {NOTARY_KEY_PATH}")

    run(["codesign", "--sign", SIGN_IDENTITY, str(dmg_path)])
    run(
        [
            "xcrun",
            "notarytool",
            "submit",
            str(dmg_path),
            "--key",
            str(NOTARY_KEY_PATH),
            "--key-id",
            NOTARY_KEY_ID,
            "--issuer",
            NOTARY_ISSUER,
            "--team-id",
            NOTARY_TEAM_ID,
            "--wait",
        ]
    )
    run(["xcrun", "stapler", "staple", str(dmg_path)])
    run(["xcrun", "stapler", "validate", str(dmg_path)])
    run(["spctl", "-a", "-t", "open", "--context", "context:primary-signature", "-vvv", str(dmg_path)])


def import_stargit_s3():
    s3_path = Path("/Users/andreascarlen/GameFusion/Applications/GitExplorer/S3.py")
    if not s3_path.is_file():
        raise FileNotFoundError(f"S3 helper not found: {s3_path}")
    spec = importlib.util.spec_from_file_location("stargit_s3", s3_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Failed to load GitExplorer S3 helper.")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def upload_storage_only(dmg_path):
    s3 = import_stargit_s3()
    destination = f"{STORAGE_PREFIX}/{dmg_path.name}"
    return s3.upload_file(str(dmg_path), destination)


def main():
    parser = argparse.ArgumentParser(description="Build, sign, notarize, and optionally upload the B-Line macOS release.")
    parser.add_argument("--skip-build", action="store_true", help="Reuse the existing release build output.")
    parser.add_argument("--skip-notarize", action="store_true", help="Build/sign/package without Apple notarization.")
    parser.add_argument("--upload-storage", action="store_true", help="Upload the final DMG to object storage using the existing GitExplorer S3 helper.")
    parser.add_argument("--jobs", type=int, default=4, help="Parallel make jobs to use for the release build.")
    args = parser.parse_args()

    qt_root = find_qt_root()
    build_dir = release_build_dir(qt_root)
    app_path = source_app_path(qt_root)

    if not args.skip_build:
        ensure_release_build(qt_root, args.jobs)

    if not app_path.exists():
        raise FileNotFoundError(f"Release app not found: {app_path}")

    verify_bundle_id(app_path)
    remove_stale_qt_bundle_artifacts(app_path)
    run([qt_root / "bin" / "macdeployqt", app_path])
    run(["xattr", "-cr", str(app_path)])
    verify_bundle_id(app_path)

    timestamp = dt.datetime.now().strftime("%Y-%m-%d_%H-%M")
    artifact_stem = f"{PRODUCT_DISPLAY_NAME}-{timestamp}-macOS-arm64"
    output_dir = SCRIPT_DIR
    dist_app_path = output_dir / DIST_APP_NAME

    if dist_app_path.exists():
        shutil.rmtree(dist_app_path)
    run(["ditto", str(app_path), str(dist_app_path)])
    run(["xattr", "-cr", str(dist_app_path)])

    sign_bundle_contents(dist_app_path)
    verify_signed_bundle(dist_app_path)

    dmg_path = build_dmg(dist_app_path, artifact_stem, output_dir)
    if args.skip_notarize:
        run(["codesign", "--sign", SIGN_IDENTITY, str(dmg_path)])
    else:
        notarize_and_staple(dmg_path)

    upload_url = ""
    if args.upload_storage:
        upload_url = upload_storage_only(dmg_path)

    print("\nRelease complete")
    print(f"Qt root: {qt_root}")
    print(f"Release build: {app_path}")
    print(f"Signed app: {dist_app_path}")
    print(f"DMG: {dmg_path}")
    if upload_url:
        print(f"Storage URL: {upload_url}")
    else:
        print("Storage upload: skipped")
    print("Website/database publish: not performed by this script.")


if __name__ == "__main__":
    main()
