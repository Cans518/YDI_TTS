#requires -Version 7.0

function Copy-BuildSources {
    param([string]$SourceRoot, [string]$StageRoot)

    $sourcePath = (Resolve-Path -LiteralPath $SourceRoot).Path
    New-Item -ItemType Directory -Force -Path $StageRoot | Out-Null
    $stagePath = (Resolve-Path -LiteralPath $StageRoot).Path
    $stagePrefix = $stagePath + [IO.Path]::DirectorySeparatorChar
    $sourcePrefix = $sourcePath + [IO.Path]::DirectorySeparatorChar
    if ($stagePath.Equals($sourcePath, [StringComparison]::OrdinalIgnoreCase) -or
        $stagePath.StartsWith($sourcePrefix, [StringComparison]::OrdinalIgnoreCase) -or
        $sourcePath.StartsWith($stagePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The staging directory and source repository must not overlap.'
    }
    $files = @(Get-Item -LiteralPath (Join-Path $sourcePath 'CMakeLists.txt'))
    foreach ($directory in @('src', 'resources', 'tests')) {
        $files += Get-ChildItem -LiteralPath (Join-Path $sourcePath $directory) -Recurse -File
    }

    $relativePaths = @()
    foreach ($file in $files) {
        $relative = [IO.Path]::GetRelativePath($sourcePath, $file.FullName)
        $destination = [IO.Path]::GetFullPath((Join-Path $stagePath $relative))
        if (!$destination.StartsWith($stagePrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Source copy target is outside the staging directory: $relative"
        }
        $relativePaths += $relative
        New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
        if (!(Test-Path -LiteralPath $destination) -or
            (Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $destination).Hash) {
            Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
        }
    }

    # Only remove files recorded by an earlier invocation of this staging helper.
    $manifest = Join-Path $stagePath '.source-manifest.json'
    if (Test-Path -LiteralPath $manifest) {
        foreach ($relative in (Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json)) {
            if ($relativePaths -contains $relative) { continue }
            $obsolete = [IO.Path]::GetFullPath((Join-Path $stagePath $relative))
            if (!$obsolete.StartsWith($stagePrefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Staging manifest contains an invalid path: $relative"
            }
            if (Test-Path -LiteralPath $obsolete -PathType Leaf) { Remove-Item -LiteralPath $obsolete }
        }
    }
    ConvertTo-Json -InputObject $relativePaths | Set-Content -LiteralPath $manifest -Encoding utf8
}

function Clear-VerificationArtifacts {
    param([string]$Directory)

    foreach ($artifact in Get-ChildItem -LiteralPath $Directory -File) {
        if ($artifact.Name -match '^(0[0-9]|1[0-7])-[a-z-]+\.png$|^(tests\.xml|portable-verification\.json|dependencies\.txt)$') {
            Remove-Item -LiteralPath $artifact.FullName
        }
    }
}

function Save-BuildEnvironment {
    $snapshot = @{}
    foreach ($name in @('PATH', 'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QT_QPA_PLATFORM', 'QT_QPA_FONTDIR', 'QT_SCALE_FACTOR', 'QT_DEBUG_PLUGINS', 'QT_FORCE_STDERR_LOGGING', 'YDI_UI_CAPTURE_DIR')) {
        $snapshot[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    }
    return $snapshot
}

function Restore-BuildEnvironment {
    param([hashtable]$Snapshot)

    foreach ($name in $Snapshot.Keys) {
        [Environment]::SetEnvironmentVariable($name, $Snapshot[$name], 'Process')
    }
}

function Set-UiTestEnvironment {
    param([string]$CaptureDirectory, [switch]$Isolated, [string]$ScaleFactor)

    if ($Isolated) { $env:PATH = "$env:WINDIR/System32;$env:WINDIR" }
    $env:QT_PLUGIN_PATH = $null
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $null
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_DEBUG_PLUGINS = '1'
    $env:QT_FORCE_STDERR_LOGGING = '1'
    $env:QT_QPA_FONTDIR = Join-Path $env:WINDIR 'Fonts'
    $env:YDI_UI_CAPTURE_DIR = $CaptureDirectory
    if ($ScaleFactor) { $env:QT_SCALE_FACTOR = $ScaleFactor }
}

function Invoke-AppSmokeTest {
    param([string]$Executable, [string]$Platform = 'offscreen')

    $env:QT_QPA_PLATFORM = $Platform
    $process = Start-Process -FilePath $Executable -ArgumentList '--smoke-test' `
        -WorkingDirectory (Split-Path $Executable -Parent) -WindowStyle Hidden -PassThru
    try {
        if (!$process.WaitForExit(15000)) {
            $process.Kill()
            $process.WaitForExit(5000) | Out-Null
            throw "Application startup timed out on $Platform."
        }
        if ($process.ExitCode -ne 0) { throw "Application startup failed on ${Platform}: $($process.ExitCode)" }
    } finally {
        $process.Dispose()
    }
}
