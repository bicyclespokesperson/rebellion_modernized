param(
  [string]$BuildDirectory = "build/native-windows",
  [string]$Configuration = "RelWithDebInfo",
  [string]$GameExecutable = $env:REB_GAME_EXE
)

$ErrorActionPreference = "Stop"
$Repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildPath = Join-Path $Repo $BuildDirectory
$Venv = Join-Path $Repo "build/tools-venv"

if (-not $GameExecutable) {
  $GameExecutable = Join-Path $Repo "orig_game_files/Star Wars - Rebellion/REBEXE.EXE"
}
if (-not (Test-Path -LiteralPath $GameExecutable -PathType Leaf)) {
  throw "Game executable not found. Set REB_GAME_EXE or pass -GameExecutable."
}
if (-not (Get-Command uv -ErrorAction SilentlyContinue)) {
  throw "uv is required for the asset extractor and Ninja setup. Install it from https://docs.astral.sh/uv/"
}

New-Item -ItemType Directory -Force -Path $BuildPath | Out-Null
if (-not (Test-Path -LiteralPath (Join-Path $Repo "third_party/SDL3-3.4.18-src/CMakeLists.txt")) -or
    -not (Test-Path -LiteralPath (Join-Path $Repo "third_party/freetype-2.13.2/CMakeLists.txt"))) {
  throw "Native dependencies are missing. Fetch them with: bash tools/fetch_deps.sh freetype liberation sdl3"
}

if (-not (Test-Path -LiteralPath (Join-Path $Venv "Scripts/python.exe"))) {
  uv venv $Venv
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
uv pip install --python (Join-Path $Venv "Scripts/python.exe") pefile ninja
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$Python = Join-Path $Venv "Scripts/python.exe"
$Ninja = Join-Path $Venv "Scripts/ninja.exe"
$AssetPack = Join-Path $BuildPath "startup-assets.bin"
& $Python (Join-Path $Repo "tools/extract_startup_assets.py") --exe $GameExecutable --out $AssetPack
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake -S $Repo -B $BuildPath -G Ninja `
  -DCMAKE_C_COMPILER=clang-cl `
  -DCMAKE_CXX_COMPILER=clang-cl `
  "-DCMAKE_MAKE_PROGRAM=$Ninja" `
  "-DCMAKE_BUILD_TYPE=$Configuration" `
  -DREB_NATIVE=ON `
  "-DREB_STARTUP_ASSET_PACK=$AssetPack"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build $BuildPath --config $Configuration --target rebellion
exit $LASTEXITCODE
