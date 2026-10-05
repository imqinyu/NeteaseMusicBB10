# Probe MV / cloud / failing cover (ASCII only: this PS 5.1 reads BOM-less UTF-8 as GBK)
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
    Write-Host ("    " + $r.Substring(0, [Math]::Min(500, $r.Length)))
    Write-Host ""
}

Probe "cloud-get"  "http://music.163.com/api/v1/cloud/get" @("limit=5", "offset=0")
Probe "search-mv"  "http://music.163.com/api/search/get/web" @("s=Chris Brown", "type=1004", "limit=3", "offset=0")
Probe "mv-detail"  "http://music.163.com/api/mv/detail" @("id=10871401")
Probe "mv-url"     "http://music.163.com/api/mv/url" @("id=10871401", "r=1080")
Probe "mv-sublist" "http://music.163.com/api/mv/sublist" @("limit=3", "offset=0")

$amp = [char]38
$bad = "http://p1.music.126.net/PoHAvY1rcp-AG4UdwUtHUA==/109951173972716676.jpg?imageView=1" + $amp + "thumbnail=800y800" + $amp + "enlarge=1%7CimageView" + $amp + "thumbnail=800y800"
Write-Host "=== cover: as-is (with Referer) ==="
$r1 = & curl.exe -s -o NUL -w "%{http_code}" -H "User-Agent: $ua" -H "Referer: http://music.163.com/" -H "Accept: image/*,*/*;q=0.8" $bad
Write-Host "  status=$r1"
Write-Host "=== cover: strip query ==="
$base = $bad.Split("?")[0]
$r2 = & curl.exe -s -o NUL -w "%{http_code} %{size_download}" -H "User-Agent: $ua" -H "Referer: http://music.163.com/" -H "Accept: image/*,*/*;q=0.8" $base
Write-Host "  status size=$r2"
Write-Host "=== cover: as-is, no Referer ==="
$r3 = & curl.exe -s -o NUL -w "%{http_code} %{size_download}" -H "User-Agent: $ua" -H "Accept: image/*,*/*;q=0.8" $bad
Write-Host "  status size=$r3"
Write-Host "=== cover: as-is, no Referer, no Accept ==="
$r4 = & curl.exe -s -o NUL -w "%{http_code} %{size_download}" -H "User-Agent: $ua" $bad
Write-Host "  status size=$r4"
