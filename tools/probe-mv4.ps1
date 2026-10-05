# Check paging params on /api/mv/all and cloud song field names (ASCII only)
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
    Write-Host ("    " + $r.Substring(0, [Math]::Min(400, $r.Length)))
    Write-Host ""
}

Probe "mv-all-paged" "http://music.163.com/api/mv/all" @("limit=5", "offset=0")
Probe "mv-first-paged" "http://music.163.com/api/mv/first" @("limit=5", "offset=0")
Probe "mv-url-real" "http://music.163.com/api/song/enhance/play/mv/url" @("id=10892044", "r=1080")
Probe "mv-url-real-480" "http://music.163.com/api/song/enhance/play/mv/url" @("id=10892044", "r=480")
