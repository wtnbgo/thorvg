# Build thorvg twice (TVG_SIMD=OFF / ON) and verify tvg-raster-test output
# is bit-identical. On x64 this validates the AVX kernels + dispatch.
#
# Usage:  pwsh tools/raster-test/compare-simd.ps1 [-BuildRoot <dir>] [-Clean]
param(
    [string]$BuildRoot = (Join-Path $env:TEMP "tvg-raster-test"),
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$src = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

$common = @(
    "-DTVG_BUILD_SHARED=OFF",
    "-DTVG_THREADS=OFF",
    "-DTVG_PARTIAL=OFF",
    "-DTVG_LOADER_SVG=OFF",
    "-DTVG_LOADER_LOTTIE=OFF",
    "-DTVG_LOADER_TTF=OFF",
    "-DTVG_LOTTIE_EXPRESSIONS=OFF",
    "-DTVG_TOOL_RASTER_TEST=ON",
    "-DCMAKE_DISABLE_FIND_PACKAGE_OpenMP=TRUE"
)

if ($Clean -and (Test-Path $BuildRoot)) { Remove-Item -Recurse -Force $BuildRoot }

$results = @{}
foreach ($simd in @("OFF", "ON")) {
    $bdir = Join-Path $BuildRoot "simd-$simd"
    Write-Host "=== configure+build TVG_SIMD=$simd -> $bdir" -ForegroundColor Cyan
    cmake -S $src -B $bdir -G "Visual Studio 17 2022" -A x64 @common "-DTVG_SIMD=$simd" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "configure failed (TVG_SIMD=$simd)" }
    cmake --build $bdir --config Release --target tvg-raster-test | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "build failed (TVG_SIMD=$simd)" }
    $exe = Join-Path $bdir "Release\tvg-raster-test.exe"
    $out = & $exe
    if ($LASTEXITCODE -ne 0) { Write-Host ($out -join "`n"); throw "raster-test reported failure (TVG_SIMD=$simd)" }
    $results[$simd] = $out
    Write-Host ($out -join "`n")
}

Write-Host "=== diff" -ForegroundColor Cyan
$a = $results["OFF"] -join "`n"
$b = $results["ON"] -join "`n"
if ($a -ceq $b) {
    Write-Host "PASS: SIMD ON/OFF outputs are bit-identical" -ForegroundColor Green
    exit 0
} else {
    Compare-Object $results["OFF"] $results["ON"] | Format-Table -AutoSize | Out-String | Write-Host
    Write-Host "FAIL: outputs differ" -ForegroundColor Red
    exit 1
}
