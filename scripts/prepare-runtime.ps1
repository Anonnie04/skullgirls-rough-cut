# Prepare a portable preview from an installed game and a local build.
param(
    [Parameter(Mandatory = $true)][string]$OriginalGame,
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../build'),
    [string]$Configuration = 'Release',
    [string]$Destination = (Join-Path $PSScriptRoot '../runtime')
)

$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path -LiteralPath $OriginalGame).ProviderPath
# Resolve relative destinations from PowerShell's current location, even before they exist.
$runtimeRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Destination)
$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).ProviderPath
# Check the build and both data folders before writing anything.
$executables = @('skullgirls-main-menu.exe', 'skullgirls-gfs.exe', 'skullgirls-sprite.exe', 'skullgirls-viewer.exe')
foreach ($name in $executables) {
    if (-not (Test-Path -LiteralPath (Join-Path $buildRoot "$Configuration/$name") -PathType Leaf)) {
        throw "Build $Configuration first; missing $name."
    }
}
foreach ($name in 'data01', 'data02') {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $name) -PathType Container)) { throw "Missing source $name." }
}
# Keep the generated runtime outside the installed game folder.
if ($runtimeRoot.TrimEnd('\') -eq $sourceRoot.TrimEnd('\') -or
    $runtimeRoot.StartsWith($sourceRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Choose a destination outside the original game folder.'
}
New-Item -ItemType Directory -Path $runtimeRoot -Force | Out-Null
# Keep the original directory layout and record each verified copy in the manifest.
$manifest = @()
foreach ($directory in 'data01', 'data02') {
    $sourceDirectory = Join-Path $sourceRoot $directory
    $targetDirectory = Join-Path $runtimeRoot $directory
    $indexDirectory = Join-Path $runtimeRoot "index/$directory"
    New-Item -ItemType Directory -Path $targetDirectory, $indexDirectory -Force | Out-Null
    foreach ($file in Get-ChildItem -LiteralPath $sourceDirectory -File -Recurse) {
        $relative = $file.FullName.Substring($sourceDirectory.Length).TrimStart('\', '/')
        $target = Join-Path $targetDirectory $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Write-Host "Copying and verifying $directory/$relative"
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force
        $expected = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        $actual = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        if ($actual -ne $expected) { throw "Copy verification failed: $directory/$relative" }
        $manifest += [pscustomobject]@{ Path = "$directory/$relative"; Bytes = $file.Length; SHA256 = $actual }
        # Write a searchable table of archive entries alongside the other indexes.
        if ($file.Extension -eq '.gfs') {
            $index = Join-Path $indexDirectory ($relative + '.tsv')
            New-Item -ItemType Directory -Path (Split-Path -Parent $index) -Force | Out-Null
            & (Join-Path $buildRoot "$Configuration/skullgirls-gfs.exe") $target | Set-Content -LiteralPath $index -Encoding UTF8
            if ($LASTEXITCODE -ne 0) { throw "Archive indexing failed: $directory/$relative" }
        }
    }
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runtimeRoot 'manifest.json') -Encoding UTF8
# Read the copied data02 archives to generate one animation index per playable fighter.
foreach ($character in 'filia', 'cerebella', 'peacock', 'parasoul', 'msfortune', 'painwheel') {
    $index = Join-Path $runtimeRoot "index/data02/$character-moves.tsv"
    & (Join-Path $buildRoot "$Configuration/skullgirls-sprite.exe") (Join-Path $runtimeRoot 'data02') $character animations |
        Set-Content -LiteralPath $index -Encoding UTF8
    if ($LASTEXITCODE -ne 0) { throw "Move indexing failed: $character" }
}
foreach ($name in $executables) {
    # The preview looks for data02 beside the executable.
    Copy-Item -LiteralPath (Join-Path $buildRoot "$Configuration/$name") -Destination (Join-Path $runtimeRoot $name) -Force
}
# Write the quick-start guide for the generated runtime.
@'
Skullgirls: Rough Cut - local runtime

Run skullgirls-main-menu.exe from this folder. It uses the adjacent data02.
data01 is preserved separately for inspection and older asset tools.
index/data01 and index/data02 contain searchable TSV archive listings.
index/data02/*-moves.tsv lists named animation records for all six playable fighters.
manifest.json records every copied file's size and verified SHA-256 hash.

This folder is local and ignored by Git. It can be moved as a unit.
'@ | Set-Content -LiteralPath (Join-Path $runtimeRoot 'README.txt') -Encoding UTF8
Write-Host "Ready: $runtimeRoot ($($manifest.Count) verified files)"
