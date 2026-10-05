# 开发期 API 探测脚本：验证 /api/user/playlist 响应结构
$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"

$commonArgs = @(
    "-s", "--compressed",
    "-H", "Cookie: $cookie",
    "-H", "Referer: http://music.163.com/",
    "-H", "User-Agent: $ua"
)

# 1. 拿 uid
$acc = & curl.exe @commonArgs "http://music.163.com/api/nuser/account/get?csrfToken="
$accJson = $acc | ConvertFrom-Json
$uid = $accJson.profile.userId
Write-Host "uid = $uid  nickname = $($accJson.profile.nickname)"

# 2. 我的歌单列表
$pl = & curl.exe @commonArgs "http://music.163.com/api/user/playlist?uid=$uid&limit=100&offset=0"
$plJson = $pl | ConvertFrom-Json
Write-Host "code = $($plJson.code)  count = $($plJson.playlist.Count)"
Write-Host "--- 第一个歌单的完整字段 ---"
$plJson.playlist[0] | ConvertTo-Json -Depth 3 | Out-String -Width 400

# 3. 看第二个歌单里和列表展示相关的字段（确认都长一样）
Write-Host "--- 各歌单关键字段一览 ---"
foreach ($p in $plJson.playlist) {
    Write-Host ("id={0}  name={1}  trackCount={2}  specialType={3}  cover={4}  creator={5}" -f `
        $p.id, $p.name, $p.trackCount, $p.specialType, $p.coverImgUrl.Substring(0, 60), $p.creator.nickname)
}
