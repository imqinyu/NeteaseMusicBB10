# port-logic.ps1 - 把 BBTieba 已验证的纯逻辑层复制到 NeteaseMusic 并重命名
#
# 映射（大小写敏感，顺序不能变）：
#   tbFromHex -> nmFromHex   （小写前缀，必须先于 Tb->Nm 之外的规则没关系，但要在 Tb 规则前做以免重复替换）
#   Tb        -> Nm          （类名 / 宏 TbTr -> NmTr）
#   TB_       -> NM_         （TB_HAVE_ZLIB / include guard TB_XXX_HPP）
#   tieba     -> nm          （namespace / 注释）
#
# 用 UTF-8 无 BOM 写出（QNX gcc / 桌面 mingw 都按 UTF-8 读源码）。

$ErrorActionPreference = 'Stop'

$srcRoot = 'D:\BBWorkspace\BBTieba\src\tieba'
$dstRoot = 'D:\BBWorkspace\NeteaseMusic\src\nm'

# 文件映射：源相对路径 -> 目标相对路径
$files = @(
    @{ src = 'util\TbJson.hpp';        dst = 'util\NmJson.hpp' },
    @{ src = 'util\TbJson.cpp';        dst = 'util\NmJson.cpp' },
    @{ src = 'util\TbString.hpp';      dst = 'util\NmString.hpp' },
    @{ src = 'util\TbString.cpp';      dst = 'util\NmString.cpp' },
    @{ src = 'util\TbTr.hpp';          dst = 'util\NmTr.hpp' },
    @{ src = 'net\TbHttpClient.hpp';   dst = 'net\NmHttpClient.hpp' },
    @{ src = 'net\TbHttpClient.cpp';   dst = 'net\NmHttpClient.cpp' },
    @{ src = 'net\TbCookieStore.hpp';  dst = 'net\NmCookieStore.hpp' },
    @{ src = 'net\TbCookieStore.cpp';  dst = 'net\NmCookieStore.cpp' }
)

foreach ($dir in @('util', 'net', 'model', 'api', 'session')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $dstRoot $dir) | Out-Null
}

foreach ($f in $files) {
    $inPath  = Join-Path $srcRoot $f.src
    $outPath = Join-Path $dstRoot $f.dst

    if (-not (Test-Path $inPath)) {
        Write-Host ('port-logic: missing source: ' + $inPath)
        exit 1
    }

    $text = [IO.File]::ReadAllText($inPath)
    $text = $text -creplace 'tbFromHex', 'nmFromHex'
    $text = $text -creplace 'Tb', 'Nm'
    $text = $text -creplace 'TB_', 'NM_'
    $text = $text -creplace 'tieba', 'nm'

    [IO.File]::WriteAllText($outPath, $text, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host ('port-logic: ' + $f.src + ' -> ' + $f.dst)
}

Write-Host 'port-logic: done'
exit 0
