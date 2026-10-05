# Probe MV list sources and cloud song fields (ASCII only)
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
    Write-Host ("    " + $r.Substring(0, [Math]::Min(600, $r.Length)))
    Write-Host ""
}

Probe "mv-all"     "http://music.163.com/api/mv/all" $null
Probe "mv-first"   "http://music.163.com/api/mv/first" @("limit=3", "offset=0")
Probe "mv-top"     "http://music.163.com/api/mv/top" @("limit=3", "offset=0")
Probe "search-1004-full" "http://music.163.com/api/search/get/web" @("csrfToken=", "hlpretag=<span class=`"s-fc2`">", "hlposttag=</span>", "s=Chris Brown", "type=1004", "offset=0", "total=true", "limit=3")
Probe "search-1004-get"  "http://music.163.com/api/search/get" @("csrfToken=", "s=Chris Brown", "type=1004", "offset=0", "total=true", "limit=3")
Probe "mv-sublist-full"  "http://music.163.com/api/mv/sublist" @("limit=10", "offset=0", "csrfToken=")
Probe "cloud-get-30"     "http://music.163.com/api/v1/cloud/get" @("limit=30", "offset=0", "csrfToken=")
