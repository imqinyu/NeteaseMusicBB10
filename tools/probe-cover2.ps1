# 探测：搜索结果只有 picId 时，怎么拿到可下载的图片地址？
$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

# 取一首搜索结果（有 picId 没 picUrl）的 id 和 picId
$search = & curl.exe @common "http://music.163.com/api/search/get/web?csrfToken=&s=Chris&type=1&offset=0&total=true&limit=3"
$sj = $search | ConvertFrom-Json
$song = $sj.result.songs[0]
$songId = $song.id
$picId = $song.album.picId
$albumId = $song.album.id
Write-Host "songId=$songId  albumId=$albumId  picId=$picId  artistImg=$($song.artists[0].img1v1Url)"

# A. /api/song/detail 有没有 picUrl
Write-Host "--- A) /api/song/detail ---"
$sd = & curl.exe @common "http://music.163.com/api/song/detail?ids=%5B$songId%5D"
$sdj = $sd | ConvertFrom-Json
Write-Host ("  album.picUrl = [{0}]  picId={1}" -f $sdj.songs[0].album.picUrl, $sdj.songs[0].album.picId)

# B. /api/album/<id> 有没有 picUrl
Write-Host "--- B) /api/album/$albumId ---"
$al = & curl.exe @common "http://music.163.com/api/album/$albumId"
$alj = $al | ConvertFrom-Json
Write-Host ("  album.picUrl = [{0}]  blurPicUrl=[{1}]" -f $alj.album.picUrl, $alj.album.blurPicUrl)

# C. 直接用 picId 拼 CDN 地址能下吗（试几种常见拼法）
Write-Host "--- C) picId 直拼 CDN ---"
foreach ($host_ in @("p1", "p2", "p3", "p4")) {
    $u1 = "http://$host_.music.126.net/$picId.jpg"
    $r = & curl.exe -s -o NUL -w "%{http_code} %{size_download}" -H "User-Agent: $ua" -H "Referer: http://music.163.com/" $u1
    Write-Host "  $u1 -> $r"
}

# D. 官方模糊图接口（老客户端常用）
Write-Host "--- D) /api/img/blur ---"
$u2 = "http://music.163.com/api/img/blur/$picId"
$r2 = & curl.exe @common -o NUL -w "%{http_code} %{size_download} %{content_type}" $u2
Write-Host "  $u2 -> $r2"

# E. 歌单里的真实 picUrl 能不能下（验证下载链路本身）
Write-Host "--- E) 已知 picUrl 下载验证 ---"
$pl = & curl.exe @common "http://music.163.com/api/v2/playlist/detail?id=10119006626&n=2&s=0"
$pj = $pl | ConvertFrom-Json
$realUrl = $pj.playlist.tracks[0].al.picUrl
Write-Host "  url = $realUrl"
$r3 = & curl.exe -s -o NUL -w "%{http_code} %{size_download} %{content_type}" -H "User-Agent: $ua" -H "Accept: image/*,*/*;q=0.8" -H "Referer: http://music.163.com/" -H "Accept-Encoding: identity" $realUrl
Write-Host "  结果 = $r3"
