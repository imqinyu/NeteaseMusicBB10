# Probe comments: song (R_SO_4_) and playlist (A_PL_0_)  [ASCII only in labels]
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
    if (-not $r -or $r.Length -lt 2) { Write-Host "    empty"; Write-Host ""; return }
    Write-Host ("    " + $r.Substring(0, [Math]::Min(700, $r.Length)))
    Write-Host ""
}

# song id 14689667 (seen in device log) and a very common one 186856 (Jay Chou)
Probe "song-comments-14689667" "http://music.163.com/api/v1/resource/comments/R_SO_4_14689667" @("limit=3", "offset=0")
Probe "song-comments-186856"   "http://music.163.com/api/v1/resource/comments/R_SO_4_186856"   @("limit=3", "offset=0")
# playlist ids seen in device log
Probe "pl-comments-129627424"  "http://music.163.com/api/v1/resource/comments/A_PL_0_129627424" @("limit=3", "offset=0")
Probe "pl-comments-10119006626" "http://music.163.com/api/v1/resource/comments/A_PL_0_10119006626" @("limit=3", "offset=0")
# older style (no /api prefix) just in case
Probe "pl-comments-v1-nocookie" "http://music.163.com/v1/resource/comments/A_PL_0_129627424" @("limit=3", "offset=0")
