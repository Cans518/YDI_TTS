#requires -Version 7.0
[CmdletBinding(SupportsShouldProcess)]
param(
    [ValidateSet('Workspace', 'Temporary', 'All')]
    [string]$Scope = 'Workspace'
)

$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
$targets = @()
if ($Scope -in @('Workspace', 'All')) {
    foreach ($name in @('build', 'out', 'dist')) {
        $targets += @{ Root = $sourceRoot; Path = Join-Path $sourceRoot $name }
    }
    foreach ($directory in @('scripts', 'tests')) {
        Get-ChildItem -LiteralPath (Join-Path $sourceRoot $directory) -Directory -Recurse -Filter '__pycache__' |
            ForEach-Object { $targets += @{ Root = $sourceRoot; Path = $_.FullName } }
    }
}
if ($Scope -in @('Temporary', 'All')) {
    foreach ($name in @('ydi-tts-verify', 'ydi-tts-portable')) {
        $targets += @{ Root = $tempRoot; Path = Join-Path $tempRoot $name }
    }
}

foreach ($target in $targets) {
    $path = [IO.Path]::GetFullPath($target.Path)
    $prefix = $target.Root.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (!$path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Cleanup target is outside its expected root: $path"
    }
    if (!(Test-Path -LiteralPath $path)) { continue }
    $item = Get-Item -LiteralPath $path -Force
    if ($item.LinkType) { throw "Cleanup target is a filesystem link: $path" }
    $links = Get-ChildItem -LiteralPath $path -Recurse -Force | Where-Object { $_.LinkType }
    if ($links) { throw "Cleanup target contains filesystem links: $path" }
    $size = (Get-ChildItem -LiteralPath $path -Recurse -File -Force | Measure-Object Length -Sum).Sum
    if ($PSCmdlet.ShouldProcess($path, ('Remove generated files ({0:N1} MB)' -f ($size / 1MB)))) {
        Remove-Item -LiteralPath $path -Recurse -Force
        Write-Output "Removed: $path"
    }
}
