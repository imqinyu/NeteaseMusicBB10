# 开发期探测：搜索结果里到底有没有封面地址？能不能下载？
$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

# 1. 搜索接口：album 节点全貌
$search = & curl.exe @common "http://music.163.com/api/search/get/web?csrfToken=&s=Chris&type=1&offset=0&total=true&limit=3"
$sj = $search | ConvertFrom-Json
Write-Host "=== search/get/web: 第一首歌的 album 节点 ==="
$sj.result.songs[0].album | ConvertTo-Json -Depth 3 | Out-String -Width 300

Write-Host "=== 前三首的 picUrl ==="
foreach ($s in ($sj.result.songs | Select-Object -First 3)) {
    Write-Host ("  {0}  picUrl=[{1}]  picId={2}" -f $s.name, $s.album.picUrl, $s.album.picId)
}

# 2. 歌单详情：al 节点（新版）
$pl = & curl.exe @common "http://music.163.com/api/v2/playlist/detail?id=10119006626&n=3&s=0"
$pj = $pl | ConvertFrom-Json
Write-Host "=== v2/playlist/detail: 第一首的 al 节点 ==="
$pj.playlist.tracks[0].al | ConvertTo-Json -Depth 3 | Out-String -Width 300

# 3. 封面能不能下？（模拟 App 的请求头：无 cookie / Accept-Encoding identity）
$firstUrl = $sj.result.songs[0].album.picUrl
Write-Host "=== 下载测试: $firstUrl ==="
$code = & curl.exe -s -o NUL -w "%{http_code} %{size_download} %{content_type}" `
    -H "User-Agent: $ua" -H "Accept: image/*,*/*;q=0.8" -H "Referer: http://music.163.com/" `
    -H "Accept-Encoding: identity" $firstUrl
Write-Host "  结果: $code"
