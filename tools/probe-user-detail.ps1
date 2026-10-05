# Dump /api/v1/user/detail/<uid> fields (what can we show on another user's page?)
# ASCII only.

$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

$other = "446664973"
Write-Host ("=== v1/user/detail/" + $other + " ===")
$a = @()
$a += $common
$a += ("http://music.163.com/api/v1/user/detail/" + $other)
$r = & curl.exe @a
$j = $r | ConvertFrom-Json
Write-Host ("code={0}  level={1}  listenSongs={2}" -f $j.code, $j.level, $j.listenSongs)
Write-Host ("top-level: " + (($j.PSObject.Properties.Name) -join ", "))
Write-Host ""
Write-Host "profile fields:"
foreach ($p in $j.profile.PSObject.Properties) {
    $v = $p.Value
    if ($v -is [PSCustomObject]) { Write-Host ("  {0} = <object>" -f $p.Name); continue }
    if ($v -is [Array]) { Write-Host ("  {0} = <array({1})>" -f $p.Name, $v.Count); continue }
    $s = [string]$v
    if ($s.Length -gt 70) { $s = $s.Substring(0, 70) + "..." }
    Write-Host ("  {0} = {1}" -f $p.Name, $s)
}
Write-Host ""
Write-Host "identify:"
if ($j.identify) {
    foreach ($p in $j.identify.PSObject.Properties) {
        $s = [string]$p.Value
        if ($s.Length -gt 80) { $s = $s.Substring(0, 80) + "..." }
        Write-Host ("  {0} = {1}" -f $p.Name, $s)
    }
}
