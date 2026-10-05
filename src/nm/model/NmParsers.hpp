/*
 * NmParsers - 网易接口 JSON -> 数据对象
 *
 * 网易的同一语义字段在不同接口下有两套名字（新旧两版客户端并存）：
 *
 *   老版（/api/search/get/web、/api/song/detail）：
 *       artists / album / duration / lMusic / mMusic / hMusic
 *   新版（/api/v2/playlist/detail、cloudsearch）：
 *       ar / al / dt / l / m / h
 *
 * 判别方法与 MeeGo 版 cloudmusicqt 相同：节点里有 "dt" 就按新版解析。
 */

#ifndef NM_PARSERS_HPP
#define NM_PARSERS_HPP

#include "model/NmItems.hpp"
#include "util/NmJson.hpp"

#include <QList>
#include <QString>

namespace nm {

class NmParsers
{
public:
    /*! 搜索单曲结果 */
    struct SearchParse
    {
        bool ok;
        QString error;      // 服务端 msg 或解析错误说明
        int code;           // 服务端 code
        QList<NmSong> songs;
        int songCount;      // 结果总数（用于翻页判断）

        SearchParse()
            : ok(false)
            , code(0)
            , songCount(0)
        {
        }
    };

    /*! 歌单详情结果 */
    struct PlaylistParse
    {
        bool ok;
        QString error;
        int code;
        NmPlaylistInfo info;
        QList<NmSong> songs;

        PlaylistParse()
            : ok(false)
            , code(0)
        {
        }
    };

    /*! 播放地址结果 */
    struct SongUrlParse
    {
        bool ok;            // HTTP + code==200 的解析层面成功
        QString error;
        NmSongUrl url;

        SongUrlParse()
            : ok(false)
        {
        }
    };

    /*! /api/user/level 的结果（等级 / 听歌数） */
    struct LevelParse
    {
        bool ok;
        QString error;
        int code;
        int level;
        int playCount;
        int loginCount;

        LevelParse()
            : ok(false)
            , code(0)
            , level(0)
            , playCount(0)
            , loginCount(0)
        {
        }
    };

    /*!
     * /api/v1/user/detail/&lt;uid&gt; 的结果（看【别人】的资料页用）。
     *
     * ★ 为什么是这个接口（2026-10 实测，见 tools/probe-user-*.ps1）：
     *   - /api/user/detail?uid=   → 404（只有路径式活着）
     *   - /api/artist/sublist     → 【不认 uid】，永远返回登录用户自己的关注歌手，
     *                               所以"别人的关注歌手"拿不到（换 userId 参数、
     *                               换成 /v1/... 路径都是 404 或返回我的）
     */
    struct UserDetailParse
    {
        bool ok;
        QString error;
        int code;
        QString nickName;
        QString avatarUrl;
        QString signature;
        int level;
        int listenSongs;
        int follows;
        int followeds;
        /*! 对方是"音乐人"时才有（profile.artistId / artistName） */
        qint64 artistId;
        QString artistName;

        UserDetailParse()
            : ok(false)
            , code(0)
            , level(0)
            , listenSongs(0)
            , follows(0)
            , followeds(0)
            , artistId(0)
        {
        }
    };

    /*! /api/artist/sublist（关注的艺人）的结果 */
    struct ArtistsParse
    {
        bool ok;
        QString error;
        int code;
        QList<NmArtist> artists;

        ArtistsParse()
            : ok(false)
            , code(0)
        {
        }
    };

    /*! /api/mv/first、/api/mv/all（MV 列表）的结果 */
    struct MvsParse
    {
        bool ok;
        QString error;
        int code;
        QList<NmMv> mvs;

        MvsParse()
            : ok(false)
            , code(0)
        {
        }
    };

    /*! /api/song/enhance/play/mv/url 的结果 */
    struct MvUrlParse
    {
        bool ok;
        QString error;
        int code;
        qint64 id;
        QString url;

        MvUrlParse()
            : ok(false)
            , code(0)
            , id(0)
        {
        }
    };

    /*! /api/v1/cloud/get（音乐云盘）的结果 */
    struct CloudParse
    {
        bool ok;
        QString error;
        int code;
        QList<NmSong> songs;
        int count;

        CloudParse()
            : ok(false)
            , code(0)
            , count(0)
        {
        }
    };

    /*! /api/personalized/playlist（推荐歌单）与 /api/discovery/recommend/songs 的结果 */
    struct DiscoverParse
    {
        bool ok;
        QString error;
        int code;
        QList<NmPlaylistSummary> playlists;
        QList<NmSong> songs;          // 每日推荐歌曲（recommend 数组）

        DiscoverParse()
            : ok(false)
            , code(0)
        {
        }
    };

    /*!
     * /api/song/lyric 的结果。
     * lyric 是原词 LRC 原文，trans 是翻译（有的歌没有，就是空串）。
     */
    struct LyricParse
    {
        bool ok;
        QString error;
        int code;
        QString lyric;
        QString trans;

        LyricParse()
            : ok(false)
            , code(0)
        {
        }
    };

    /*! /api/v1/resource/comments/R_SO_4_<id>（歌曲评论）的结果 */
    struct CommentsParse
    {
        bool ok;
        QString error;
        int code;
        int total;
        QList<NmComment> hotComments;
        QList<NmComment> comments;

        CommentsParse()
            : ok(false)
            , code(0)
            , total(0)
        {
        }
    };

    /*! /api/song/detail 的结果（搜索结果缺封面时用它的 album.picUrl 补齐） */
    struct SongDetailParse
    {
        bool ok;
        QString error;
        int code;
        QList<NmSong> songs;

        SongDetailParse()
            : ok(false)
            , code(0)
        {
        }
    };

    /*! /api/user/playlist（我的歌单列表）的结果 */
    struct UserPlaylistsParse
    {
        bool ok;
        QString error;
        int code;
        QList<NmPlaylistSummary> playlists;

        UserPlaylistsParse()
            : ok(false)
            , code(0)
        {
        }
    };

    /*! 登录态结果 */
    struct AccountParse
    {
        bool ok;
        QString error;
        NmAccount account;

        AccountParse()
            : ok(false)
        {
        }
    };

    /*! /api/search/get/web 的响应 */
    static SearchParse parseSearch(const NmJson &root);

    /*! /api/v2/playlist/detail 的响应 */
    static PlaylistParse parsePlaylist(const NmJson &root);

    /*! /api/v1/album/{id} 的响应（复用 PlaylistParse：info=专辑，songs=曲目） */
    static PlaylistParse parseAlbum(const NmJson &root);

    /*! /api/song/enhance/player/url 的响应 */
    static SongUrlParse parseSongUrl(const NmJson &root);

    /*! /api/nuser/account/get 的响应 */
    static AccountParse parseAccount(const NmJson &root);

    /*! /api/user/playlist 的响应（2026-10 实测：playlist 数组 +
     *  id/name/trackCount/specialType/coverImgUrl/playCount） */
    static UserPlaylistsParse parseUserPlaylists(const NmJson &root);

    /*! /api/song/detail 的响应（只有它有搜索结果缺失的 album.picUrl） */
    static SongDetailParse parseSongDetail(const NmJson &root);

    /*! /api/personalized/playlist 的响应（推荐歌单：picUrl + copywriter） */
    static DiscoverParse parseDiscover(const NmJson &root);

    /*! /api/discovery/recommend/songs 的响应（每日推荐歌曲：recommend 数组） */
    static DiscoverParse parseRecommend(const NmJson &root);

    /*! /api/v1/resource/comments/... 的响应（hotComments + comments） */
    static CommentsParse parseComments(const NmJson &root);

    /*! /api/song/lyric 的响应（lrc.lyric / tlyric.lyric） */
    static LyricParse parseLyric(const NmJson &root);

    /*! /api/user/level 的响应 */
    static LevelParse parseLevel(const NmJson &root);

    /*! /api/v1/user/detail/<uid> 的响应（看别人的资料页） */
    static UserDetailParse parseUserDetail(const NmJson &root);

    /*! /api/artist/sublist 的响应（data 数组） */
    static ArtistsParse parseArtists(const NmJson &root);

    /*! /api/mv/first、/api/mv/all 的响应（data 数组） */
    static MvsParse parseMvs(const NmJson &root);

    /*! /api/song/enhance/play/mv/url 的响应（data.url，带时效性签名） */
    static MvUrlParse parseMvUrl(const NmJson &root);

    /*! /api/v1/cloud/get 的响应（data[] 里是 simpleSong 包裹的曲目） */
    static CloudParse parseCloud(const NmJson &root);

private:
    /*! 新旧两种歌曲节点的统一解析 */
    static NmSong parseSong(const NmJson &node);

    /*! "a/b/c" 拼歌手名 */
    static QString joinArtists(const QList<NmJson> &artists);
    static QString joinArtistsNew(const QList<NmJson> &artists);
    /*! 评论节点 -> NmComment（hotComments 与 comments 结构相同） */
    static NmComment parseComment(const NmJson &node, bool isHot);
};

} // namespace nm

#endif // NM_PARSERS_HPP
