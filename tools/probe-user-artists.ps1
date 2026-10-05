# Probe: does /api/artist/sublist honor a "uid" param?
# Goal: the user-profile page wants to show ANOTHER user's followed artists.
#       If the endpoint ignores uid we would get MY artists -> can not do it.
# ASCII only (avoid any source-encoding surprises in the shell).
#
# ===== RESULT (2026-10) =====
#   A (no uid) / B (uid=other) / C (uid=me)  ->  IDENTICAL, all = MY artists.
#   => /api/artist/sublist IGNORES uid. Another user's followed artists
#      CAN NOT be fetched with this API. (Also tried, all failed:)
#        userId=other param ....... same as mine
#        /api/user/subcount ....... 404
#        /api/v1/user/subcount/<u>  404
#        /api/v1/artist/sublist/<u> 404
#        /api/user/artist/list?uid= 404
#        /api/v1/user/artist/<u> .. 404
#   What DOES work for other users:
#        /api/v1/user/detail/<u>  -> level, listenSongs, profile{nickname,
#                                    avatarUrl, signature, follows, followeds,
#                                    artistId/artistName if the user is a musician}
#        /api/user/playlist?uid=  -> their playlists
#   (field layout: see probe-user-detail.ps1)

$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

$mine  = "1935209761"   # me
$other = "446664973"    # a comment author (from device log)

function Probe($name, $url, $params) {
    Write-Host "=== $name ==="
    $a = @()
    $a += $common
    if ($params) { $a += "-G"; foreach ($p in $params) { $a += @("-d", $p) } }
    $a += $url
    $r = & curl.exe @a
    try { $j = $r | ConvertFrom-Json } catch {
        Write-Host ("    NOT JSON: " + $r.Substring(0, [Math]::Min(160, $r.Length)))
        Write-Host ""
        return
    }
    Write-Host ("    code={0}  top-level: {1}" -f $j.code, (($j.PSObject.Properties.Name) -join ", "))
    if ($j.PSObject.Properties.Name -contains 'data') {
        $d = @($j.data)
        Write-Host ("    data count = {0}" -f $d.Count)
        $n = [Math]::Min(6, $d.Count)
        for ($i = 0; $i -lt $n; $i++) {
            Write-Host ("      [{0}] id={1} name={2}" -f $i, $d[$i].id, $d[$i].name)
        }
    }
    Write-Host ""
}

Probe "A no-uid (should be MINE)"        "http://music.163.com/api/artist/sublist" @("limit=10", "offset=0")
Probe "B uid=$other (the other user)?"    "http://music.163.com/api/artist/sublist" @("uid=$other", "limit=10", "offset=0")
Probe "C uid=$mine (me, control)"         "http://music.163.com/api/artist/sublist" @("uid=$mine", "limit=10", "offset=0")
Probe "D user/detail?uid=$other"          "http://music.163.com/api/user/detail"    @("uid=$other")
