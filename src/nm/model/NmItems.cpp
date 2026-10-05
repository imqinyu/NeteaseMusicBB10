#include "model/NmItems.hpp"

#include <QLatin1String>

namespace nm {

QString NmSong::durationText() const
{
    if (duration <= 0)
        return QString(QLatin1String("0:00"));

    const int total = (int)(duration / 1000);
    const int m = total / 60;
    const int s = total % 60;
    return QString(QLatin1String("%1:%2"))
        .arg(m)
        .arg(s, 2, 10, QLatin1Char('0'));
}

QVariantMap NmSong::toVariantMap() const
{
    QVariantMap map;
    map.insert(QLatin1String("id"), QString::number(id));
    map.insert(QLatin1String("name"), name);
    map.insert(QLatin1String("artistsText"), artistsText);
    map.insert(QLatin1String("albumName"), albumName);
    map.insert(QLatin1String("albumId"), QString::number(albumId));
    map.insert(QLatin1String("duration"), (double)duration);
    map.insert(QLatin1String("durationText"), durationText());
    map.insert(QLatin1String("fee"), fee);
    map.insert(QLatin1String("isVip"), isVip());
    map.insert(QLatin1String("artUrl"), artUrl);
    // MV：QML 里做提示（hasMv）与"播放 MV"（mvId 传回 C++）
    map.insert(QLatin1String("mvId"), QString::number(mvId));
    map.insert(QLatin1String("hasMv"), hasMv());
    return map;
}

QVariantMap NmPlaylistInfo::toVariantMap() const
{
    QVariantMap map;
    map.insert(QLatin1String("id"), id);
    map.insert(QLatin1String("name"), name);
    map.insert(QLatin1String("creatorName"), creatorName);
    map.insert(QLatin1String("coverUrl"), coverUrl);
    map.insert(QLatin1String("description"), description);
    map.insert(QLatin1String("trackCount"), trackCount);
    map.insert(QLatin1String("playCount"), playCount);
    return map;
}

QVariantMap NmMv::toVariantMap() const
{
    QVariantMap map;
    map.insert(QLatin1String("id"), QString::number(id));
    map.insert(QLatin1String("name"), name);
    map.insert(QLatin1String("coverUrl"), coverUrl);
    map.insert(QLatin1String("artistName"), artistName);
    map.insert(QLatin1String("artistId"), QString::number(artistId));
    map.insert(QLatin1String("duration"), (double)duration);
    map.insert(QLatin1String("playCount"), playCount);
    return map;
}

QVariantMap NmArtist::toVariantMap() const
{
    QVariantMap map;
    map.insert(QLatin1String("id"), QString::number(id));
    map.insert(QLatin1String("name"), name);
    map.insert(QLatin1String("trans"), trans);
    map.insert(QLatin1String("picUrl"), picUrl);
    map.insert(QLatin1String("albumSize"), albumSize);
    map.insert(QLatin1String("mvSize"), mvSize);
    return map;
}

QVariantMap NmComment::toVariantMap() const
{
    QVariantMap map;
    map.insert(QLatin1String("id"), QString::number(id));
    map.insert(QLatin1String("userId"), QString::number(userId));
    map.insert(QLatin1String("userName"), userName);
    map.insert(QLatin1String("userAvatarUrl"), userAvatarUrl);
    map.insert(QLatin1String("content"), content);
    map.insert(QLatin1String("timeText"), timeText);
    map.insert(QLatin1String("likedCount"), likedCount);
    map.insert(QLatin1String("isHot"), isHot);
    return map;
}

QVariantMap NmPlaylistSummary::toVariantMap() const
{
    QVariantMap map;
    map.insert(QLatin1String("id"), id);
    map.insert(QLatin1String("name"), name);
    map.insert(QLatin1String("coverUrl"), coverUrl);
    map.insert(QLatin1String("description"), description);
    map.insert(QLatin1String("trackCount"), trackCount);
    map.insert(QLatin1String("playCount"), playCount);
    map.insert(QLatin1String("specialType"), specialType);
    map.insert(QLatin1String("subscribed"), subscribed);
    return map;
}

} // namespace nm
