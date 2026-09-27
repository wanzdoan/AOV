$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$output = Join-Path $project ('build/config-test-' + [IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $output | Out-Null
& clang++ -std=c++17 -O0 -g "-I$project/jni" "$project/tests/config_store_test.cpp" "$project/jni/src/features/aov_config.cpp" "$project/jni/src/features/config_store.cpp" -o "$output/config_store_test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Persistence test compilation failed' }
foreach ($mode in @('write','read','disable','disabled-read','failures')) {
    & "$output/config_store_test.exe" $mode "$output/app-data"
    if ($LASTEXITCODE -ne 0) { throw "Persistence test failed: $mode" }
}
