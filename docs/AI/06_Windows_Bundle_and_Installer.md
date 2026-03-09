# Windows Bundle and Installer (Qt 6.10, MSVC 2022)

## Goal
Create a reproducible Windows packaging flow for B-Line:
1. Compile `Boarder.exe`.
2. Build a runnable bundle folder (`Boarder.exe` + Qt/runtime DLLs).
3. Build an offline installer (`.exe`) with Qt Installer Framework.

## Packaging Files
1. Bundle script: `build-vs2019-qt6/tools/windows/Bundle-Windows.ps1`
2. Installer script: `build-vs2019-qt6/tools/windows/Create-Installer.ps1`
3. IFW config template: `build-vs2019-qt6/tools/windows/ifw/config/config.xml.in`
4. IFW package template: `build-vs2019-qt6/tools/windows/ifw/packages/com.gamefusion.bline/meta/package.xml.in`
5. IFW install script: `build-vs2019-qt6/tools/windows/ifw/packages/com.gamefusion.bline/meta/installscript.qs`

## Prerequisites
1. Qt runtime/tooling installed at `C:\Qt\6.10.2\msvc2022_64`.
2. Qt Installer Framework installed at `C:\Qt\Tools\QtInstallerFramework\4.10`.
3. Visual Studio C++ toolchain available (MSVC 2022 / VS 18 toolset in this environment).
4. Workspace root is `F:\GameSource`.
5. Optional AI runtime: `LlamaEngine.dll` available if AI features are required.

## Release Pipeline (Step-by-Step)
Run from repo root: `F:\GameSource\Applications\B-Line`.

1. Compile Release executable.
```powershell
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"" && cd /d F:\GameSource\Applications\B-Line\build-vs2019-qt6\build\Desktop_Qt_6_10_2_MSVC2022_64bit-Release && nmake /f Makefile.Release"
```

2. Build Release bundle.
```powershell
powershell -ExecutionPolicy Bypass -File .\build-vs2019-qt6\tools\windows\Bundle-Windows.ps1 `
  -BuildDir F:\GameSource\Applications\B-Line\build-vs2019-qt6\build\Desktop_Qt_6_10_2_MSVC2022_64bit-Release `
  -Config Release `
  -QtRoot C:\Qt\6.10.2\msvc2022_64 `
  -GameSourceRoot F:\GameSource
```

3. Smoke test the bundle.
```powershell
F:\GameSource\Applications\B-Line\build-vs2019-qt6\dist\windows\B-Line-1.0.0-Release\Boarder.exe
```

4. Build offline installer from the release bundle.
```powershell
powershell -ExecutionPolicy Bypass -File .\build-vs2019-qt6\tools\windows\Create-Installer.ps1 `
  -BundleDir F:\GameSource\Applications\B-Line\build-vs2019-qt6\dist\windows\B-Line-1.0.0-Release `
  -IfwRoot C:\Qt\Tools\QtInstallerFramework\4.10 `
  -OutputDir F:\GameSource\Applications\B-Line\build-vs2019-qt6\dist\windows\installer
```

5. Confirm output installer.
`F:\GameSource\Applications\B-Line\build-vs2019-qt6\dist\windows\installer\B-Line-Setup-1.0.0.exe`

## Debug Bundle Pipeline (Step-by-Step)
1. Compile Debug executable.
```powershell
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"" && cd /d F:\GameSource\Applications\B-Line\build-vs2019-qt6\build\Desktop_Qt_6_10_2_MSVC2022_64bit-Debug && nmake /f Makefile.Debug"
```

2. Build Debug bundle.
```powershell
powershell -ExecutionPolicy Bypass -File .\build-vs2019-qt6\tools\windows\Bundle-Windows.ps1 `
  -BuildDir F:\GameSource\Applications\B-Line\build-vs2019-qt6\build\Desktop_Qt_6_10_2_MSVC2022_64bit-Debug `
  -Config Debug `
  -QtRoot C:\Qt\6.10.2\msvc2022_64 `
  -GameSourceRoot F:\GameSource
```

3. Run Debug bundle.
`F:\GameSource\Applications\B-Line\build-vs2019-qt6\dist\windows\B-Line-1.0.0-Debug\Boarder.exe`

## Optional Script Parameters
1. `-FfmpegBinDir <path>`: force FFmpeg runtime DLL source.
2. `-LlamaEngineDll <path>`: copy `LlamaEngine.dll` into bundle.
3. `-BundleDir <path>`: write bundle to a custom folder.
4. `-OutputDir <path>`: write installer to a custom folder.

## Troubleshooting
1. `Executable not found ...\...\release\Boarder.exe`
Use a real `-BuildDir` path. Do not use `...` placeholders.

2. `Access denied` during bundle cleanup
`Boarder.exe` or a DLL in the target bundle folder is still running/locked. Close the app or use a new `-BundleDir`.

3. Missing FFmpeg DLL at runtime (`avformat-58.dll`, etc.)
Re-run bundling and verify selected FFmpeg DLLs in script output. Force with `-FfmpegBinDir` if needed.

4. `LlamaEngine.dll not found` warning
If AI features are needed, pass `-LlamaEngineDll` or put the DLL next to `Boarder.exe`.

5. Smart App Control blocks launch
Unsigned binaries can be blocked outside dev tools. Internal testing may require a documented temporary exception. Production distribution requires signed binaries and signed installer.

## Publisher
Installer publisher default is `Stargit Studio AB` (set in `build-vs2019-qt6/tools/windows/Create-Installer.ps1`).

## Local Security Exception (Temporary, Internal Only)
1. Disable Smart App Control only on designated internal dev/test machines.
2. Record machine, owner, date, and reason in studio IT/security tracking.
3. Apply same documented policy for partner studio testing only.
4. Remove this exception path after signing is in place and validated.
