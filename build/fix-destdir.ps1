# =============================================================================
#  fix-destdir.ps1 - 把生成的 Makefile 里 DESTDIR / OBJECTS_DIR 改成当前目录
#  （从 BBTieba 移植，原理见其 README 第九节）
#
#  为什么需要这一步：
#    cascades10.prf 里这两个变量用的是相对「工程根」的写法：
#        DESTDIR     = o.le-v7-g/
#        OBJECTS_DIR = o.le-v7-g/.obj/
#    但 make 是在 arm/o.le-v7-g/ 里执行的，于是路径再解析一次，变成
#        arm/o.le-v7-g/o.le-v7-g/NeteaseMusic
#    多套了一层同名目录，产物位置就和 bar-descriptor.xml 对不上了。
#
#  用法：powershell -NoProfile -File fix-destdir.ps1 <Makefile 路径>
# =============================================================================

param(
    [Parameter(Mandatory = $true)]
    [string] $MakefilePath
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $MakefilePath)) {
    Write-Host ('fix-destdir: not found: ' + $MakefilePath)
    exit 1
}

$text = [System.IO.File]::ReadAllText($MakefilePath, [System.Text.UTF8Encoding]::new($false))
$original = $text

# 只改这几行；值一律指向当前目录（make 的工作目录即构建子目录）
$text = [regex]::Replace($text, '(?m)^DESTDIR\s*=.*$',     'DESTDIR       = .')
$text = [regex]::Replace($text, '(?m)^OBJECTS_DIR\s*=.*$', 'OBJECTS_DIR   = ./.obj/')
$text = [regex]::Replace($text, '(?m)^MOC_DIR\s*=.*$',     'MOC_DIR       = ./.moc/')
$text = [regex]::Replace($text, '(?m)^RCC_DIR\s*=.*$',     'RCC_DIR       = ./.rcc/')
$text = [regex]::Replace($text, '(?m)^UI_DIR\s*=.*$',      'UI_DIR        = ./.ui/')

# TARGET 也带上了同样的目录前缀（TARGET = o.le-v7-g/NeteaseMusic），
# 会把产物再塞回那个嵌套目录，所以前缀一并去掉：
#     o.le-v7-g/NeteaseMusic    -> NeteaseMusic
#     o.le-v7/NeteaseMusic.so   -> NeteaseMusic.so
$text = [regex]::Replace($text, '(?m)^(TARGET\s*=\s*)([^\r\n]*/)([^/\r\n]+)\s*$', '$1$3')

if ($text -ne $original) {
    [System.IO.File]::WriteAllText($MakefilePath, $text, (New-Object System.Text.UTF8Encoding($false)))
}

exit 0
