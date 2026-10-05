# 探测推荐页 / 评论相关接口是否还活着
# 注意：查询参数用 curl -G -d 传，避免在脚本里写 & （这个 PS 版本解析会报错）
$musicU = $env:NM_MUSIC_U
if (-not $musicU) { Write-Error "未设置 NM_MUSIC_U（开发期凭据已从仓库移除，跑脚本前请先设好这个环境变量）"; exit 1 }
$cookie = "MUSIC_U=$musicU; os=pc; appver=8.10.05; osver=Microsoft-Windows-10; channel=netease"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
$common = @("-s", "--compressed", "-H", "Cookie: $cookie", "-H", "Referer: http://music.163.com/", "-H", "User-Agent: $ua")

function Probe($name, $url, $params) {
    Write-Host "=== $name ==="
    $args2 = @()
    $args2 += $common
    if ($params) {
        $args2 += "-G"
        foreach ($p in $params) { $args2 += @("-d", $p) }
    }
    $args2 += $url
    $r = & curl.exe @args2

    try {
        $j = $r | ConvertFrom-Json
    } catch {
        $head = if ($r.Length -gt 150) { $r.Substring(0, 150) } else { $r }
        Write-Host "    非 JSON: $head"
        Write-Host ""
        return
    }

    Write-Host ("    code={0}" -f $j.code)
    $props = $j.PSObject.Properties.Name

    if ($props -contains 'result') {
        $arr = @($j.result)
        Write-Host ("    result 数量={0}" -f $arr.Count)
        if ($arr.Count -gt 0) {
            Write-Host ("    result[0] 字段: " + (($arr[0].PSObject.Properties.Name | Select-Object -First 14) -join ", "))
        }
    }
    if ($props -contains 'list')      { Write-Host ("    list 数量={0}" -f @($j.list).Count) }
    if ($props -contains 'data') {
        $arr = @($j.data)
        Write-Host ("    data 数量={0}" -f $arr.Count)
        if ($arr.Count -gt 0) {
            Write-Host ("    data[0] 字段: " + (($arr[0].PSObject.Properties.Name | Select-Object -First 14) -join ", "))
        }
    }
    if ($props -contains 'hotComments') {
        $hot = @($j.hotComments)
        $cms = @($j.comments)
        Write-Host ("    热评={0} 普通评论={1} total={2}" -f $hot.Count, $cms.Count, $j.total)
        if ($hot.Count -gt 0) {
            $c = $hot[0]
            $txt = ($c.content -replace "`r", " " -replace "`n", " ")
            Write-Host ("    示例: [{0}] {1} (赞 {2})" -f $c.user.nickname, $txt.Substring(0, [Math]::Min(40, $txt.Length)), $c.likedCount)
        }
    }
    if ($props -contains 'lrc')       { Write-Host ("    歌词长度={0}" -f $j.lrc.lyric.Length) }
    if ($props -contains 'recommend') { Write-Host ("    recommend 数量={0}" -f @($j.recommend).Count) }
    Write-Host ""
}

# 注意：调用行的标签用 ASCII —— 这个 PS 5.1 按 GBK 读无 BOM 的 UTF-8，
# 中文标签会让字节错位、吞掉后面的引号（"string is missing the terminator"）。
Probe "A-personalized-playlist" "http://music.163.com/api/personalized/playlist" @("limit=10", "offset=0", "total=true", "n=10")
Probe "B-personalized-newsong"  "http://music.163.com/api/personalized/newsong"  $null
Probe "C-recommend-songs"       "http://music.163.com/api/discovery/recommend/songs" @("total=true")
Probe "D-toplist"               "http://music.163.com/api/toplist" $null
Probe "E-comments"              "http://music.163.com/api/v1/resource/comments/R_SO_4_1446020184" @("limit=10", "offset=0")
Probe "F-lyric"                 "http://music.163.com/api/song/lyric" @("id=1446020184", "lv=1", "kv=1", "tv=-1")
