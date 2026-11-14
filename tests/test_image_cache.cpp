#include "util/NmImageCache.hpp"
#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QSignalSpy>

class ImageServer : public QTcpServer {
    Q_OBJECT
public:
    QByteArray body;
    int requests;
    ImageServer() : requests(0) {
        connect(this, SIGNAL(newConnection()), this, SLOT(acceptClient()));
        listen(QHostAddress::LocalHost);
    }
    QString url(const QString &name) const {
        return QString("http://127.0.0.1:%1/%2.jpg").arg(serverPort()).arg(name);
    }
private slots:
    void acceptClient() {
        while (hasPendingConnections()) {
            QTcpSocket *socket = nextPendingConnection();
            connect(socket, SIGNAL(readyRead()), this, SLOT(respond()));
            connect(socket, SIGNAL(disconnected()), socket, SLOT(deleteLater()));
        }
    }
    void respond() {
        QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
        if (socket->property("answered").toBool())
            return;
        const QByteArray request = socket->property("request").toByteArray() + socket->readAll();
        socket->setProperty("request", request);
        if (!request.contains("\r\n\r\n"))
            return;
        socket->setProperty("answered", true);
        ++requests;
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: "
                      + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
        socket->write(body);
        socket->disconnectFromHost();
    }
};

class ClearAndReload : public QObject {
    Q_OBJECT
public:
    nm::NmImageCache *cache;
    QString url;
    bool cleared;
    ClearAndReload(nm::NmImageCache *c, const QString &u) : cache(c), url(u), cleared(false) {
        connect(cache, SIGNAL(ready()), this, SLOT(ready()));
    }
private slots:
    void ready() {
        if (cleared)
            return;
        cleared = true;
        cache->clear();
        cache->pathFor(url);
    }
};

class ImageCacheTest : public QObject {
    Q_OBJECT
    QString root;
    static QByteArray jpeg(int size) {
        QImage image(size, size / 2, QImage::Format_RGB32);
        image.fill(qRgb(30, 80, 160));
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "JPG", 90);
        return bytes;
    }
    static bool waitReady(QSignalSpy &spy, int count) {
        for (int i = 0; i < 100 && spy.count() < count; ++i)
            QTest::qWait(50);
        return spy.count() >= count;
    }
private slots:
    void initTestCase() {
        root = QDir::currentPath() + "/nm-image-regression-" + QString::number(QCoreApplication::applicationPid());
        QVERIFY(QDir().mkpath(root));
    }
    void scaledImageAndDiskReuse() {
        ImageServer server;
        server.body = jpeg(4096);
        const QString dir = root + "/scaled";
        QString path;
        {
            nm::NmImageCache cache;
            cache.setCacheDir(dir);
            QSignalSpy spy(&cache, SIGNAL(ready()));
            QVERIFY(cache.pathFor(server.url("cover")).isEmpty());
            QVERIFY(waitReady(spy, 1));
            path = cache.pathFor(server.url("cover"));
            QImage image(QUrl(path).toLocalFile());
            QCOMPARE(image.size(), QSize(160, 80));
            QCOMPARE(server.requests, 1);
        }
        nm::NmImageCache reopened;
        reopened.setCacheDir(dir);
        QCOMPARE(reopened.pathFor(server.url("cover")), path);
        QCOMPARE(server.requests, 1);
    }
    void clearWhileBusyAndReload() {
        ImageServer server;
        server.body = jpeg(4096);
        nm::NmImageCache cache;
        const QString dir = root + "/clear";
        cache.setCacheDir(dir);
        QSignalSpy spy(&cache, SIGNAL(ready()));
        ClearAndReload reload(&cache, server.url("new"));
        for (int i = 0; i < 12; ++i)
            cache.pathFor(server.url(QString::number(i)));
        QVERIFY(waitReady(spy, 2));
        QTest::qWait(300);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(cache.cachedCount(), 1);
        QCOMPARE(cache.queuedCount(), 0);
        QCOMPARE(QDir(dir).entryList(QDir::Files).size(), 1);
        QVERIFY(!cache.pathFor(server.url("new")).isEmpty());
    }
    void badResponseIsNotPublished() {
        ImageServer server;
        server.body = "this is not an image";
        nm::NmImageCache cache;
        cache.setCacheDir(root + "/bad");
        QSignalSpy spy(&cache, SIGNAL(ready()));
        cache.pathFor(server.url("bad"));
        QTest::qWait(500);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(cache.cachedCount(), 0);
        QVERIFY(cache.pathFor(server.url("bad")).isEmpty());
        QTest::qWait(100);
        QCOMPARE(server.requests, 1);
        cache.clear();
        server.body = jpeg(320);
        cache.pathFor(server.url("bad"));
        QVERIFY(waitReady(spy, 1));
    }
    void emptyOldCacheFileIsReplaced() {
        ImageServer server;
        server.body = jpeg(320);
        nm::NmImageCache cache;
        const QString dir = root + "/empty";
        cache.setCacheDir(dir);
        QSignalSpy spy(&cache, SIGNAL(ready()));
        cache.pathFor(server.url("empty"));
        QVERIFY(waitReady(spy, 1));
        const QString path = QUrl(cache.pathFor(server.url("empty"))).toLocalFile();
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.close();
        nm::NmImageCache reopened;
        reopened.setCacheDir(dir);
        QSignalSpy again(&reopened, SIGNAL(ready()));
        reopened.pathFor(server.url("empty"));
        QVERIFY(waitReady(again, 1));
        QVERIFY(QFileInfo(path).size() > 0);
    }
    void oversizedResponseIsRejected() {
        ImageServer server;
        server.body = QByteArray(16 * 1024 * 1024 + 1, 'x');
        nm::NmImageCache cache;
        cache.setCacheDir(root + "/oversized");
        QSignalSpy spy(&cache, SIGNAL(ready()));
        cache.pathFor(server.url("large"));
        QTest::qWait(700);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(cache.cachedCount(), 0);
        QCOMPARE(cache.activeCount(), 0);
        QVERIFY(cache.pathFor(server.url("large")).isEmpty());
        QTest::qWait(100);
        QCOMPARE(server.requests, 1);
    }
    void cleanupTestCase() {
        QDir dir(root);
        foreach (const QString &child, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QDir sub(dir.filePath(child));
            foreach (const QString &file, sub.entryList(QDir::Files))
                QVERIFY(QFile::remove(sub.filePath(file)));
            QVERIFY(dir.rmdir(child));
        }
        QVERIFY(QDir().rmdir(root));
    }
};
QTEST_MAIN(ImageCacheTest)
#include "test_image_cache.moc"
