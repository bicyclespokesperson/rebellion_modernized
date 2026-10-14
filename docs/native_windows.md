# Native Windows build

The native target now selects a Windows OS adapter when configured on Windows. It builds the shared game sources for the host ABI, uses SDL3 and FreeType for presentation, and uses the Windows SDK and MSVC-compatible CRT for operating-system services. The player's licensed game files remain in their own installation directory.

Use a Visual Studio Developer PowerShell with the x64 C++ tools and Windows SDK, plus CMake, LLVM `clang-cl`, Git Bash, and `uv`. The build script creates a local uv environment for the PE asset extractor and Ninja. Set `REB_GAME_EXE` to your installed `REBEXE.EXE` if it is not at the default path. Fetch the source dependencies once from Git Bash:

```powershell
bash tools/fetch_deps.sh freetype liberation sdl3
tools\build_native_windows.ps1
```

The script extracts `startup-assets.bin`, configures `build/native-windows` with the requested single-configuration CMake build type, and builds the `rebellion` target. To choose another output directory or configuration, pass `-BuildDirectory` or `-Configuration`.

GitHub Actions compiles the Windows native objects and runs resource-free tests with `clang-cl` and the Windows SDK. The Windows backend has not yet been run through a game startup/load/simulation/save/reload smoke check, so Windows support is not considered verified until that host validation succeeds.
