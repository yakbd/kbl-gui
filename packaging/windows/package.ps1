# Windows packages of the editor: a portable zip and yakbd-setup.exe (the CLI's own installer with the editor as its
# extra payload, yakbd-cli docs/installer-cli.md §12.1), both with the CLI of one tagged yakbd-cli release.
#   pwsh packaging/windows/package.ps1 -Build build -Cli dist/cli/yakbd.exe -CliSource dist/cli-src -Out dist
# -CliSource: a checkout of yakbd/yakbd-cli at the same tag (for tools/toolsetup); Go must be on PATH.
param(
  [Parameter(Mandatory)] [string] $Build,
  [Parameter(Mandatory)] [string] $Cli,
  [string] $CliSource = "",
  [string] $CliDocs = "",  # folder with yakbd-cli-LICENSE and yakbd-cli-NOTICE; default: the folder of -Cli
  [Parameter(Mandatory)] [string] $Out
)
$ErrorActionPreference = "Stop"
$Build = (Resolve-Path $Build).Path
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$ver = (Select-String -Path "$Build/CMakeCache.txt" -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(.*)$').Matches[0].Groups[1].Value
$name = "yakbd-gui-$ver-windows-x64"
$work = Join-Path $Out ".work"
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
$stage = Join-Path $work "gui"

# 1. Qt's deployment script (windeployqt): Qt DLLs, plugins, QML modules and the MinGW runtime next to the exe.
cmake --install $Build --prefix $stage
if ($LASTEXITCODE -ne 0) { throw "cmake --install failed" }
$bin = $stage  # flat: CMAKE_INSTALL_BINDIR is "." on Windows
# If the deployment script still put the DLLs into bin\, flatten: everything beside yakbd-gui.exe.
if (Test-Path (Join-Path $stage "bin/Qt6Core.dll")) {
  Get-ChildItem (Join-Path $stage "bin") | Move-Item -Destination $stage -Force
  Remove-Item (Join-Path $stage "bin") -Recurse -Force
  Set-Content -Encoding ascii (Join-Path $stage "qt.conf") "[Paths]`r`nPrefix = .`r`n"
}
if (-not (Test-Path (Join-Path $stage "Qt6Core.dll")) -or -not (Test-Path (Join-Path $stage "yakbd-gui.exe"))) {
  throw "the install is not flat: yakbd-gui.exe and Qt6Core.dll must be in one folder"
}
foreach ($style in "Material", "Imagine", "Universal") {
  Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $bin "qml/QtQuick/Controls/$style")
  Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $bin "Qt6QuickControls2$style*.dll")
}
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $bin "qml/QtTest"), (Join-Path $bin "plugins/qmltooling")
Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $bin "Qt6Test.dll"), (Join-Path $bin "Qt6QuickTest.dll")
# Licences: ours, Qt's (LGPL), and the MinGW runtime's from the toolchain.
$docs = Join-Path $bin "licenses"
New-Item -ItemType Directory -Force -Path $docs | Out-Null
Copy-Item LICENSE, NOTICE $bin
Copy-Item third_party/licenses/* $docs
$gcc = (Get-Command g++).Source
$mingwLicenses = Join-Path (Split-Path (Split-Path $gcc)) "licenses"
foreach ($part in "gcc", "mingw-w64", "winpthreads") {
  $from = Join-Path $mingwLicenses $part
  if (Test-Path $from) { Copy-Item -Recurse $from (Join-Path $docs "mingw-$part") }
}
Get-ChildItem -Recurse $stage | Where-Object { $_.FullName -notlike "$bin*" -and -not $_.PSIsContainer } |
  ForEach-Object { Write-Host "not packaged: $($_.FullName)" }

# The CLI's own LICENSE and NOTICE, next to yakbd.exe in the zip and in the setup's payload.
if ($CliDocs -eq "") { $CliDocs = Split-Path $Cli }
foreach ($doc in "LICENSE", "NOTICE") {
  $f = Join-Path $CliDocs "yakbd-cli-$doc"
  if (-not (Test-Path $f)) { throw "missing $f (the CLI's $doc must ship with it)" }
  Copy-Item $f $bin
}

# 2. Portable zip: the editor and yakbd.exe in one folder.
$portable = Join-Path $work $name
Copy-Item -Recurse $bin $portable
Copy-Item $Cli (Join-Path $portable "yakbd.exe")
& (Join-Path $portable "yakbd.exe") version
Compress-Archive -Path $portable -DestinationPath (Join-Path $Out "$name.zip") -Force

# 3. yakbd-setup.exe with the editor as the extra payload (x64 only: the editor has no other Windows build yet).
if ($CliSource -ne "" -and -not (Select-String -Quiet -Path (Join-Path $CliSource "tools/toolsetup/main.go") -Pattern '"extra"')) {
  # yakbd-cli 0.1.0-preview.1 has no extra payload (it came with 0.1.0-preview.2): zip only.
  Write-Warning "this yakbd-cli release cannot carry the editor in yakbd-setup.exe; only the zip is built"
  $CliSource = ""
}
if ($CliSource -ne "") {
  Push-Location $CliSource
  try {
    $log = go run ./tools/toolsetup -o (Join-Path $Out "yakbd-setup.exe") -builds x64 -extra "x64=$bin" 2>&1
    $log | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "toolsetup failed" }
    # The yakbd.exe it carries must be the released one (yakbd-cli docs/installer-cli.md §12.1): same SHA-256 as
    # the yakbd.exe of the release zip. A different Go version gives a different build.
    $carried = (($log | Select-String '^yakbd.exe x64 ([0-9a-f]{64})').Matches | Select-Object -First 1).Groups[1].Value
    $released = (Get-FileHash -Algorithm SHA256 $Cli).Hash.ToLower()
    if ($carried -ne $released) { throw "yakbd-setup.exe carries yakbd.exe $carried, the release has $released" }
    Write-Host "yakbd.exe in yakbd-setup.exe is the released one ($released)"
  } finally { Pop-Location }
}

Remove-Item -Recurse -Force $work
Get-ChildItem $Out -File | ForEach-Object {
  "{0}  {1}" -f (Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLower(), $_.Name
} | Set-Content -Encoding ascii (Join-Path $Out "$name.SHA256SUMS")
Get-ChildItem $Out
