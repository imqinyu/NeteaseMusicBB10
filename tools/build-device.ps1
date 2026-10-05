# 构建 Device-Debug 配置（用法见根 Makefile 文件头）
$ndk = 'C:\NDKFiles'
$env:QNX_HOST   = "$ndk\host_10_3_1_12\win32\x86"
$env:QNX_TARGET = "$ndk\target_10_3_1_995\qnx6"
$env:PATH = "$env:QNX_HOST\usr\bin;$env:PATH"
Set-Location 'D:\BBWorkspace\NeteaseMusic'
make -j4 Device-Debug
