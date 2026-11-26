<#
  Builds an MCnotepad release and packages it. Run from the repository root:

      powershell -ExecutionPolicy Bypass -File package.ps1
      powershell -ExecutionPolicy Bypass -File package.ps1 -PortableOnly

  Produces, in dist\<version>\ :
      MCnotepad-<version>-setup.exe      installer (the normal way to use MCnotepad)
      MCnotepad-<version>-portable.zip   portable copy: unzip anywhere, keeps its data in its own "data" folder
      SHA256SUMS.txt

  The version comes from project(MCnotepad VERSION x.y.z) in CMakeLists.txt; raise
  it there for a new release. The release is built in build\package with
  MCNOTEPAD_RELEASE=ON, separate from the development build in build\windows-x64.
#>
param(
    [switch]$AllowDirty,   # package even with uncommitted changes (build id gets "-dirty")
    [switch]$PortableOnly  # skip Inno Setup and create the canonical portable release only
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
Set-Location $root

function Step($text) { Write-Host "==> $text" -ForegroundColor Cyan }
function Fail($text) { Write-Host "ERROR: $text" -ForegroundColor Red; exit 1 }

# ---- version and build id ---------------------------------------------------------
$cmakeText = Get-Content -Raw (Join-Path $root 'CMakeLists.txt')
if ($cmakeText -notmatch 'project\(MCnotepad VERSION (\d+\.\d+\.\d+)') { Fail 'version not found in CMakeLists.txt' }
$version = $Matches[1]
$buildId = (git rev-parse --short HEAD).Trim()
if (git status --porcelain --untracked-files=no) {
    if (-not $AllowDirty) { Fail 'there are uncommitted changes; commit them first (or pass -AllowDirty for a test package)' }
    $buildId += '-dirty'
}
Step "MCnotepad $version ($buildId)"

# ---- tools --------------------------------------------------------------------------
$iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe", "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $PortableOnly -and -not $iscc) { Fail 'Inno Setup 6 is not installed; use -PortableOnly for a portable release or install Inno Setup 6' }

# ---- release build --------------------------------------------------------------------
$buildDir = Join-Path $root 'build\package'
Step 'configure release build'
cmake --preset windows-x64 -B $buildDir -DMCNOTEPAD_RELEASE=ON "-DMCNOTEPAD_BUILD_ID=$buildId" | Out-Null
if ($LASTEXITCODE) { Fail 'cmake configure failed' }
Step 'build'
cmake --build $buildDir --config Release --parallel 4 | Select-String -Pattern 'error|warning C' | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE) { Fail 'build failed' }
$exe = Join-Path $buildDir 'Release\MCnotepad.exe'
Step 'self-test'
# Started as its own process: only the exit code counts (libraries may print warnings).
$test = Start-Process -FilePath $exe -ArgumentList '--self-test' -Wait -PassThru -WindowStyle Hidden
if ($test.ExitCode) { Fail "self-test failed (exit $($test.ExitCode)); run `"$exe --self-test`" to see which check" }

# ---- stage the program files ----------------------------------------------------------
$out = Join-Path $root "dist\$version"
if (Test-Path $out) {
    Get-ChildItem $out -File | Where-Object {
        $_.Name -like "MCnotepad-$version-*" -or $_.Name -eq 'SHA256SUMS.txt'
    } | Remove-Item -Force
}
$stage = Join-Path $out '_stage'
if (Test-Path $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item $exe $stage
Copy-Item (Join-Path $buildDir 'Release\WebView2Loader.dll') $stage   # Markdown / HTML preview
foreach ($folder in 'resources', 'dict') { Copy-Item -Recurse (Join-Path $buildDir "Release\$folder") $stage }
Copy-Item (Join-Path $root 'README.md') $stage

# ---- portable zip ---------------------------------------------------------------------
Step 'portable zip'
$portableName = "MCnotepad-$version-portable"
$portable = Join-Path $out $portableName
if (Test-Path $portable) { Remove-Item -LiteralPath $portable -Recurse -Force }
Copy-Item -Recurse $stage $portable
@(
    '; This file makes MCnotepad portable: sessions and recovery snapshots are kept'
    '; in the "data" folder next to MCnotepad.exe instead of %LOCALAPPDATA%\MCnotepad.'
    '; Delete this file to use the same data as the installed MCnotepad.'
) | Set-Content -Encoding UTF8 (Join-Path $portable 'portable.ini')
$zip = "$portable.zip"
if (Test-Path $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path $portable -DestinationPath $zip -CompressionLevel Optimal
Remove-Item -LiteralPath $portable -Recurse -Force

# ---- installer ------------------------------------------------------------------------
if (-not $PortableOnly) {
    Step 'installer'
    & $iscc /Q "/DAppVersion=$version" "/DBuildId=$buildId" "/DStageDir=$stage" "/DOutputDir=$out" (Join-Path $root 'installer\MCnotepad.iss')
    if ($LASTEXITCODE) { Fail 'Inno Setup failed' }
}
Remove-Item -LiteralPath $stage -Recurse -Force

# ---- checksums and summary ------------------------------------------------------------
$files = Get-ChildItem $out -File | Where-Object { $_.Name -like "MCnotepad-$version-*" }
$files | ForEach-Object { '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name } |
    Set-Content -Encoding ASCII (Join-Path $out 'SHA256SUMS.txt')
$latest = Join-Path $root 'dist\latest'
if (Test-Path $latest) { Remove-Item -LiteralPath $latest -Recurse -Force }
New-Item -ItemType Directory -Force -Path $latest | Out-Null
foreach ($file in $files) { Copy-Item -LiteralPath $file.FullName -Destination $latest }
Copy-Item -LiteralPath (Join-Path $out 'SHA256SUMS.txt') -Destination $latest
Set-Content -LiteralPath (Join-Path $latest 'VERSION.txt') -Value @(
    "MCnotepad $version"
    "Build: $buildId"
    "Source: dist\$version"
) -Encoding ASCII
Step "done: $out"
Write-Host "latest: $latest"
$files | ForEach-Object { Write-Host ('    {0,-36} {1,8:N1} MB' -f $_.Name, ($_.Length / 1MB)) }
