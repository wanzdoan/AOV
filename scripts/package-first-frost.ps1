$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
& cmake --build "$project/build/cmake_arm64-v8a" --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Android build failed' }
$library = Join-Path $project 'build/cmake_arm64-v8a/libaov_zygisk.so'
$stage = Join-Path $project ('build/frost-stage-' + [IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $stage | Out-Null
foreach ($file in @('module.prop','customize.sh','post-fs-data.sh','post-mount.sh','service.sh')) {
    Copy-Item -LiteralPath (Join-Path $project $file) -Destination $stage
}
foreach ($folder in @('META-INF','webroot')) {
    Copy-Item -LiteralPath (Join-Path $project $folder) -Destination $stage -Recurse
}
New-Item -ItemType Directory -Path "$stage/zygisk" | Out-Null
Copy-Item -LiteralPath $library -Destination "$stage/zygisk/arm64-v8a.so"
Copy-Item -LiteralPath $library -Destination "$project/zygisk/arm64-v8a.so" -Force

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipPath = Join-Path $project 'aov_zygisk_v2.6.0_first_frost_r2.zip'
$stream = [IO.File]::Open($zipPath,[IO.FileMode]::Create)
$zip = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in Get-ChildItem -LiteralPath $stage -File -Recurse) {
        $entry = $file.FullName.Substring($stage.Length + 1).Replace('\','/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$entry,[IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $zip.Dispose(); $stream.Dispose() }

$zip = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    foreach ($required in @('module.prop','zygisk/arm64-v8a.so','META-INF/com/google/android/update-binary','webroot/index.html')) {
        if (-not $zip.GetEntry($required)) { throw "Missing ZIP entry: $required" }
    }
    foreach ($entry in $zip.Entries) {
        if ($entry.FullName.Contains('\')) { throw 'ZIP contains backslash paths' }
    }
    $entryStream = $zip.GetEntry('zygisk/arm64-v8a.so').Open()
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $entryHash = [BitConverter]::ToString($sha.ComputeHash($entryStream)).Replace('-','') }
    finally { $entryStream.Dispose(); $sha.Dispose() }
    if ($entryHash -ne (Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash) {
        throw 'Packaged library differs from build output'
    }
} finally { $zip.Dispose() }
Write-Output "Package verified: $zipPath"
Write-Output "Retained staging directory: $stage"
Get-FileHash -LiteralPath $zipPath -Algorithm SHA256
