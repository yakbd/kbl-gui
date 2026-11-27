# The editor's Windows payload for one architecture, the contents of the install folder YAKBD\Editor\: yakbd-gui.exe,
# qt.conf, the licence texts and the CLI's LICENSE and NOTICE, with Qt, its plugins and QML and the Visual C++ runtime
# in the subfolder qt\ (a private side-by-side assembly; the build needs -DYAKBD_QT_SUBFOLDER=ON). The editor finds the
# CLI at ..\CLI\yakbd.exe. Run in a Visual Studio developer shell of the target architecture (for VCToolsRedistDir).
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
# Only what the editor uses. It runs the FluentWinUI3 style (which builds on Fusion and Basic); Qt Quick draws with
# Direct3D 11, whose shader compiler (d3dcompiler_47.dll) is part of Windows 10 and later; it has no network code
# and loads only SVG and ICO images.
foreach ($style in "Material", "Imagine", "Universal", "Windows", "iOS") {
  Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $stage "qml/QtQuick/Controls/$style")
  Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $stage "Qt6QuickControls2$style*.dll")
}
$unused = "qml/QtTest", "qml/QtQuick/NativeStyle", "Qt6QuickNativeStyle.dll", "Qt6Test.dll", "Qt6QuickTest.dll",
  "plugins/qmltooling", "plugins/tls", "plugins/networkinformation", "plugins/generic",
  "plugins/imageformats/qjpeg.dll", "plugins/imageformats/qgif.dll",
  "dxcompiler.dll", "dxil.dll", "d3dcompiler_47.dll", "opengl32sw.dll"
foreach ($u in $unused) { Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $stage $u) }

# 2. The Visual C++ runtime DLLs the payload imports, app-local (Microsoft's redistributable files, from the build's
#    own toolset); dumpbin lists every import, so a DLL nobody imports is not shipped.
$crt = Get-ChildItem -Directory (Join-Path $VcRedist $Arch) -Filter "Microsoft.VC*.CRT" | Select-Object -First 1
if (-not $crt) { throw "no Visual C++ runtime in $VcRedist\$Arch (run in a Visual Studio developer shell)" }
$available = @{}
Get-ChildItem (Join-Path $crt.FullName "*.dll") | ForEach-Object { $available[$_.Name.ToLower()] = $_.FullName }
$needed = @{}
$queue = [System.Collections.Generic.Queue[string]]::new()
Get-ChildItem -Recurse -File $stage -Include *.dll, *.exe | ForEach-Object { $queue.Enqueue($_.FullName) }
while ($queue.Count -gt 0) {
  $file = $queue.Dequeue()
  foreach ($line in (& dumpbin /nologo /dependents $file)) {
    $dll = $line.Trim().ToLower()
    if ($available.ContainsKey($dll) -and -not $needed.ContainsKey($dll)) {
      $needed[$dll] = $true
      $queue.Enqueue($available[$dll])
    }
  }
}
foreach ($dll in $needed.Keys) { Copy-Item $available[$dll] $stage }
Write-Host "Visual C++ runtime from $($crt.FullName): $(($needed.Keys | Sort-Object) -join ', ')"

# 2b. Qt, its plugins and QML, and the runtime into the subfolder qt\, so that the install folder holds only
#     yakbd-gui.exe, qt.conf and the licences. The DLLs become a private side-by-side assembly named "qt": the exe
#     (built with YAKBD_QT_SUBFOLDER) declares it in its manifest, and Windows looks for a private assembly in
#     <exe folder>\<assembly name>\ (Microsoft Learn, "Assembly Searching Sequence"), so no PATH is needed. qt.conf
#     points Qt at qt\plugins and qt\qml.
$qt = Join-Path $stage "qt"
New-Item -ItemType Directory -Force -Path $qt | Out-Null
Get-ChildItem (Join-Path $stage "*.dll") | Move-Item -Destination $qt
foreach ($dir in "plugins", "qml") { Move-Item (Join-Path $stage $dir) (Join-Path $qt $dir) }
$asmArch = if ($Arch -eq "x64") { "amd64" } else { "arm64" }  # PowerShell names ignore case: not $arch
$files = (Get-ChildItem (Join-Path $qt "*.dll") | Sort-Object Name | ForEach-Object { "  <file name=`"$($_.Name)`"/>" }) -join "`r`n"
Set-Content -Encoding utf8 (Join-Path $qt "qt.manifest") @"
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity type="win32" name="qt" version="6.12.1.0" processorArchitecture="$asmArch"/>
$files
</assembly>
"@
Set-Content -Encoding ascii (Join-Path $stage "qt.conf") "[Paths]`r`nPrefix = qt`r`n"
Write-Host "qt\: $((Get-ChildItem (Join-Path $qt '*.dll')).Count) DLLs in the private assembly"

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
