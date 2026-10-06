# =============================================================================
#  fix-release-artifact.ps1
#
#  解决的问题：
#    Device-Release 编出来的产物会落进一层同名的子目录 ——
#        arm/o.le-v7/o.le-v7/NeteaseMusic.so
#    而 bar-descriptor.xml 的 Device-Release 配置要的是
#        arm/o.le-v7/NeteaseMusic.so
#    两者对不上，打包就报：
#        Packaging failed:1
#        Error: Invalid asset path "arm/o.le-v7/NeteaseMusic.so"
#
#  为什么 fix-destdir.ps1 不够：
#    那个脚本改的是生成的 Makefile 里 DESTDIR / OBJECTS_DIR 这类【变量的那几行】。
#    但 qmake 在生成 Makefile 时，已经把 o.le-v7/ 这个前缀【展开写死】进了
#    OBJECTS / MOC / 链接规则等具体行里，改变量对它们不起作用。
#    Debug 分支（TEMPLATE=app）走的那套规则没被展开，所以一直正常；
#    Release 分支（TEMPLATE=lib，产物是 .so）才会踩到。
#
#  所以这里在编译【之后】把产物搬到 bar-descriptor.xml 期望的位置。
#  在根 Makefile 的 Device-Release 目标里被调用。
#
#  用法：powershell -NoProfile -ExecutionPolicy Bypass -File fix-release-artifact.ps1
# =============================================================================

$ErrorActionPreference = 'Stop'

# 工程根（本脚本在 <root>/build/ 下）
$root = Split-Path -Parent $PSScriptRoot

$nested = Join-Path $root 'arm/o.le-v7/o.le-v7'   # qmake 实际写出的位置
$destDir = Join-Path $root 'arm/o.le-v7'          # 描述文件期望的位置

# 打包需要 .so；.sym 跟着一起搬，方便出问题时看符号
$names = @('NeteaseMusic.so', 'NeteaseMusic.so.sym')

$moved = 0
foreach ($n in $names) {
    $src = Join-Path $nested $n
    if (Test-Path $src) {
        Copy-Item -Force $src (Join-Path $destDir $n)
        $moved++
    }
}

if ($moved -eq 0) {
    # 不报错退出：让构建的整体结果由 make 自己决定，
    # 这里只负责"能搬就搬"，真出问题看编译输出。
    Write-Host 'fix-release-artifact: 没找到嵌套产物（构建可能失败了）'
} else {
    Write-Host ('fix-release-artifact: 已搬移 {0} 个文件到 arm/o.le-v7/' -f $moved)
}

exit 0
