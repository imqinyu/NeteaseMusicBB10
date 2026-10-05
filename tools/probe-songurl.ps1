# 用 app 的完整请求头复现播放地址接口（gzip 由 curl 自动解压）
$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$ua = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36'
$url = 'http://music.163.com/api/song/enhance/player/url?br=128000&ids=%5B2690164389%5D'

Write-Host '=== 1. 完整 app cookie（含 os/appver 等） ==='
$r = curl.exe -s -m 15 --compressed -H "Cookie: MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease" -H 'Referer: http://music.163.com/' -A $ua $url
$r.Substring(0, [Math]::Min(400, $r.Length))

Write-Host ''
Write-Host '=== 2. 纯 MUSIC_U（对照） ==='
$r2 = curl.exe -s -m 15 --compressed -H "Cookie: MUSIC_U=$musicU" -A $ua $url
$r2.Substring(0, [Math]::Min(400, $r2.Length))
