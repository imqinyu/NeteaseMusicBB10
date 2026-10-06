/*
 * NmItems - 网易云音乐的数据对象
 *
 * 设计要点（继承自 BBTieba 的经验）：
 *   - 所有交给 QML 的数据都通过 toVariantMap() 变成 QVariantMap；
 *   - ID 用字符串承载：网易的新版 ID（歌单/图片 picId 等）已经超过
 *     int32 甚至 2^53，QML 的 JS Number 装不下，字符串是唯一安全形式；
 *   - C++ 侧仍然用 qint64 存数值型 ID，只在 toVariantMap() 时转 QString。
 *
 * fee 字段的语义（来自官方客户端，用于灰掉无法播放的曲目）：
 *   0 = 免费或无版权判断
 *   1 = VIP 曲目（普通账号拿不到播放地址）
 *   4 = 需购买专辑
 *   8 = 非会员可听的免费曲目（低音质）
 */

#ifndef NM_ITEMS_HPP
#define NM_ITEMS_HPP

#include <QList>
#include <QString>
#include <QVariantMap>

namespace nm {

/*! 一首歌 */
struct NmSong
{
    qint64 id;
    QString name;
    QString artistsText;   // "周杰伦/温岚" 拼好的显示串
    QString albumName;
    qint64 albumId;
    qint64 duration;       // 毫秒
    int fee;               // 见文件头说明
    QString artUrl;        // 封面，可能为空（搜索结果里常常没有）
    qint64 mvId;           // 关联 MV 的 id（0 = 没有 MV）

    NmSong()
        : id(0)
        , albumId(0)
        , duration(0)
        , fee(0)
        , mvId(0)
    {
    }

    bool isVip() const { return fee == 1; }
    bool isPurchasedOnly() const { return fee == 4; }
    bool hasMv() const { return mvId > 0; }

    /*! "3:52" 形式的时长文本 */
    QString durationText() const;

    QVariantMap toVariantMap() const;
};

/*! 歌单信息 */
struct NmPlaylistInfo
{
    QString id;
    QString name;
    QString creatorName;
    QString coverUrl;
    QString description;
    int trackCount;
    int playCount;
    /*!
     * 发行 / 创建日期（毫秒时间戳）。
     * 专辑填 album.publishTime，歌单填 playlist.createTime；
     * 其它的（搜索结果 / 艺人页 / 每日推荐）留 0，表示不显示日期。
     */
    qint64 date;

    NmPlaylistInfo()
        : trackCount(0)
        , playCount(0)
        , date(0)
    {
    }

    QVariantMap toVariantMap() const;
};

/*! /song/enhance/player/url 的单条结果 */
struct NmSongUrl
{
    qint64 id;         // 请求的歌曲 id（回传对账用）
    QString url;       // 空 = 拿不到（未登录 / VIP 曲目 / 无版权）
    int code;          // 200=成功 -110=无权限 404=无版权
    int br;            // 实际码率
    int size;          // 字节
    QString type;      // mp3 / m4a ...
    int fee;

    NmSongUrl()
        : id(0)
        , code(0)
        , br(0)
        , size(0)
        , fee(0)
    {
    }

    bool hasUrl() const { return !url.isEmpty(); }
};

/*! 歌单摘要（/api/user/playlist 与 /api/personalized/playlist 共用） */
struct NmPlaylistSummary
{
    QString id;
    QString name;
    QString coverUrl;
    QString description;  // 推荐歌单里叫 copywriter，自建歌单里叫 description
    int trackCount;
    int playCount;
    int specialType;   // 5 = 「我喜欢的音乐」
    /*!
     * 是不是【收藏】来的（true = 收藏的，false = 自建的）。
     * 资料库的歌单列表按它分成「我创建的 / 我收藏的」两组。
     * ★ /api/user/playlist 返回的就是这个字段；推荐歌单接口没有，恒 false。
     */
    bool subscribed;

    NmPlaylistSummary()
        : trackCount(0)
        , playCount(0)
        , specialType(0)
        , subscribed(false)
    {
    }

    QVariantMap toVariantMap() const;
};

/*!
 * MV（/api/mv/first 最新 MV、/api/mv/all 全部 MV 的列表项）
 *
 * 播放地址要另外用 /api/song/enhance/play/mv/url 拿（带时效性签名）。
 */
struct NmMv
{
    qint64 id;
    QString name;
    QString coverUrl;
    QString artistName;
    qint64 artistId;
    qint64 duration;      // 毫秒（部分条目为 0）
    int playCount;

    NmMv()
        : id(0)
        , artistId(0)
        , duration(0)
        , playCount(0)
    {
    }

    QVariantMap toVariantMap() const;
};

/*! 关注的艺人（/api/artist/sublist 的列表项） */
struct NmArtist
{
    qint64 id;
    QString name;
    QString trans;        // 译名（可能为空）
    QString picUrl;
    int albumSize;
    int mvSize;

    NmArtist()
        : id(0)
        , albumSize(0)
        , mvSize(0)
    {
    }

    QVariantMap toVariantMap() const;
};

/*!
 * 一条评论（/api/v1/resource/comments/R_SO_4_<歌曲id>）
 *
 * timeStr 是服务端给好的时间文本（如 "2017年9月30日"），直接用，
 * 省得在 BB10 上自己格式化毫秒时间戳（Qt 4.8 的时区处理很麻烦）。
 */
struct NmComment
{
    qint64 id;
    /*! 评论者的用户 id（长按评论「打开用户资料」要用） */
    qint64 userId;
    QString userName;
    QString userAvatarUrl;
    QString content;
    QString timeText;
    int likedCount;
    bool isHot;

    NmComment()
        : id(0)
        , userId(0)
        , likedCount(0)
        , isHot(false)
    {
    }

    QVariantMap toVariantMap() const;
};

/*! 登录态检查（/nuser/account/get）的结果 */
struct NmAccount
{
    bool valid;        // profile 存在
    QString nickname;
    qint64 userId;
    int vipType;       // 0=非VIP
    QString avatarUrl;

    NmAccount()
        : valid(false)
        , userId(0)
        , vipType(0)
    {
    }
};

} // namespace nm

#endif // NM_ITEMS_HPP
