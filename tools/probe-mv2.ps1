# Probe MV endpoints (ASCII only)
$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

function Probe($name, $url, $params) {
    Write-Host "=== $name ==="
    $a = @(); $a += $common
    if ($params) { $a += "-G"; foreach ($p in $params) { $a += @("-d", $p) } }
    $a += $url
    $r = & curl.exe @a
    if ($r.Length -lt 2) { Write-Host "    empty"; Write-Host ""; return }
    Write-Host ("    " + $r.Substring(0, [Math]::Min(700, $r.Length)))
    Write-Host ""
}

# artist MV list (use a subscribed artist id from earlier probe: 21377)
Probe "artist-mv-21377" "http://music.163.com/api/artist/mv/21377" @("limit=3", "offset=0")
Probe "artist-mv-alt"   "http://music.163.com/api/artist/mv" @("id=21377", "limit=3", "offset=0")
Probe "search-mv-old"   "http://music.163.com/api/search/get" @("s=Chris Brown", "type=1004", "limit=3", "offset=0")
Probe "cloudsearch-mv"  "http://music.163.com/api/cloudsearch/pc" @("s=Chris Brown", "type=1004", "limit=3", "offset=0")
Probe "mv-detail-5436713" "http://music.163.com/api/mv/detail" @("id=5436713")
Probe "mv-url-5436713"    "http://music.163.com/api/mv/url" @("id=5436713", "r=1080")
Probe "mv-play-url"       "http://music.163.com/api/song/enhance/play/mv/url" @("id=5436713", "r=1080")
