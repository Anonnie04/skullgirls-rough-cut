# Optional Windows controller runtime. Uses SDL's native Steam Controller BLE driver.
param(
    [ValidateSet('x86','x64')][string]$Architecture='x86',
    [string]$Destination=(Join-Path $PSScriptRoot '../runtime')
)
$ErrorActionPreference='Stop'
$version='3.4.20'
$hashes=@{
    x86='f6fbf5afa42195a01b1da8adf25567377d0e212f4b2b6770923903eb3feea254'
    x64='368bed8b24e44ad9ea372990814f2343fcfb9e468abb7f8f4a3bc146c7304752'
}
$target=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Destination)
$archive=[IO.Path]::GetTempFileName()
try {
    Invoke-WebRequest -Uri "https://github.com/libsdl-org/SDL/releases/download/release-$version/SDL3-$version-win32-$Architecture.zip" -OutFile $archive
    if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hashes[$Architecture]){throw 'SDL3 archive checksum mismatch.'}
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip=[IO.Compression.ZipFile]::OpenRead($archive)
    try {
        New-Item -ItemType Directory -Force -Path $target | Out-Null
        foreach($item in @(@('SDL3.dll','SDL3.dll'),@('LICENSE.txt','LICENSE-SDL3.txt'))){
            $entry=$zip.GetEntry($item[0]);if($null -eq $entry){throw "Missing $($item[0]) in SDL3 archive."}
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,(Join-Path $target $item[1]),$true)
        }
    } finally {$zip.Dispose()}
    Write-Host "Installed verified SDL $version ($Architecture) to $target"
} finally {Remove-Item -LiteralPath $archive -ErrorAction SilentlyContinue}
