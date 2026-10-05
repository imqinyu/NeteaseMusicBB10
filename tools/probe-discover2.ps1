# 看每日推荐 / 推荐新歌里歌曲节点的具体结构（决定用哪套字段名解析）
$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

Write-Host "=== daily recommend: recommend[0] ==="
$r = & curl.exe @common -G -d "total=true" "http://music.163.com/api/discovery/recommend/songs"
$j = $r | ConvertFrom-Json
($j.recommend[0].PSObject.Properties.Name) -join ", "
Write-Host "--- recommend[0] 完整 ---"
$j.recommend[0] | ConvertTo-Json -Depth 4 | Out-String -Width 400

Write-Host "=== newsong: result[0].song ==="
$r2 = & curl.exe @common "http://music.163.com/api/personalized/newsong"
$j2 = $r2 | ConvertFrom-Json
Write-Host ("  result[0] 字段: " + (($j2.result[0].PSObject.Properties.Name) -join ", "))
$j2.result[0].song | ConvertTo-Json -Depth 3 | Out-String -Width 400

Write-Host "=== comments: hotComments[0] 字段 ==="
$r3 = & curl.exe @common -G -d "limit=3" -d "offset=0" "http://music.163.com/api/v1/resource/comments/R_SO_4_1446020184"
$j3 = $r3 | ConvertFrom-Json
Write-Host ("  hotComments[0] 字段: " + (($j3.hotComments[0].PSObject.Properties.Name) -join ", "))
Write-Host ("  user 字段: " + (($j3.hotComments[0].user.PSObject.Properties.Name | Select-Object -First 8) -join ", "))
Write-Host ("  total = " + $j3.total + "  more = " + $j3.more)
