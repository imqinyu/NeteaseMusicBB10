/*
 * test_core - NeteaseMusic 逻辑层的本机验证（桌面 Qt 4.8 + mingw）
 *
 * 覆盖：
 *   - NmJson：真实响应的解析（搜索/歌单/播放地址/账号）
 *   - NmParsers：四个解析器的字段映射（夹具来自真实接口抓包）
 *   - NmItems：durationText / toVariantMap 的键名
 *   - NmSession：MUSIC_U 的多种粘贴形式
 *   - NmString：ids 数组参数的百分号编码
 *
 * 注意：断言中文时用 const char* 比较（QString 会走 Latin-1 隐式转换，
 * 这是 BBTieba 踩过的坑）。
 */

#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "model/NmItems.hpp"
#include "model/NmParsers.hpp"
#include "session/NmSession.hpp"
#include "util/NmJson.hpp"
#include "util/NmString.hpp"

static int g_failed = 0;
static int g_checked = 0;

static void check(bool ok, const char *what)
{
    ++g_checked;
    if (!ok) {
        ++g_failed;
        std::printf("FAIL: %s\n", what);
    }
}

/*! 字节级比较 QString 与 UTF-8 字面量（避免 Latin-1 隐式转换） */
static void checkStr(const QString &actual, const char *expectedUtf8, const char *what)
{
    ++g_checked;
    const QString expected = QString::fromUtf8(expectedUtf8);
    if (actual != expected) {
        ++g_failed;
        std::printf("FAIL: %s\n  actual:   %s\n  expected: %s\n",
                    what, actual.toUtf8().constData(), expectedUtf8);
    }
}

static QByteArray readFixture(const QString &name)
{
    const QString path = QString::fromAscii("fixtures/") + name;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::printf("cannot open fixture: %s\n", path.toUtf8().constData());
        std::exit(2);
    }
    return f.readAll();
}

static void testJsonBasics()
{
    const nm::NmJson bad = nm::NmJson::parse("{not json");
    check(!bad.isValid(), "invalid json detected");

    const nm::NmJson good = nm::NmJson::parse(
        "{\"code\":200,\"n\":-5,\"big\":109951165671182684,"
        "\"s\":\"\xe4\xb8\xad\xe6\x96\x87\",\"flag\":true,\"arr\":[1,2,3]}");
    check(good.isValid(), "valid json parsed");
    check(good.member("big").toLongLong() == 109951165671182684LL,
          "int64 preserved beyond 2^53");
    checkStr(good.member("s").toString(), "\xe4\xb8\xad\xe6\x96\x87", "utf8 string");
    check(good.member("arr").size() == 3, "array size");
    check(good.member("missing").isMissing(), "missing member is null");
    check(good.member("code").toString() == QString::fromAscii("200"),
          "number toString keeps literal");
}

static void testSearchParser()
{
    const nm::NmJson root = nm::NmJson::parse(readFixture("search.json"));
    check(root.isValid(), "search fixture parses");

    nm::NmParsers::SearchParse r = nm::NmParsers::parseSearch(root);
    check(r.ok, "search parse ok");
    check(r.songs.size() == 3, "search returns 3 songs");
    check(r.songCount > 3, "total songCount reported");

    if (r.songs.size() == 3) {
        const nm::NmSong &s0 = r.songs.at(0);
        checkStr(s0.name, "\xe5\xb1\x8b\xe9\xa1\xb6", "song name (屋顶)");
        check(s0.id == 5257138, "song id");
        check(s0.duration == 319039, "song duration ms");
        check(s0.durationText() == QString::fromAscii("5:19"), "duration text");
        checkStr(s0.artistsText,
                 "\xe5\x91\xa8\xe6\x9d\xb0\xe4\xbc\xa6/\xe6\xb8\xa9\xe5\xb2\x9a/\xe5\x90\xb4\xe5\xae\x97\xe5\xae\xaa",
                 "artists joined (周杰伦/温岚/吴宗宪)");
        checkStr(s0.albumName,
                 "\xe7\x94\xb7\xe5\xa5\xb3\xe6\x83\x85\xe6\xad\x8c\xe5\xaf\xb9\xe5\x94\xb1\xe5\x86\xa0\xe5\x86\x9b\xe5\x85\xa8\xe8\xae\xb0\xe5\xbd\x95",
                 "album name");
        check(s0.fee == 8, "fee");
        check(!s0.isVip(), "fee=8 is not vip");

        // QVariantMap 的键（QML 依赖这些名字）
        const QVariantMap m = s0.toVariantMap();
        check(m.value("id").toString() == QString::fromAscii("5257138"),
              "variant id is string");
        check(m.value("isVip").toBool() == false, "variant isVip");
        check(m.value("durationText").toString() == QString::fromAscii("5:19"),
              "variant durationText");
        check(m.contains("artistsText") && m.contains("albumName")
              && m.contains("artUrl"), "variant keys");
    }
}

static void testPlaylistParser()
{
    const nm::NmJson root = nm::NmJson::parse(readFixture("playlist.json"));
    check(root.isValid(), "playlist fixture parses");

    nm::NmParsers::PlaylistParse r = nm::NmParsers::parsePlaylist(root);
    check(r.ok, "playlist parse ok");
    checkStr(r.info.name, "\xe7\x83\xad\xe6\xad\x8c\xe6\xa6\x9c", "playlist name (热歌榜)");
    checkStr(r.info.creatorName, "\xe7\xbd\x91\xe6\x98\x93\xe4\xba\x91\xe9\x9f\xb3\xe4\xb9\x90",
             "creator nickname (网易云音乐)");
    check(r.info.trackCount == 200, "trackCount from server");
    check(r.songs.size() == 3, "3 tracks with n=3");

    if (r.songs.size() >= 1) {
        const nm::NmSong &s0 = r.songs.at(0);
        checkStr(s0.name, "\xe6\xb5\xb7\xe5\xb1\xbf\xe4\xbd\xa0", "new-format song name (海屿你)");
        check(s0.id == 1973665667, "new-format id");
        check(s0.duration == 295940, "new-format duration (dt)");
        check(s0.fee == 8, "new-format fee");
        check(!s0.artUrl.isEmpty(), "al.picUrl captured");
        checkStr(s0.artistsText, "\xe9\xa9\xac\xe4\xb9\x9f_Crabbit", "ar joined");
    }
}

static void testSongUrlParser()
{
    // 匿名请求：code 200 但 url=null, code=-110
    const nm::NmJson root = nm::NmJson::parse(readFixture("songurl_anon.json"));
    check(root.isValid(), "songurl fixture parses");

    nm::NmParsers::SongUrlParse r = nm::NmParsers::parseSongUrl(root);
    check(r.ok, "songurl parse ok");
    check(!r.url.hasUrl(), "anonymous has no url");
    check(r.url.code == -110, "code -110");
    check(r.url.id == 210049, "song id echoed");
}

static void testAccountParser()
{
    const nm::NmJson root = nm::NmJson::parse(readFixture("account_anon.json"));
    nm::NmParsers::AccountParse r = nm::NmParsers::parseAccount(root);
    check(r.ok, "anon account parse ok");
    check(!r.account.valid, "anonymous is not logged in");

    const nm::NmJson logged = nm::NmJson::parse(
        "{\"code\":200,\"profile\":{\"nickname\":\"testuser\",\"userId\":42,"
        "\"vipType\":0,\"avatarUrl\":\"http://x/a.jpg\"}}");
    nm::NmParsers::AccountParse r2 = nm::NmParsers::parseAccount(logged);
    check(r2.ok && r2.account.valid, "logged-in account valid");
    checkStr(r2.account.nickname, "testuser", "nickname");
    check(r2.account.userId == 42, "userId");

    const nm::NmJson err = nm::NmJson::parse("{\"code\":301,\"msg\":\"not login\"}");
    nm::NmParsers::AccountParse r3 = nm::NmParsers::parseAccount(err);
    check(!r3.ok, "301 treated as error");
}

static void testSessionCookieParsing()
{
    QCoreApplication::setOrganizationName("NeteaseMusicTests");
    QCoreApplication::setApplicationName("test_core");

    nm::NmSession session;

    // 纯值
    check(session.setCookieInput("abc123MUSICUVALUE"), "raw value accepted");
    check(session.musicU() == QString::fromAscii("abc123MUSICUVALUE"), "raw value stored");

    // 整段 cookie
    check(session.setCookieInput("__csrf=abc; MUSIC_U=xyz789; os=pc"),
          "cookie string accepted");
    check(session.musicU() == QString::fromAscii("xyz789"), "MUSIC_U extracted from cookie");

    // 请求参数形式
    check(session.setCookieInput("foo=1&MUSIC_U=paramform&bar=2"),
          "query form accepted");
    check(session.musicU() == QString::fromAscii("paramform"), "MUSIC_U extracted from query");

    // 带引号
    check(session.setCookieInput("MUSIC_U=\"quoted\""), "quoted value accepted");
    check(session.musicU() == QString::fromAscii("quoted"), "quotes stripped");

    // 没有 MUSIC_U
    check(!session.setCookieInput("NOSUCH=1; another=2"), "no MUSIC_U rejected");

    // cookie 头包含 MUSIC_U
    session.setCookieInput("MUSIC_U=secret");
    check(session.cookieHeader().contains(QString::fromAscii("MUSIC_U=secret")),
          "cookie header contains MUSIC_U");

    session.clear();
    check(!session.hasCookie(), "clear works");
    check(session.cookieHeader() == QString::fromAscii("os=pc"),
          "anonymous cookie header");
}

static void testIdsParamEncoding()
{
    // ids=[210049] 的编码（百分号、大小写）
    const QString encoded = nm::NmString::percentEncode(
        QString::fromAscii("[210049]"));
    check(encoded == QString::fromAscii("%5B210049%5D"), "ids param encoded");

    const QString multi = nm::NmString::percentEncode(
        QString::fromAscii("[1,2,3]"));
    check(multi == QString::fromAscii("%5B1%2C2%2C3%5D"), "multi ids encoded");
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testJsonBasics();
    testSearchParser();
    testPlaylistParser();
    testSongUrlParser();
    testAccountParser();
    testSessionCookieParsing();
    testIdsParamEncoding();

    std::printf("%d checks, %d failed\n", g_checked, g_failed);
    return g_failed == 0 ? 0 : 1;
}
