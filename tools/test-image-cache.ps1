param(
    [string]$QtRoot = 'C:\QtSDK\Desktop\Qt\4.8.1\mingw',
    [string]$MingwRoot = 'C:\QtSDK\mingw'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'tmp\image-cache-tests'
$qmake = Join-Path $QtRoot 'bin\qmake.exe'
$make = Join-Path $MingwRoot 'bin\mingw32-make.exe'
foreach ($tool in @($qmake, $make)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Required tool not found: $tool"
    }
}

function Invoke-CheckedNative {
    param([string]$Executable, [string[]]$Arguments)
    # Qt 4.8 qmake emits a harmless Windows-version warning on stderr.
    # Judge native tools by their exit code, rather than their output stream.
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $Executable @Arguments
        $nativeExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($nativeExitCode -ne 0) {
        throw "$Executable failed with exit code $nativeExitCode"
    }
}

$savedPath = $env:PATH
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
Push-Location -LiteralPath $buildDir
try {
    $env:PATH = "$MingwRoot\bin;$QtRoot\bin;$savedPath"
    Invoke-CheckedNative $qmake @('-o', 'Makefile',
        (Join-Path $projectRoot 'tests\image-cache.pro'),
        'CONFIG+=debug', 'CONFIG-=release')
    Invoke-CheckedNative $make @('-f', 'Makefile', 'debug')
    Invoke-CheckedNative (Join-Path $buildDir 'debug\test_image_cache.exe') @('-o', 'test-results.txt')
    Get-Content -LiteralPath (Join-Path $buildDir 'test-results.txt')
} finally {
    Pop-Location
    $env:PATH = $savedPath
}
