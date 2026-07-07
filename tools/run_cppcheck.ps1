$ErrorActionPreference = "Stop"

$cppcheck = Get-Command cppcheck -ErrorAction SilentlyContinue
if (-not $cppcheck) {
    Write-Error "cppcheck was not found. Install Cppcheck or add cppcheck.exe to PATH."
}

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$sourceRoot = Join-Path $root "app\src"
$buildRoot = Join-Path $root "app\build"
$compileCommands = Join-Path $buildRoot "Desktop_Qt_6_8_3_MSVC2022_64bit-Debug\compile_commands.json"

$args = @(
    "--enable=warning,style,performance,portability",
    "--inline-suppr",
    "--suppress=missingIncludeSystem",
    "--suppress=unusedFunction",
    "--error-exitcode=1"
)

if (Test-Path -LiteralPath $compileCommands) {
    $args += "--project=$compileCommands"
} else {
    $args += "--std=c++17"
    $args += $sourceRoot
}

Write-Host "Running cppcheck for app/src. Third-party and build outputs are intentionally excluded."
& $cppcheck.Source @args
