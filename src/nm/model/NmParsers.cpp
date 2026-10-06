#include "model/NmParsers.hpp"

#include <QLatin1String>
#include <QStringList>
#include <QVariantMap>

#include "util/NmTr.hpp"
#include "util/NmString.hpp"

namespace nm {

namespace {

/*!
 * 网易的错误响应：{"code":xxx,"msg":"..."} 或 {"code":xxx,"message":"..."}。
 * 拿出来统一成一个 (code, text)。
 */
void extractError(const NmJson &root, int *code, QString *text)
{
    if (code)
        *code = (int)root.member(QLatin1String("code")).toLongLong(-1);
    if (text) {
        QString msg = root.member(QLatin1String("msg")).toString();
        if (msg.isEmpty())
            msg = root.member(QLatin1String("message")).toString();
        *text = msg;
    }
}

} // anonymous namespace

QString NmParsers::joinArtists(const QList<NmJson> &artists)
{
    QStringList names;
    foreach (const NmJson &a, artists)
        names.append(a.member(QLatin1String("name")).toString());
    return names.join(QLatin1String("/"));
}

QString NmParsers::joinArtistsNew(const QList<NmJson> &artists)
{
    QStringList names;
    foreach (const NmJson &a, artists)
        names.append(a.member(QLatin1String("name")).toString());
    return names.join(QLatin1String("/"));
}

NmSong NmParsers::parseSong(const NmJson &node)
{
    NmSong song;
    if (!node.isObject())
        return song;

    song.id = node.member(QLatin1String("id")).toLongLong();
    song.name = node.member(QLatin1String("name")).toString();
    song.fee = node.member(QLatin1String("fee")).toInt();
    // 关联 MV（0 = 没有）。新旧两种歌曲格式里都在顶层。
    song.mvId = node.member(QLatin1String("mv")).toLongLong();

    if (node.contains(QLatin1String("dt"))) {
        // 新版（playlist / cloudsearch）
        song.duration = node.member(QLatin1String("dt")).toLongLong();
        song.artistsText = joinArtistsNew(
            node.member(QLatin1String("ar")).items());
        const NmJson &al = node.member(QLatin1String("al"));
        song.albumName = al.member(QLatin1String("name")).toString();
        song.albumId = al.member(QLatin1String("id")).toLongLong();
        song.artUrl = al.member(QLatin1String("picUrl")).toString();
    } else {
        // 老版（search / song detail）
        song.duration = node.member(QLatin1String("duration")).toLongLong();
        song.artistsText = joinArtists(
            node.member(QLatin1String("artists")).items());
        const NmJson &album = node.member(QLatin1String("album"));
        song.albumName = album.member(QLatin1String("name")).toString();
        song.albumId = album.member(QLatin1String("id")).toLongLong();
        song.artUrl = album.member(QLatin1String("picUrl")).toString();
    }

    return song;
}

NmParsers::SearchParse NmParsers::parseSearch(const NmJson &root)
{
    SearchParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    const NmJson &result = root.member(QLatin1String("result"));
    if (!result.isObject()) {
        out.error = NmTr("E5938DE5BA94E7BCBAE5B191resultE5AD97E6AEB5");
        return out;
    }

    out.songCount = (int)result.member(QLatin1String("songCount")).toLongLong();

    // /api/search/get/web 用 "songs"；个别版本返回 "songs" 为空但 songCount>0
    foreach (const NmJson &item, result.member(QLatin1String("songs")).items()) {
        NmSong song = parseSong(item);
        if (song.id > 0)
            out.songs.append(song);
    }

    out.ok = true;
    return out;
}

NmParsers::PlaylistParse NmParsers::parsePlaylist(const NmJson &root)
{
    PlaylistParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    const NmJson &pl = root.member(QLatin1String("playlist"));
    if (!pl.isObject()) {
        out.error = NmTr("E5938DE5BA94E7BCBAE5B191playlistE5AD97E6AEB5");
        return out;
    }

    out.info.id = pl.member(QLatin1String("id")).toString();
    out.info.name = pl.member(QLatin1String("name")).toString();
    out.info.coverUrl = pl.member(QLatin1String("coverImgUrl")).toString();
    out.info.description = pl.member(QLatin1String("description")).toString();
    out.info.trackCount = (int)pl.member(QLatin1String("trackCount")).toLongLong();
    out.info.playCount = (int)pl.member(QLatin1String("playCount")).toLongLong();
    // 歌单：创建日期
    out.info.date = pl.member(QLatin1String("createTime")).toLongLong();
    out.info.creatorName =
        pl.member(QLatin1String("creator")).member(QLatin1String("nickname")).toString();

    foreach (const NmJson &item, pl.member(QLatin1String("tracks")).items()) {
        NmSong song = parseSong(item);
        if (song.id > 0)
            out.songs.append(song);
    }

    // v2 接口在 n 参数限制下可能截断，真实数目以 trackCount 为准
    if (out.info.trackCount < out.songs.size())
        out.info.trackCount = out.songs.size();

    out.ok = true;
    return out;
}

NmParsers::PlaylistParse NmParsers::parseAlbum(const NmJson &root)
{
    PlaylistParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    /*
     * /api/v1/album/{id} 的响应形如 { "album": {...}, "songs": [...] }；
     * 有的版本把 songs 塞在 album 里，这里两处都试。
     */
    const NmJson &album = root.member(QLatin1String("album"));
    out.info.id = album.member(QLatin1String("id")).toString();
    out.info.name = album.member(QLatin1String("name")).toString();
    out.info.coverUrl = album.member(QLatin1String("picUrl")).toString();
    // 专辑：发行日期
    out.info.date = album.member(QLatin1String("publishTime")).toLongLong();
    out.info.creatorName =
        album.member(QLatin1String("artist")).member(QLatin1String("name")).toString();

    NmJson songs = root.member(QLatin1String("songs"));
    if (songs.items().isEmpty())
        songs = album.member(QLatin1String("songs"));

    foreach (const NmJson &item, songs.items()) {
        NmSong song = parseSong(item);
        if (song.id > 0)
            out.songs.append(song);
    }

    out.info.trackCount = out.songs.size();
    out.ok = true;
    return out;
}

NmParsers::SongUrlParse NmParsers::parseSongUrl(const NmJson &root)
{
    SongUrlParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    const NmJson &data = root.member(QLatin1String("data"));
    if (!data.isArray() || data.size() == 0) {
        out.error = NmTr("E5938DE5BA94E7BCBAE5B191dataE5AD97E6AEB5");
        return out;
    }

    const NmJson &item = data.at(0);
    out.url.id = item.member(QLatin1String("id")).toLongLong();
    out.url.url = item.member(QLatin1String("url")).toString();
    out.url.code = (int)item.member(QLatin1String("code")).toLongLong();
    out.url.br = (int)item.member(QLatin1String("br")).toLongLong();
    out.url.size = (int)item.member(QLatin1String("size")).toLongLong();
    out.url.type = item.member(QLatin1String("type")).toString();
    out.url.fee = (int)item.member(QLatin1String("fee")).toInt();

    out.ok = true;
    return out;
}

NmParsers::AccountParse NmParsers::parseAccount(const NmJson &root)
{
    AccountParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    if (code != 200) {
        // 301/40x = 未登录 / cookie 失效
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    const NmJson &profile = root.member(QLatin1String("profile"));
    if (!profile.isObject()) {
        // code==200 但无 profile：匿名态
        out.ok = true;
        return out;
    }

    out.account.valid = true;
    out.account.nickname = profile.member(QLatin1String("nickname")).toString();
    out.account.userId = profile.member(QLatin1String("userId")).toLongLong();
    out.account.vipType = (int)profile.member(QLatin1String("vipType")).toLongLong();
    out.account.avatarUrl = NmString::cleanAvatarUrl(profile.member(QLatin1String("avatarUrl")).toString());

    out.ok = true;
    return out;
}

NmParsers::SongDetailParse NmParsers::parseSongDetail(const NmJson &root)
{
    SongDetailParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    foreach (const NmJson &item, root.member(QLatin1String("songs")).items()) {
        NmSong song = parseSong(item);
        if (song.id > 0)
            out.songs.append(song);
    }

    out.ok = true;
    return out;
}

NmParsers::MvsParse NmParsers::parseMvs(const NmJson &root)
{
    MvsParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    // 实测（2026-10）：data[] = id / name / cover / playCount /
    //                          artistName / artistId / duration
    foreach (const NmJson &item, root.member(QLatin1String("data")).items()) {
        if (!item.isObject())
            continue;
        NmMv mv;
        mv.id = item.member(QLatin1String("id")).toLongLong();
        mv.name = item.member(QLatin1String("name")).toString();
        mv.coverUrl = item.member(QLatin1String("cover")).toString();
        mv.artistName = item.member(QLatin1String("artistName")).toString();
        mv.artistId = item.member(QLatin1String("artistId")).toLongLong();
        mv.duration = item.member(QLatin1String("duration")).toLongLong();
        mv.playCount = (int)item.member(QLatin1String("playCount")).toLongLong();
        if (mv.id > 0)
            out.mvs.append(mv);
    }

    out.ok = true;
    return out;
}

NmParsers::MvUrlParse NmParsers::parseMvUrl(const NmJson &root)
{
    MvUrlParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    const NmJson &data = root.member(QLatin1String("data"));
    out.id = data.member(QLatin1String("id")).toLongLong();
    out.url = data.member(QLatin1String("url")).toString();
    // data.code 才是业务码（如 -125 = 无版权），url 会是 null
    const int bizCode = (int)data.member(QLatin1String("code")).toLongLong(200);
    if (bizCode != 200 || out.url.isEmpty()) {
        out.error = NmTr("E8AFA54D56E697A0E58FAFE794A8E692ADE694BEE59CB0E59D80"); // 该MV无可用播放地址
        return out;
    }

    out.ok = true;
    return out;
}

NmParsers::CloudParse NmParsers::parseCloud(const NmJson &root)
{
    CloudParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    out.count = (int)root.member(QLatin1String("count")).toLongLong();

    /*
     * 云盘的每条是 { simpleSong: {...}, songId, fileName, ... }。
     * simpleSong 里是新版字段（dt / al / ar），正好走 parseSong。
     * 拿不到就退回用 songId / fileName。
     */
    foreach (const NmJson &item, root.member(QLatin1String("data")).items()) {
        if (!item.isObject())
            continue;

        const NmJson simple = item.member(QLatin1String("simpleSong"));
        const NmJson &node = simple.isObject() ? simple : item;

        NmSong song = parseSong(node);
        if (song.id <= 0)
            song.id = item.member(QLatin1String("songId")).toLongLong();
        if (song.name.isEmpty())
            song.name = item.member(QLatin1String("fileName")).toString();

        if (song.id > 0)
            out.songs.append(song);
    }

    out.ok = true;
    return out;
}

NmParsers::LevelParse NmParsers::parseLevel(const NmJson &root)
{
    LevelParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    // 实测：data 是对象（不是数组）—— {userId, level, nowPlayCount, ...}
    const NmJson &data = root.member(QLatin1String("data"));
    out.level = (int)data.member(QLatin1String("level")).toLongLong();
    out.playCount = (int)data.member(QLatin1String("nowPlayCount")).toLongLong();
    out.loginCount = (int)data.member(QLatin1String("nowLoginCount")).toLongLong();

    out.ok = true;
    return out;
}

NmParsers::UserDetailParse NmParsers::parseUserDetail(const NmJson &root)
{
    UserDetailParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    // 实测：等级 / 听歌数在【顶层】，个人资料都在 profile 里
    out.level = (int)root.member(QLatin1String("level")).toLongLong();
    out.listenSongs = (int)root.member(QLatin1String("listenSongs")).toLongLong();

    const NmJson &profile = root.member(QLatin1String("profile"));
    out.nickName = profile.member(QLatin1String("nickname")).toString();
    out.avatarUrl = NmString::cleanAvatarUrl(profile.member(QLatin1String("avatarUrl")).toString());
    out.signature = profile.member(QLatin1String("signature")).toString();
    out.follows = (int)profile.member(QLatin1String("follows")).toLongLong();
    out.followeds = (int)profile.member(QLatin1String("followeds")).toLongLong();
    // 对方是音乐人时才有（普通用户是 0 / 空）
    out.artistId = profile.member(QLatin1String("artistId")).toLongLong();
    out.artistName = profile.member(QLatin1String("artistName")).toString();

    out.ok = true;
    return out;
}

NmParsers::ArtistsParse NmParsers::parseArtists(const NmJson &root)
{
    ArtistsParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    foreach (const NmJson &item, root.member(QLatin1String("data")).items()) {
        if (!item.isObject())
            continue;
        NmArtist artist;
        artist.id = item.member(QLatin1String("id")).toLongLong();
        artist.name = item.member(QLatin1String("name")).toString();
        artist.trans = item.member(QLatin1String("trans")).toString();
        artist.picUrl = item.member(QLatin1String("picUrl")).toString();
        artist.albumSize = (int)item.member(QLatin1String("albumSize")).toLongLong();
        artist.mvSize = (int)item.member(QLatin1String("mvSize")).toLongLong();
        if (artist.id > 0)
            out.artists.append(artist);
    }

    out.ok = true;
    return out;
}

NmParsers::DiscoverParse NmParsers::parseDiscover(const NmJson &root)
{
    DiscoverParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    // 实测（2026-10）：result[] = id / name / picUrl / playCount /
    // trackCount / copywriter（注意这里是 picUrl，不是 coverImgUrl）
    foreach (const NmJson &item, root.member(QLatin1String("result")).items()) {
        if (!item.isObject())
            continue;
        NmPlaylistSummary pl;
        pl.id = item.member(QLatin1String("id")).toString();
        pl.name = item.member(QLatin1String("name")).toString();
        pl.coverUrl = item.member(QLatin1String("picUrl")).toString();
        pl.description = item.member(QLatin1String("copywriter")).toString();
        pl.trackCount = (int)item.member(QLatin1String("trackCount")).toLongLong();
        pl.playCount = (int)item.member(QLatin1String("playCount")).toLongLong();
        if (!pl.id.isEmpty())
            out.playlists.append(pl);
    }

    out.ok = true;
    return out;
}

NmParsers::DiscoverParse NmParsers::parseRecommend(const NmJson &root)
{
    DiscoverParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    // 每日推荐：recommend[]，老格式（duration / artists / album.picUrl）
    foreach (const NmJson &item, root.member(QLatin1String("recommend")).items()) {
        NmSong song = parseSong(item);
        if (song.id > 0)
            out.songs.append(song);
    }

    out.ok = true;
    return out;
}

NmComment NmParsers::parseComment(const NmJson &node, bool isHot)
{
    NmComment comment;
    if (!node.isObject())
        return comment;

    comment.id = node.member(QLatin1String("commentId")).toLongLong();
    comment.content = node.member(QLatin1String("content")).toString();
    // timeStr 是服务端给好的文本（"2017年9月30日"），省得自己格式化
    comment.timeText = node.member(QLatin1String("timeStr")).toString();
    comment.likedCount = (int)node.member(QLatin1String("likedCount")).toLongLong();
    comment.isHot = isHot;

    const NmJson &user = node.member(QLatin1String("user"));
    comment.userId = user.member(QLatin1String("userId")).toLongLong();
    comment.userName = user.member(QLatin1String("nickname")).toString();
    comment.userAvatarUrl = NmString::cleanAvatarUrl(user.member(QLatin1String("avatarUrl")).toString());

    return comment;
}

NmParsers::LyricParse NmParsers::parseLyric(const NmJson &root)
{
    LyricParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    out.code = (int)root.member(QLatin1String("code")).toLongLong(200);

    /*
     * 原词：lrc.lyric
     * 翻译：tlyric.lyric（很多歌没有这个字段，取不到就是空串）
     * 两者都是 LRC 原文，形如 "[00:12.34]歌词\n[00:15.67]…"
     */
    out.lyric = root.member(QLatin1String("lrc"))
                    .member(QLatin1String("lyric")).toString();
    out.trans = root.member(QLatin1String("tlyric"))
                    .member(QLatin1String("lyric")).toString();

    out.ok = true;
    return out;
}

NmParsers::QrKeyParse NmParsers::parseQrKey(const NmJson &root)
{
    QrKeyParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    out.code = (int)root.member(QLatin1String("code")).toLongLong(200);

    /*
     * 响应形如 {"code":200,"unikey":"9f8a..."}
     * 拿到 key 后拼成登录 URL：http://music.163.com/login?codekey=<unikey>
     */
    out.unikey = root.member(QLatin1String("unikey")).toString();

    out.ok = true;
    return out;
}

NmParsers::QrStatusParse NmParsers::parseQrStatus(const NmJson &root)
{
    QrStatusParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    /*
     * 响应里就 code 有用：800 过期 / 801 待扫码 / 802 待确认 / 803 成功。
     * ★ 803 这次响应会带 Set-Cookie（MUSIC_U）—— 但那是在
     *   NmHttpClient 里统一收的，不归这里管。
     */
    out.code = (int)root.member(QLatin1String("code")).toLongLong(0);

    out.ok = true;
    return out;
}

NmParsers::CommentsParse NmParsers::parseComments(const NmJson &root)
{
    CommentsParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    out.total = (int)root.member(QLatin1String("total")).toLongLong();

    foreach (const NmJson &item, root.member(QLatin1String("hotComments")).items()) {
        NmComment comment = parseComment(item, true);
        if (comment.id > 0)
            out.hotComments.append(comment);
    }

    foreach (const NmJson &item, root.member(QLatin1String("comments")).items()) {
        NmComment comment = parseComment(item, false);
        if (comment.id > 0)
            out.comments.append(comment);
    }

    out.ok = true;
    return out;
}

NmParsers::UserPlaylistsParse NmParsers::parseUserPlaylists(const NmJson &root)
{
    UserPlaylistsParse out;

    if (!root.isObject()) {
        out.error = NmTr("E5938DE5BA94E4B88DE698AFE59088E6B395JSON");
        return out;
    }

    const qint64 code = root.member(QLatin1String("code")).toLongLong(200);
    out.code = (int)code;
    if (code != 200) {
        extractError(root, 0, &out.error);
        if (out.error.isEmpty())
            out.error = NmTr("E69C8DE58AA1E599A8E99499E8AFAF");
        return out;
    }

    // 实测（2026-10）：more=true 表示还有下一页（配合 offset 翻页）
    foreach (const NmJson &item, root.member(QLatin1String("playlist")).items()) {
        if (!item.isObject())
            continue;
        NmPlaylistSummary pl;
        pl.id = item.member(QLatin1String("id")).toString();
        pl.name = item.member(QLatin1String("name")).toString();
        pl.coverUrl = item.member(QLatin1String("coverImgUrl")).toString();
        pl.trackCount = (int)item.member(QLatin1String("trackCount")).toLongLong();
        pl.playCount = (int)item.member(QLatin1String("playCount")).toLongLong();
        pl.specialType = (int)item.member(QLatin1String("specialType")).toLongLong();
        // 收藏来的还是自建的（资料库歌单列表按它分组）
        pl.subscribed = item.member(QLatin1String("subscribed")).toLongLong() != 0;
        if (!pl.id.isEmpty() && !pl.name.isEmpty())
            out.playlists.append(pl);
    }

    out.ok = true;
    return out;
}

} // namespace nm
