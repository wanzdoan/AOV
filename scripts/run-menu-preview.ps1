$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$output = Join-Path $project 'build/ui_preview'
New-Item -ItemType Directory -Force -Path $output | Out-Null
# Mechanical include substitution only; production UI code is tested verbatim.
$view = [IO.File]::ReadAllText((Join-Path $project 'jni/src/menu/menu.cpp'))
$view = $view -replace '(?m)^#include "(menu.hpp|../esp/esp.hpp|../../include/globals.hpp|../../include/game_actors.hpp)"\r?\n', ''
$view = $view.Replace('../../vendor/imgui/', '').Replace('../../include/aov_config.hpp','include/aov_config.hpp')
[IO.File]::WriteAllText((Join-Path $output 'menu_under_test.cpp'), $view, [Text.UTF8Encoding]::new($false))
$imgui = Join-Path $project 'jni/vendor/imgui'
& clang++ -std=c++17 -O0 -g "-I$output" "-I$imgui" "-I$project/jni" "$project/tests/menu_preview.cpp" "$project/jni/src/features/aov_config.cpp" "$project/jni/src/features/config_store.cpp" "$imgui/imgui.cpp" "$imgui/imgui_draw.cpp" "$imgui/imgui_widgets.cpp" "$imgui/imgui_tables.cpp" -lopengl32 -lgdi32 -luser32 -o "$output/menu_preview.exe"
if ($LASTEXITCODE -ne 0) { throw 'Preview compilation failed' }
Push-Location $output
try {
    & ./menu_preview.exe
    if ($LASTEXITCODE -ne 0) { throw "Preview checks failed: $LASTEXITCODE" }
    Add-Type -AssemblyName System.Drawing
    Get-ChildItem -LiteralPath $output -Filter '*.bmp' | ForEach-Object {
        $bitmap = [Drawing.Bitmap]::new($_.FullName)
        try { $bitmap.Save([IO.Path]::ChangeExtension($_.FullName, '.png'), [Drawing.Imaging.ImageFormat]::Png) }
        finally { $bitmap.Dispose() }
    }
} finally { Pop-Location }
