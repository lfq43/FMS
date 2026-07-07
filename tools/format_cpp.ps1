param(
    [switch]$Check
)

$ErrorActionPreference = "Stop"

$clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
$clangFormatPath = $null

if ($clangFormat) {
    $clangFormatPath = $clangFormat.Source
} else {
    $candidatePaths = @(
        "C:\Program Files\LLVM\bin\clang-format.exe",
        "C:\Program Files (x86)\LLVM\bin\clang-format.exe",
        "D:\LLVM\bin\clang-format.exe"
    )

    foreach ($candidatePath in $candidatePaths) {
        if (Test-Path -LiteralPath $candidatePath) {
            $clangFormatPath = $candidatePath
            break
        }
    }
}

if (-not $clangFormatPath) {
    Write-Error "clang-format was not found. Install LLVM/Clang or add clang-format.exe to PATH."
}

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$sourceRoot = Join-Path $root "app\src"
$files = Get-ChildItem -Path $sourceRoot -Recurse -File -Include *.cpp,*.h,*.hpp

if (-not $files) {
    Write-Host "No C++ source files found under app/src."
    exit 0
}

if ($Check) {
    $failed = $false
    foreach ($file in $files) {
        $formatted = & $clangFormatPath --style=file $file.FullName
        $current = Get-Content -Raw -LiteralPath $file.FullName
        if ($formatted -ne $current) {
            Write-Host "Needs formatting: $($file.FullName)"
            $failed = $true
        }
    }

    if ($failed) {
        exit 1
    }

    Write-Host "clang-format check passed for app/src."
    exit 0
}

foreach ($file in $files) {
    & $clangFormatPath -i --style=file $file.FullName
}

Write-Host "Formatted C++ source files under app/src."
