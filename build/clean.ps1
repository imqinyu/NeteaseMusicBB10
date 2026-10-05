# =============================================================================
#  clean.ps1 - 清理构建产物（从 BBTieba 移植）
#
#  为什么用脚本而不是直接写在 Makefile 里：
#    Windows 上 make 调用的 shell 是 sh（NDK 自带的），Makefile 配方里
#    不能用 cmd 的 rmdir/del。且 recipe 行里的 '#' 不是 make 注释，
#    中文注释会导致 "cannot execute binary file"。所以逻辑放这里。
#
#  ★ $ErrorActionPreference 必须是 Continue：SilentlyContinue 会把删除失败
#    静默吞掉，表现为「make clean 成功但目录还在」。
#
#  用法：
#    powershell -File build/clean.ps1             清构建产物
#    powershell -File build/clean.ps1 distclean   连生成的 Makefile 一起清
# =============================================================================

param(
    [string] $Mode = 'clean'
)

$ErrorActionPreference = 'Continue'

$scriptDir = $PSScriptRoot
if ([string]::IsNullOrEmpty($scriptDir)) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}
if ([string]::IsNullOrEmpty($scriptDir)) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
}
if ([string]::IsNullOrEmpty($scriptDir)) {
    Write-Host 'clean: cannot determine script directory ($PSScriptRoot and MyInvocation are both empty)'
    exit 1
}

$root = Split-Path -Parent $scriptDir
if ([string]::IsNullOrEmpty($root)) {
    Write-Host ('clean: cannot determine project root from ' + $scriptDir)
    exit 1
}

$failed = 0

# 构建目录
foreach ($d in @('arm', 'arm-p', 'x86')) {
    $p = Join-Path $root $d
    if (Test-Path $p) {
        try {
            Remove-Item -Path $p -Recurse -Force -ErrorAction Stop
        } catch {
            Write-Host ('clean: failed to remove ' + $d + ' : ' + $_.Exception.Message)
            $failed++
        }
    }
}

# 散落在工程根的库 / 符号文件
foreach ($pattern in @('libNeteaseMusic.so*', 'NeteaseMusic.so', 'NeteaseMusic.so.sym')) {
    Get-ChildItem -Path $root -Filter $pattern -Force -ErrorAction SilentlyContinue | ForEach-Object {
        try {
            Remove-Item -Path $_.FullName -Force -ErrorAction Stop
        } catch {
            Write-Host ('clean: failed to remove ' + $_.Name + ' : ' + $_.Exception.Message)
            $failed++
        }
    }
}

if ($Mode -eq 'distclean') {
    $p = Join-Path $root 'Makefile.qmake'
    if (Test-Path $p) {
        try {
            Remove-Item -Path $p -Force -ErrorAction Stop
        } catch {
            Write-Host ('clean: failed to remove Makefile.qmake : ' + $_.Exception.Message)
            $failed++
        }
    }
}

if ($failed -gt 0) {
    Write-Host ('clean: ' + $failed + ' item(s) could not be removed')
    exit 1
}

exit 0
