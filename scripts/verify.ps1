#requires -Version 7.0

param(
    [string]$QtRoot = 'D:/Qt/6.8.2/mingw_64',
    [string]$ToolsRoot = 'D:/Qt/Tools',
    [string]$BuildRoot = (Join-Path ([IO.Path]::GetTempPath()) 'ydi-tts-verify'),
    [switch]$SkipTests,
    [switch]$Package,
    [ValidateRange(1, 32)][int]$Jobs = 2
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-common.ps1')
$sourceRoot = Split-Path $PSScriptRoot -Parent
$stageRoot = Join-Path $BuildRoot 'source'
$binaryRoot = Join-Path $BuildRoot 'build'
$artifactRoot = Join-Path $sourceRoot 'build/verification'
$cmake = Join-Path $ToolsRoot 'CMake_64/bin/cmake.exe'
$compiler = Join-Path $ToolsRoot 'mingw1310_64/bin/g++.exe'
$ninja = Join-Path $ToolsRoot 'Ninja/ninja.exe'

# Keep the actual compile paths ASCII and outside OneDrive's cloud placeholders.
New-Item -ItemType Directory -Force -Path $stageRoot, $artifactRoot | Out-Null
Clear-VerificationArtifacts -Directory $artifactRoot
Copy-BuildSources -SourceRoot $sourceRoot -StageRoot $stageRoot

$previousEnvironment = Save-BuildEnvironment
try {
    $env:PATH = "$QtRoot/bin;$(Split-Path $compiler -Parent);$env:PATH"
    $tests = if ($SkipTests) { 'OFF' } else { 'ON' }
    & $cmake --fresh -S $stageRoot -B $binaryRoot -G Ninja -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_MAKE_PROGRAM=$ninja" "-DBUILD_TESTING=$tests" -DYDI_SINGLE_FILE=OFF
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmake --build $binaryRoot --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    if (!$SkipTests) {
        Set-UiTestEnvironment -CaptureDirectory $artifactRoot
        & (Join-Path $ToolsRoot 'CMake_64/bin/ctest.exe') --test-dir $binaryRoot --output-on-failure --output-junit (Join-Path $artifactRoot 'tests.xml')
        if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    }
    Copy-Item -LiteralPath (Join-Path $binaryRoot 'YDI_TTS.exe') -Destination $artifactRoot -Force
    if ($Package) {
        & $cmake --install $binaryRoot --prefix (Join-Path $artifactRoot 'app')
        if ($LASTEXITCODE -ne 0) { throw 'Deployment failed.' }
        $platforms = Join-Path $artifactRoot 'app/plugins/platforms'
        Copy-Item -LiteralPath (Join-Path $QtRoot 'plugins/platforms/qoffscreen.dll') -Destination $platforms -Force
        # Verify the packaged application without the development Qt or MinGW PATH.
        Set-UiTestEnvironment -CaptureDirectory $artifactRoot -Isolated
        $packagedExe = Join-Path $artifactRoot 'app/bin/YDI_TTS.exe'
        foreach ($platform in @('offscreen', 'windows')) {
            Invoke-AppSmokeTest -Executable $packagedExe -Platform $platform
        }
        Write-Output 'Packaged application startup: passed (isolated PATH).'
    }
    Write-Output "Build: $binaryRoot"
    Write-Output "Artifacts: $artifactRoot"
} finally {
    Restore-BuildEnvironment -Snapshot $previousEnvironment
}
