# Probe: does /api/user/playlist return a usable "subscribed" flag?
# (资料库歌单列表要用它分「我创建的 / 我收藏的」两组)
# ASCII only.

$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

$uid = "1935209761"
$a = @()
$a += $common
$a += "-G"
$a += @("-d", "uid=$uid", "-d", "limit=100", "-d", "offset=0")
$a += "http://music.163.com/api/user/playlist"
$r = & curl.exe @a
Write-Host ("raw head: " + $r.Substring(0, [Math]::Min(300, $r.Length)))
Write-Host ""
$j = $r | ConvertFrom-Json

Write-Host ("code={0}  playlist count={1}" -f $j.code, @($j.playlist).Count)
Write-Host ("one item's keys: " + ((@($j.playlist)[0].PSObject.Properties.Name) -join ", "))
Write-Host ""
Write-Host "name | subscribed | specialType | creator.userId | trackCount"
foreach ($p in @($j.playlist)) {
    $creator = ""
    if ($p.creator) { $creator = $p.creator.userId }
    Write-Host ("  {0} | {1} | {2} | {3} | {4}" -f $p.name, $p.subscribed, $p.specialType, $creator, $p.trackCount)
}
