# The editor's Windows payload for one architecture: yakbd-gui.exe with Qt, the Visual C++ runtime, the licence
# texts and the CLI's LICENSE and NOTICE, in one flat folder. packaging/windows/make-setup.sh puts the x64 and arm64
# payloads into yakbd-setup.exe, the yakbd tool's own installer (yakbd-cli docs/installer-cli.md §12.1), next to
# yakbd.exe. Run in a Visual Studio developer shell of the target architecture (for VCToolsRedistDir).
#   pwsh packaging/windows/package.ps1 -Build build -Arch x64|arm64 -CliDocs <folder> -Out dist
# Writes <Out>\payload-<Arch>\.
param(
  [Parameter(Mandatory)] [string] $Build,
  [Parameter(Mandatory)] [ValidateSet("x64", "arm64")] [string] $Arch,
  [Parameter(Mandatory)] [string] $CliDocs,  # folder with yakbd-cli-LICENSE and yakbd-cli-NOTICE
  [Parameter(Mandatory)] [string] $Out,
  [string] $VcRedist = $env:VCToolsRedistDir
)
$ErrorActionPreference = "Stop"
$Build = (Resolve-Path $Build).Path
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$stage = Join-Path $Out "payload-$Arch"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }

# 1. Qt's deployment script (windeployqt): Qt DLLs, plugins and QML modules next to the exe.
cmake --install $Build --prefix $stage
if ($LASTEXITCODE -ne 0) { throw "cmake --install failed" }
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
  Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $stage "qml/QtQuick/Controls/$style")
  Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $stage "Qt6QuickControls2$style*.dll")
}
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $stage "qml/QtTest"), (Join-Path $stage "plugins/qmltooling")
Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $stage "Qt6Test.dll"), (Join-Path $stage "Qt6QuickTest.dll")

# 2. The Visual C++ runtime DLLs, app-local (Microsoft's redistributable files, from the build's own toolset).
$crt = Get-ChildItem -Directory (Join-Path $VcRedist $Arch) -Filter "Microsoft.VC*.CRT" | Select-Object -First 1
if (-not $crt) { throw "no Visual C++ runtime in $VcRedist\$Arch (run in a Visual Studio developer shell)" }
Copy-Item (Join-Path $crt.FullName "*.dll") $stage
Write-Host "Visual C++ runtime from $($crt.FullName): $((Get-ChildItem (Join-Path $crt.FullName '*.dll')).Name -join ', ')"

# 3. Licences: ours, Qt's (LGPL) and the other texts NOTICE names, and the CLI's own LICENSE and NOTICE (they go next
#    to yakbd.exe when the setup installs the payload).
$docs = Join-Path $stage "licenses"
New-Item -ItemType Directory -Force -Path $docs | Out-Null
Copy-Item LICENSE, NOTICE $stage
Copy-Item third_party/licenses/* $docs
foreach ($doc in "LICENSE", "NOTICE") {
  $f = Join-Path $CliDocs "yakbd-cli-$doc"
  if (-not (Test-Path $f)) { throw "missing $f (the CLI's $doc must ship with it)" }
  Copy-Item $f $stage
}
foreach ($bad in "yakbd.exe", "setup.exe") {
  if (Test-Path (Join-Path $stage $bad)) { throw "the payload must not contain $bad" }
}
$files = Get-ChildItem -Recurse -File $stage
Write-Host "payload-${Arch}: $($files.Count) files, $([math]::Round(($files | Measure-Object Length -Sum).Sum / 1MB, 1)) MB"
