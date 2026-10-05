# 探测「我的」页要用的用户信息接口
$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

function Probe($name, $url, $params) {
    Write-Host "=== $name ==="
    $a = @()
    $a += $common
    if ($params) { $a += "-G"; foreach ($p in $params) { $a += @("-d", $p) } }
    $a += $url
    $r = & curl.exe @a
    try { $j = $r | ConvertFrom-Json } catch { Write-Host "    非 JSON: $($r.Substring(0, [Math]::Min(120, $r.Length)))"; Write-Host ""; return }
    Write-Host ("    code={0}" -f $j.code)
    $p2 = $j.PSObject.Properties.Name
    Write-Host ("    顶层字段: " + (($p2 | Select-Object -First 12) -join ", "))
    if ($p2 -contains 'data') {
        $d = @($j.data)
        Write-Host ("    data 数量={0}" -f $d.Count)
        if ($d.Count -gt 0) { Write-Host ("    data[0] 字段: " + (($d[0].PSObject.Properties.Name | Select-Object -First 14) -join ", ")) }
    }
    Write-Host ("    原文前 240 字符: " + $r.Substring(0, [Math]::Min(240, $r.Length)))
    Write-Host ""
}

Probe "A-subcount"      "http://music.163.com/api/user/subcount" $null
Probe "B-artist-sublist" "http://music.163.com/api/artist/sublist" @("limit=10", "offset=0")
Probe "C-user-level"    "http://music.163.com/api/user/level" $null
