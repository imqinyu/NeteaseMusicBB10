# 只打印需要的字段（避免上次那种超长 JSON 刷屏）
$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

Write-Host "=== comments: hotComments[0] keys ==="
$r3 = & curl.exe @common -G -d "limit=3" -d "offset=0" "http://music.163.com/api/v1/resource/comments/R_SO_4_1446020184"
$j3 = $r3 | ConvertFrom-Json
Write-Host (($j3.hotComments[0].PSObject.Properties.Name) -join ", ")
Write-Host ("  user keys: " + (($j3.hotComments[0].user.PSObject.Properties.Name | Select-Object -First 10) -join ", "))
Write-Host ("  total={0} more={1}" -f $j3.total, $j3.more)
Write-Host ("  sample: content=[{0}] likedCount={1} time={2} avatar=[{3}]" -f `
    $j3.hotComments[0].content.Substring(0, 20), $j3.hotComments[0].likedCount, `
    $j3.hotComments[0].time, $j3.hotComments[0].user.avatarUrl)

Write-Host ""
Write-Host "=== newsong: result[0] keys ==="
$r2 = & curl.exe @common "http://music.163.com/api/personalized/newsong"
$j2 = $r2 | ConvertFrom-Json
Write-Host ("  result[0] keys: " + (($j2.result[0].PSObject.Properties.Name) -join ", "))
Write-Host ("  song keys: " + (($j2.result[0].song.PSObject.Properties.Name | Select-Object -First 12) -join ", "))
Write-Host ("  song.album.picUrl = [{0}]" -f $j2.result[0].song.album.picUrl)
Write-Host ("  song has dt? {0}" -f ($j2.result[0].song.PSObject.Properties.Name -contains 'dt'))

Write-Host ""
Write-Host "=== personalized/playlist: result[0] sample ==="
$r1 = & curl.exe @common -G -d "limit=3" -d "n=3" "http://music.163.com/api/personalized/playlist"
$j1 = $r1 | ConvertFrom-Json
Write-Host ("  name=[{0}] id={1} playCount={2} trackCount={3} copywriter=[{4}]" -f `
    $j1.result[0].name, $j1.result[0].id, $j1.result[0].playCount, `
    $j1.result[0].trackCount, $j1.result[0].copywriter)
Write-Host ("  picUrl=[{0}]" -f $j1.result[0].picUrl)
