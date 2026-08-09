param(
    [ValidateSet("Debug", "Release")]
    [string]$Tipo = "Release",
    [switch]$SemAudio
)

$ErrorActionPreference = "Stop"
$Audio = if ($SemAudio) { "OFF" } else { "ON" }
$Pasta = "build/windows"

cmake -S . -B $Pasta -G "MinGW Makefiles" `
    -DCMAKE_BUILD_TYPE=$Tipo `
    -DMYTHARA_AUDIO=$Audio `
    -DMYTHARA_AVISOS_COMO_ERROS=ON
cmake --build $Pasta --parallel
ctest --test-dir $Pasta --output-on-failure

Write-Host "Mythara compilada em $Pasta/mythara.exe"
