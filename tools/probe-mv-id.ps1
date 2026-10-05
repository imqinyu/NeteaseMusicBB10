# Check whether the MV ids from the list actually return a playable url
$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

foreach ($id in @(14689667, 14690387, 10892044)) {
    $a = @(); $a += $common; $a += "-G"; $a += @("-d", "id=$id"); $a += @("-d", "r=480")
    $a += "http://music.163.com/api/song/enhance/play/mv/url"
    $r = & curl.exe @a
    Write-Host "=== mv id=$id ==="
    Write-Host ("    " + $r.Substring(0, [Math]::Min(260, $r.Length)))
    Write-Host ""
}
