# Build status

Latest local build: **clean** (zero compile errors), `ctest` 1/1 passed, `NetControl.exe` launches elevated and responds.

The source package includes a Windows CI workflow (`.github/workflows/windows-build.yml`) that builds with the Visual Studio 2022 toolset on `windows-latest` and runs `ctest`.

## Local build (this machine)

Visual Studio Build Tools **2026** (MSVC 19.x) was used successfully:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Generator notes

| Environment | Generator |
|-------------|-----------|
| VS 2022 | `Visual Studio 17 2022` |
| VS 2026 Build Tools | `Visual Studio 18 2026` |

CI uses VS 2022 on `windows-latest`. Local docs mention both; the project does **not** require a specific VS year beyond CMake 3.24 + MSVC C++20.

Executable: `build\Release\NetControl.exe`
