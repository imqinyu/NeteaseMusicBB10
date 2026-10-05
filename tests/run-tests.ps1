#requires -Version 2.0
<#
    Host-side verification script (development only; NOT part of the BB10 project).

    Purpose: build and run the tests under tests/ with desktop Qt 4.8 + mingw,
    so the pure logic layer (JSON / HTTP / session / models / parsers / api)
    can be verified without a BB10 device.

    Two traps that are already handled here -- do not reintroduce them:
      1. C:\QtSDK\mingw\bin MUST be on PATH. gcc's cc1plus child process needs the
         DLLs in that directory; without them it exits silently with
         0xC0000135 (STATUS_DLL_NOT_FOUND) and prints nothing at all.
      2. Every header containing Q_OBJECT must go through moc first, otherwise the
         link fails with "undefined reference to vtable for ...".

    Usage (from the NeteaseMusic directory):
        powershell -ExecutionPolicy Bypass -File tests\run-tests.ps1

    NOTE: this file is saved as UTF-8 with BOM on purpose. Windows PowerShell 5.1
    reads .ps1 files as ANSI (GBK on zh-CN) when there is no BOM, which corrupts
    any non-ASCII text and can break parsing.
#>

param(
    [string]$Only = ''
)

$ErrorActionPreference = 'Stop'

$root   = Split-Path -Parent $PSScriptRoot
$qt     = 'C:\QtSDK\Desktop\Qt\4.8.1\mingw'
$mingw  = 'C:\QtSDK\mingw\bin'
$moc    = Join-Path $qt 'bin\moc.exe'
$build  = Join-Path $PSScriptRoot 'build'
$tmp    = Join-Path $PSScriptRoot 'tmp'
$mocOut = Join-Path $build 'moc'

foreach ($p in @($qt, $mingw, $moc)) {
    if (-not (Test-Path $p)) {
        throw ("Toolchain component not found: " + $p + " -- edit `$qt / `$mingw in this script.")
    }
}

New-Item -ItemType Directory -Force -Path $build, $tmp, $mocOut | Out-Null
$env:PATH = "$mingw;$qt\bin;$env:PATH"

$srcRoot = Join-Path $root 'src'
$nmDir   = Join-Path $srcRoot 'nm'
$includes = @(
    # stubs 放最前面：用它顶掉真正的 bb/cascades/*（开发机上没有）
    "-I$PSScriptRoot\stubs",
    "-I$qt\include",
    "-I$qt\include\QtCore",
    "-I$qt\include\QtNetwork",
    "-I$nmDir",
    # MusicController.hpp 在 src 根下
    "-I$srcRoot"
)

# ---------------------------------------------------------------- backend sources
$sources = @(
    'util\NmJson.cpp',
    'util\NmString.cpp',
    'net\NmCookieStore.cpp',
    'net\NmHttpClient.cpp',
    'session\NmSession.cpp',
    'model\NmItems.cpp',
    'model\NmParsers.cpp',
    'api\NmApi.cpp'
) | ForEach-Object { Join-Path $nmDir $_ } | Where-Object { Test-Path $_ }

# ------------------------------------------------- moc: headers that use Q_OBJECT
$headersWithQObject = Get-ChildItem $nmDir -Recurse -Filter '*.hpp' |
    Where-Object { (Get-Content $_.FullName -Raw) -match 'Q_OBJECT' }

$mocSources = @()
$mocNames = @()
$mocErrorLog = Join-Path $tmp 'moc.log'
foreach ($h in $headersWithQObject) {
    $out = Join-Path $mocOut ('moc_' + $h.BaseName + '.cpp')
    # Run moc through cmd.exe on purpose: Qt tools print
    # "Untested Windows version 6.2 detected!" on stderr, and PowerShell 5.1
    # turns native-command stderr into a terminating error under
    # $ErrorActionPreference='Stop'.
    cmd /c "`"$moc`" `"$($h.FullName)`" -o `"$out`" 2> `"$mocErrorLog`""
    if ($LASTEXITCODE -ne 0) {
        $details = ''
        if (Test-Path $mocErrorLog) { $details = (Get-Content $mocErrorLog -Raw) }
        throw ("moc failed for " + $h.FullName + ":" + $details)
    }
    $mocSources += $out
    $mocNames += $h.BaseName
}

Write-Host ("Backend sources: " + $sources.Count + ", moc sources: " + $mocSources.Count) -ForegroundColor DarkGray
Write-Host ("moc: " + ($mocNames -join ', ')) -ForegroundColor DarkGray

# ---------------------------------------------------------------------------
# Compile-only check for MusicController.
# 它依赖 bb/cascades/ArrayDataModel（tests\stubs 里有替身），能在开发机上
# 做编译检查，及时发现它与逻辑层之间的签名漂移。
# ---------------------------------------------------------------------------
$controllerSrc = Join-Path $srcRoot 'MusicController.cpp'
if (Test-Path $controllerSrc) {
    Write-Host "### Compile check: MusicController.cpp" -ForegroundColor Cyan
    $cLog = Join-Path $tmp 'MusicController.build.log'
    & "$mingw\g++.exe" -c -Wall "-I$PSScriptRoot\stubs" @includes `
        -o (Join-Path $build 'MusicController.o') $controllerSrc 2>&1 |
        Tee-Object -FilePath $cLog | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "### COMPILE FAILED: MusicController.cpp" -ForegroundColor Red
        Get-Content $cLog |
            Where-Object { $_ -match ': (error|warning)' } |
            Select-Object -First 40 |
            ForEach-Object { Write-Host ("    " + $_) }
        exit 1
    }
    Write-Host "### OK: MusicController.cpp" -ForegroundColor Green
}

# ---------------------------------------------------------------- test targets
$tests = @(
    @{ Name = 'test_core'; Src = Join-Path $PSScriptRoot 'test_core.cpp' }
)

$failed = 0
$ran = 0
foreach ($t in $tests) {
    if ($Only -ne '' -and $t.Name -ne $Only) { continue }
    if (-not (Test-Path $t.Src)) {
        Write-Host ("Skip " + $t.Name + " (source missing)") -ForegroundColor Yellow
        continue
    }

    $exe = Join-Path $build ($t.Name + '.exe')
    Write-Host ""
    Write-Host ("### Build " + $t.Name) -ForegroundColor Cyan

    $log = Join-Path $tmp ($t.Name + '.build.log')
    & "$mingw\g++.exe" -o $exe @includes $t.Src @sources @mocSources "-L$qt\lib" -lQtCore4 -lQtNetwork4 2>&1 |
        Tee-Object -FilePath $log | Out-Null
    $buildExit = $LASTEXITCODE

    if ($buildExit -ne 0) {
        Write-Host ("### BUILD FAILED: " + $t.Name) -ForegroundColor Red
        Get-Content $log |
            Where-Object { $_ -match 'error|undefined reference' } |
            Select-Object -First 40 |
            ForEach-Object { Write-Host ("    " + $_) }
        $failed++
        continue
    }

    Write-Host ("### Run " + $t.Name) -ForegroundColor Cyan
    $ran++

    # 运行目录必须是 tests\（夹具按相对路径 fixtures\ 读）
    # stderr 走日志文件：Qt 会打 "Untested Windows version" 警告
    $runLog = Join-Path $tmp ($t.Name + '.run.log')
    Push-Location $PSScriptRoot
    cmd /c "`"$exe`" 2> `"$runLog`""
    $runExit = $LASTEXITCODE
    Pop-Location

    if (Test-Path $runLog) {
        $warnings = Get-Content $runLog | Where-Object { $_ -notmatch 'Untested Windows version' }
        foreach ($w in $warnings) { Write-Host ("    [stderr] " + $w) -ForegroundColor DarkYellow }
    }

    if ($runExit -ne 0) {
        Write-Host ("### TEST FAILED: " + $t.Name + " (exit=" + $runExit + ")") -ForegroundColor Red
        $failed++
    } else {
        Write-Host ("### PASS: " + $t.Name) -ForegroundColor Green
    }
}

if ($failed -gt 0) {
    Write-Host ""
    Write-Host ("$failed / $ran test targets failed") -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------------------
# NmTr 十六进制校验（中文乱码防线：手写的 hex 必须能正确解码）
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host '### NmTr hex check' -ForegroundColor Cyan
& (Join-Path $root 'tools\check-nmtr.ps1') (Join-Path $root 'src')
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "NmTr hex check failed" -ForegroundColor Red
    exit 1
}

Write-Host ""
if ($ran -gt 0) {
    Write-Host ("All passed ($ran test targets)") -ForegroundColor Green
}
exit 0
