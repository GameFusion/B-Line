# macOS Bundle, Sign, Notarize, and Publish (Qt 6.10, Apple Silicon)

## Goal
Create a reproducible macOS release flow for B-Line:
1. Compile the macOS Release app bundle (`Boarder.app`).
2. Build a distributable app bundle (`B-Line.app`) with Qt/runtime dependencies deployed.
3. Sign all nested binaries and the app bundle with a Developer ID Application certificate.
4. Build a DMG installer and notarize it with Apple.
5. Validate the notarized DMG locally.
6. Publish the final DMG into the SyncPipeline B-Line Early Access release surface.

## Packaging Files
1. macOS build/sign/notarize script:
   - `build-vs2019-qt6/build_and_notarize_boarder_macos.py`
2. Project plist used for bundle metadata:
   - `build-vs2019-qt6/Info.plist`
3. qmake project file:
   - `build-vs2019-qt6/Boarder.pro`
4. SyncPipeline publish script:
   - `/Users/andreascarlen/StargitStudio/SyncPipeline/scripts/publish_tool_release.py`
5. SyncPipeline release runbook:
   - `/Users/andreascarlen/StargitStudio/SyncPipeline/docs/releases/b-line-macos-notarized-release-runbook.md`

## Current macOS App Metadata
Verified current shipping identifiers:
1. Product display name: `B-Line`
2. Code/app target name: `Boarder`
3. Bundle identifier: `com.stargitstudio.bline`
4. DMG volume name: `B-Line Installer`

## Prerequisites
1. macOS build machine with Qt macOS kit installed.
   - The script auto-detects a kit from `~/Qt/*/macos`
   - You can also override with:
     - `QT_ROOT`
     - `QT_DIR`
2. Xcode command line tools available.
3. Apple Developer account with:
   - Developer ID Application certificate installed in Keychain
   - notarization key material available for `notarytool`
4. `macdeployqt` available from the selected Qt kit.
5. `codesign`, `xcrun`, `hdiutil`, `spctl`, `xattr` available on the machine.
6. For website publish:
   - access to the production SyncPipeline server
   - SyncPipeline production environment configured
   - object storage credentials already configured on the server side

## Environment Variables
The script supports these overrides:

1. `BOARDER_SIGN_IDENTITY`
   - defaults to:
   - `Developer ID Application: GameFusion SAS (W9Y52TA3WD)`
2. `BOARDER_NOTARY_KEY_PATH`
3. `BOARDER_NOTARY_KEY_ID`
4. `BOARDER_NOTARY_ISSUER`
5. `BOARDER_NOTARY_TEAM_ID`
6. `BOARDER_STORAGE_PREFIX`
   - defaults to:
   - `Applications/B-Line`
7. `QT_ROOT` or `QT_DIR`
   - to force a specific Qt macOS kit

## Versioning Best Practice
1. Keep semantic versioning in `build-vs2019-qt6/Boarder.pro`.
2. Keep the bundle identifier stable once public distribution begins.
3. Include release date/time in the generated DMG filename.
4. Use a release label in SyncPipeline such as:
   - `0.9.0-early-access`
   - `0.9.1-early-access`
5. Prefer a new release label for new public drops rather than repeatedly replacing the same one.

## Release Pipeline (Step-by-Step)
Run from repo root:
- `/Users/andreascarlen/GameFusion/Applications/Boarder`

### 1. Bump version if needed
Edit:
- `build-vs2019-qt6/Boarder.pro`

Example:
```qmake
VERSION = 1.0.1
```

### 2. Run the macOS release script
```bash
python3 build-vs2019-qt6/build_and_notarize_boarder_macos.py
```

What this does:
1. Detects the Qt macOS kit
2. Regenerates the Release makefiles
3. Builds the Release app
4. Verifies the bundle identifier
5. Runs `macdeployqt`
6. Clears extended attributes
7. Copies `Boarder.app` to `B-Line.app`
8. Signs nested frameworks, plugins, dylibs, main binary, and app bundle
9. Builds a DMG
10. Signs the DMG
11. Submits the DMG to Apple notarization
12. Staples the notarization ticket
13. Verifies Gatekeeper acceptance

### 3. Expected output artifacts
Generated in:
- `build-vs2019-qt6/`

Expected outputs:
1. Signed app bundle:
   - `build-vs2019-qt6/B-Line.app`
2. Notarized DMG:
   - `build-vs2019-qt6/B-Line-<YYYY-MM-DD_HH-MM>-macOS-arm64.dmg`

## Validation Commands
Run these after packaging:

### 1. Verify signed app bundle
```bash
codesign --verify --deep --strict --verbose=2 build-vs2019-qt6/B-Line.app
spctl -a -vvv build-vs2019-qt6/B-Line.app
```

### 2. Verify notarized DMG
```bash
spctl -a -t open --context context:primary-signature -vvv build-vs2019-qt6/B-Line-<TIMESTAMP>-macOS-arm64.dmg
```

Expected result:
1. app accepted by `codesign`
2. app accepted by `spctl`
3. DMG accepted by `spctl`
4. notarized DMG should report:
   - `source=Notarized Developer ID`

## Optional Storage-Only Upload
The script can upload the final DMG to object storage without updating website/database records:

```bash
python3 build-vs2019-qt6/build_and_notarize_boarder_macos.py --upload-storage
```

Important:
1. this uses the existing GitExplorer S3 helper
2. this is storage-only
3. this does **not** publish to SyncPipeline website/database

## Skip Modes
### 1. Reuse existing Release build output
```bash
python3 build-vs2019-qt6/build_and_notarize_boarder_macos.py --skip-build
```

### 2. Build/package without Apple notarization
```bash
python3 build-vs2019-qt6/build_and_notarize_boarder_macos.py --skip-notarize
```

Use `--skip-notarize` only for internal iteration or debugging.  
Do not use it for public download distribution.

## Publish to SyncPipeline
The Boarder script stops after build/sign/notarize plus optional storage upload.

To publish the macOS release to the live B-Line page on SyncPipeline:
1. prepare a SyncPipeline release manifest
2. point the manifest at the final `.dmg` file, not the `.app`
3. use the SyncPipeline publish script on the production server

Reference runbook:
- `/Users/andreascarlen/StargitStudio/SyncPipeline/docs/releases/b-line-macos-notarized-release-runbook.md`

Key point:
1. if the manifest points to an `.app`, SyncPipeline will repackage it to `.zip`
2. if the manifest points to a `.dmg`, SyncPipeline will preserve the notarized DMG

## Publish Model (High-Level)
1. Build and notarize in the Boarder repo
2. Copy the final DMG to the production SyncPipeline server
3. Create/update the release manifest for tool slug:
   - `b-line-boarder`
4. Dry-run the SyncPipeline publish script
5. Apply publish to update:
   - `tool_releases`
   - `tool_release_artifacts`
   - `tool_release_visuals`
6. Verify:
   - SyncPipeline tool page
   - public Spaces URL
   - package format shown as `dmg`

## Troubleshooting
### 1. `No Qt macOS kit found`
Set:
```bash
export QT_ROOT=/Users/<user>/Qt/<version>/macos
```

### 2. Bundle identifier mismatch
Check:
1. `build-vs2019-qt6/Info.plist`
2. `build-vs2019-qt6/Boarder.pro`
3. generated app plist inside the built app bundle

### 3. `qyieldcpu.h` / `__yield` build failure
This project already carries a qmake workaround in:
- `build-vs2019-qt6/Boarder.pro`

If this reappears, verify that qmake was rerun and the generated Makefile is current.

### 4. Notary key file not found
Set:
```bash
export BOARDER_NOTARY_KEY_PATH=/absolute/path/to/AuthKey_<KEYID>.p8
```

### 5. App validates but DMG does not
Check:
1. DMG was signed
2. notarization submit completed successfully
3. stapling completed
4. validation used:
```bash
spctl -a -t open --context context:primary-signature -vvv <dmg>
```

### 6. SyncPipeline still shows old `Boarder.zip`
Possible causes:
1. publish was run only against local/dev SyncPipeline DB
2. production publish was not applied
3. manifest pointed to an `.app` instead of the final `.dmg`
4. old object still exists in Spaces even though the DB row changed

### 7. Old artifact remains in Spaces after replacement
`--replace-existing` on the SyncPipeline side replaces database rows, but does not remove the previous object from object storage.

## Recommended Operational Policy
1. Use notarized DMG for all macOS external distribution.
2. Keep the Boarder build/sign/notarize flow and the SyncPipeline publish flow documented separately.
3. Treat object storage upload and website/database publish as separate steps.
4. Prefer new release labels for externally visible releases rather than repeatedly mutating the same public version.
5. Record every public macOS release with:
   - version label
   - DMG filename
   - bundle identifier
   - signing identity
   - publish timestamp
   - public download URL

## Suggested Companion Docs
1. Windows:
   - `docs/AI/06_Windows_Bundle_and_Installer.md`
2. SyncPipeline release publish:
   - `/Users/andreascarlen/StargitStudio/SyncPipeline/docs/releases/b-line-macos-notarized-release-runbook.md`
