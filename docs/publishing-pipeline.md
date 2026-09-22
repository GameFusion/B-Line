# Windows packaging and SyncPipeline publication

Publication has two separate catalogs. The legacy `Applications.Deploy` MySQL
table uses `Name = B-Line` and `Platform = Windows`; updating it does **not**
update SyncPipeline Tools. Artifacts live under `Applications/B-Line/` in the
established `fusion` DigitalOcean Space.

The portal at <https://syncpipeline.com/tools> requires a website login and reads
PostgreSQL `tool_releases` and `tool_release_artifacts`, using tool slug
`b-line-boarder`. Its source and publisher are in
<https://github.com/StargitStudio/SyncPipeline>,
`scripts/publish_tool_release.py`. A Windows release is not fully published until
this catalog and the actual Tools download link are verified.

## Package the tested build

First build the shared libraries and B-Line with a consistent MSVC toolset,
run the smoke tests in `tests/README.windows.md`, and deploy the release runtime
to `build-vs2019-qt6/distrib` using `windeployqt --release --force`.
Include the application's FFmpeg 4.x runtime DLLs and the separate `ffmpeg.exe`
encoder used by movie export.

From the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build-vs2019-qt6\tools\windows\Create-Installer.ps1
```

The script reads the version from `Boarder.exe`, stages only runtime files,
adds app-local MSVC DLLs, and creates an offline Qt Installer Framework installer
and ZIP named `B-Line-Setup-<version>-<date>`. A new version has a distinct URL,
including when two versions are published on the same day. It records all runtime hashes in `build-manifest.json` and writes
`release.json` plus a ZIP SHA-256 sidecar under
`build-vs2019-qt6/dist/windows/installer`. Existing output files are not replaced;
pass a new `-OutputDir` when packaging again on the same date.

The component ID remains `com.stargitstudio.bline`, matching the April 2026
installer. The installer uses Qt's normal directory checks: it does not run a
recursive directory deletion to replace an installation. Users should uninstall
the old application with its maintenance tool before installing into that same
directory. Keep projects outside the application installation directory.

Test a fresh installation into a unique temporary directory with
`--root <directory> --default-answer --confirm-command install CreateShortcuts=false`.
Compare the installed files with the manifest and launch `Boarder.exe` with a
clean system `PATH`. Remove only that temporary installation using its own
maintenance tool and `--default-answer --confirm-command purge`.

## Upload and register the legacy deployment

The publishing helper reads the existing GitExplorer `S3.py` and `SQL.py`
configuration without copying or printing credentials. It needs Python packages
`boto3`, `mysql-connector-python`, and `requests`. All HTTPS requests verify TLS.

```powershell
# Read-only authentication and release preflight.
python .\build-vs2019-qt6\tools\windows\Publish-Windows.py .\build-vs2019-qt6\dist\windows\installer\release.json

# Run after installation and runtime verification.
python .\build-vs2019-qt6\tools\windows\Publish-Windows.py .\build-vs2019-qt6\dist\windows\installer\release.json --publish
```

Publication uploads a new versioned, dated object, downloads and verifies its complete
SHA-256 and size, then inserts a new B-Line Windows record. It verifies that this
record is the latest legacy Windows release and writes `published-release.json`.
Previous rows are backed up locally and retained in the database; previous
objects are retained for rollback. Identical retries reuse the artifact/record,
while conflicting artifacts or a concurrent release stop publication.
No schema migrations or release notifications are performed.

## Update and verify SyncPipeline Tools

Use the production SyncPipeline deployment configuration for this step; the
GitExplorer storage/MySQL credentials do not grant access to its PostgreSQL
catalog. Never copy production secrets into this repository.

For the September 22 repair, the SyncPipeline patch changes
`sql/019_bline_windows_release_artifact.sql` to seed only a missing Windows
artifact, and adds `sql/052_bline_windows_20260922.sql` to replace the April
artifact on the current published release. The repair retains other platforms,
visuals, historical releases, and the old object, and records the prior artifact
in `tool_release_publish_jobs` for rollback. It does not replace a future build.
Deploy both files, then apply only the new SQL file to the production database.
Do not run the full bootstrap command just to publish a build.

For subsequent Windows updates, copy `Publish-Tools-Catalog.py` and the new
`published-release.json` to a unique release audit directory on the server, then
run it from `/srv/syncpipeline/app` with the production virtualenv:

```sh
.venv/bin/python /path/to/release/Publish-Tools-Catalog.py /path/to/release/published-release.json --syncpipeline-root /srv/syncpipeline/app --expected-release-id <current-id> --apply
```

The helper invokes SyncPipeline's official manifest publisher. It creates a new
release, retaining all current public platform artifacts and release visuals,
because Tools selects the latest release for the whole tool, not per platform.
It verifies the public ZIP's complete checksum before applying the manifest and
refuses an existing version or a concurrently changed active release. Current
catalog metadata is backed up before publishing. If a run fails, inspect its
backup and current catalog before retrying; do not delete the backup blindly.
Existing release rows and uploaded objects are retained for rollback.

The helper verifies both production-rendered pages, `/tools` and
`/tools/b-line-boarder`, against the live database and writes
`syncpipeline-published-release.json`. When an authenticated browser session is
available, also check both pages through HTTP. The current checks use production
templates and the database directly; they do not claim authenticated HTTP
verification. An upload or legacy database receipt alone is not evidence that
the Tools page was updated.

The September 22 repair is deployed: legacy record 139, SyncPipeline release 1,
Windows artifact 8, and publish job 6. Both production templates were rendered
against the live catalog and produced the September Windows URL. The public ZIP
was downloaded and SHA-256 verified from the production server. Authenticated
HTTP verification was unavailable; the checks used the production application
and database directly.

Production is `/srv/syncpipeline/app` on `root@syncpipeline.com`, with the existing
`.env` and `.venv`. On this Windows host, Git's SSH client with the explicit
`$env:USERPROFILE\.ssh\id_rsa` key successfully authenticated. Backups and the
deployment receipt are retained under
`/srv/syncpipeline/pre-deploy-backups/20260922-bline-windows/`.

The current September 22, 2026 package is **1.0.2**, published at 16:17 UTC with
the scene-collapse accessibility freeze fix. It is an optimized MSVC Release
build (`/O2`, `/OPT:REF`, `/OPT:ICF`). The ZIP contains one offline installer,
`B-Line-Setup-1.0.2-2026-09-22.exe`, and is 96,329,454 bytes.
Its SHA-256 is
`71fa4f769c99eedbda89d6901fc95c46b43ac993aa7ddc13405f2481e9515d29`.

Publication references: legacy Deploy **140**, Tools release **7**, Windows
artifact **13**, publish job **7**. Both production templates render the new
versioned URL. macOS/Linux artifacts and all three visuals were preserved.
The service remained active without a restart. Audit files are retained under
`/srv/syncpipeline/pre-deploy-backups/20260922-bline-windows-1.0.2/` and locally in
`build-vs2019-qt6/dist/windows/installer-1.0.2/`.

All seven smoke suites passed, including the new accessibility regression and
audio playback. Fresh installation verified all 75 runtime files and the
executable version; the app launched with a clean system `PATH`. The movie smoke
test also passed from the installed directory with no developer runtime or
plugin path. The empty disposable launch instance required termination after a
normal close request; only that test PID was stopped. Its installation was then
uninstalled and the original installation registration was unchanged. The
user's prior frozen session was not touched. This build and installer are
unsigned; no Windows code-signing certificate was available.
