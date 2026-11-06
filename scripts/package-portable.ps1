#requires -Version 7.0

param(
    [string]$QtRoot = 'D:/Qt/6.8.2-static/mingw1310_64',
    [string]$ToolsRoot = 'D:/Qt/Tools',
    [string]$BuildRoot = (Join-Path ([IO.Path]::GetTempPath()) 'ydi-tts-portable'),
    [ValidateRange(1, 32)][int]$Jobs = 2
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-common.ps1')
$sourceRoot = Split-Path $PSScriptRoot -Parent
$stageRoot = Join-Path $BuildRoot 'source'
$binaryRoot = Join-Path $BuildRoot 'build'
$releaseRoot = Join-Path $sourceRoot 'build/portable/release'
$verificationRoot = Join-Path $sourceRoot 'build/portable/verification'
$cmake = Join-Path $ToolsRoot 'CMake_64/bin/cmake.exe'
$ctest = Join-Path $ToolsRoot 'CMake_64/bin/ctest.exe'
$compiler = Join-Path $ToolsRoot 'mingw1310_64/bin/g++.exe'
$objdump = Join-Path $ToolsRoot 'mingw1310_64/bin/objdump.exe'
$ninja = Join-Path $ToolsRoot 'Ninja/ninja.exe'

foreach ($toolPath in @($cmake, $ctest, $compiler, $objdump, $ninja, (Join-Path $QtRoot 'lib/cmake/Qt6/Qt6Config.cmake'))) {
    if (!(Test-Path -LiteralPath $toolPath)) { throw "Required tool or static Qt configuration is missing: $toolPath" }
}
New-Item -ItemType Directory -Force -Path $stageRoot, $releaseRoot, $verificationRoot | Out-Null
Clear-VerificationArtifacts -Directory $verificationRoot
# Build in an ASCII temporary path; the original workspace is never moved.
Copy-BuildSources -SourceRoot $sourceRoot -StageRoot $stageRoot

$previousEnvironment = Save-BuildEnvironment
try {
    $env:PATH = "$QtRoot/bin;$(Split-Path $compiler -Parent);$env:PATH"
    & $cmake --fresh -S $stageRoot -B $binaryRoot -G Ninja -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_MAKE_PROGRAM=$ninja" `
        -DYDI_SINGLE_FILE=ON -DBUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) { throw 'Static Qt configuration failed.' }
    & $cmake --build $binaryRoot --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Single-file build failed.' }

    $binary = Join-Path $binaryRoot 'YDI_TTS_Portable.exe'
    $peDetails = & $objdump -p $binary
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect executable dependencies.' }
    $dependencies = @($peDetails | ForEach-Object {
        if ($_ -match 'DLL Name:\s*(\S+)') { $Matches[1] }
    } | Sort-Object -Unique)
    if ($dependencies.Count -eq 0) { throw 'Executable dependency inspection returned no DLL imports.' }
    foreach ($dependency in $dependencies) {
        $isApiSet = $dependency -match '^(api-ms-win-|ext-ms-win-)'
        $systemDll = Join-Path (Join-Path $env:WINDIR 'System32') $dependency
        if ($dependency -match '^(Qt[0-9]|libgcc|libstdc\+\+|libwinpthread)' -or
            (!$isApiSet -and !(Test-Path -LiteralPath $systemDll))) {
            throw "Single-file build still imports a non-system DLL: $dependency"
        }
    }
    $dependencies | Set-Content -LiteralPath (Join-Path $verificationRoot 'dependencies.txt') -Encoding utf8

    # Test with no development Qt/MinGW paths or external Qt plugins.
    Set-UiTestEnvironment -CaptureDirectory $verificationRoot -Isolated -ScaleFactor '2'
    & $ctest --test-dir $binaryRoot --output-on-failure --output-junit (Join-Path $verificationRoot 'tests.xml')
    if ($LASTEXITCODE -ne 0) { throw 'Single-file UI/protocol tests failed.' }

    # Only one executable is copied to a fresh directory containing spaces and Chinese.
    $smokeRoot = Join-Path ([IO.Path]::GetTempPath()) ("ydi-portable-$([Guid]::NewGuid().ToString('N')) 便携验证")
    New-Item -ItemType Directory -Path $smokeRoot | Out-Null
    $smokeExe = Join-Path $smokeRoot 'YDI_TTS_Portable.exe'
    Copy-Item -LiteralPath $binary -Destination $smokeExe
    foreach ($platform in @('offscreen', 'windows')) {
        Invoke-AppSmokeTest -Executable $smokeExe -Platform $platform
    }
    $smokeFiles = @(Get-ChildItem -LiteralPath $smokeRoot -Force)
    if ($smokeFiles.Count -ne 1 -or $smokeFiles[0].Name -ne 'YDI_TTS_Portable.exe') {
        throw "Standalone startup created unexpected sidecar files; inspect $smokeRoot"
    }
    # Remove just our executable copy and its now-empty temporary directory.
    Remove-Item -LiteralPath $smokeExe
    Remove-Item -LiteralPath $smokeRoot

    $releaseExe = Join-Path $releaseRoot 'YDI_TTS_Portable.exe'
    Copy-Item -LiteralPath $binary -Destination $releaseExe -Force
    $exeInfo = Get-Item -LiteralPath $releaseExe
    [ordered]@{
        executable = $releaseExe
        bytes = $exeInfo.Length
        sha256 = (Get-FileHash -LiteralPath $releaseExe -Algorithm SHA256).Hash
        imports = $dependencies
        uiAndProtocolTests = 'passed'
        isolatedOffscreenStartup = 'passed'
        isolatedWindowsStartup = 'passed'
        sidecarFilesCreated = 0
        verifiedAt = (Get-Date).ToString('o')
    } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $verificationRoot 'portable-verification.json') -Encoding utf8
    Write-Output "Portable executable: $releaseExe"
    Write-Output ("Size: {0:N2} MiB" -f ($exeInfo.Length / 1MB))
    Write-Output 'Static imports, UI/protocol tests, isolated Windows/offscreen startup and no-sidecar check: passed.'
} finally {
    Restore-BuildEnvironment -Snapshot $previousEnvironment
}
