/*
 * Copyright (c) 2011-2015 BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "applicationui.hpp"

#include "MusicController.hpp"

#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/SceneCover>
#include <bb/cascades/MultiCover>
#include <bb/cascades/Container>
#include <bb/cascades/LayoutUpdateHandler>
#include <bb/cascades/LocaleHandler>

#include <bb/multimedia/MediaPlayer>

#include <QDeclarativeError>
#include <QNetworkProxyFactory>
#include <QTranslator>
#include <QTimer>
#include <cstdio>

using namespace bb::cascades;

ApplicationUI::ApplicationUI() :
        QObject(),
        m_pTranslator(0),
        m_pLocaleHandler(0),
        m_pMusic(0),
        m_pPlayer(0),
        m_coverHigh(0),
        m_coverMedium(0),
        m_coverPrimeTimer(new QTimer(this)),
        m_coverInThumbnail(false)
{
    // 使用系统代理设置（企业网络/运营商环境需要）
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // prepare the localization
    m_pTranslator = new QTranslator(this);
    m_pLocaleHandler = new LocaleHandler(this);

    bool res = QObject::connect(m_pLocaleHandler, SIGNAL(systemLanguageChanged()), this, SLOT(onSystemLanguageChanged()));
    // This is only available in Debug builds
    Q_ASSERT(res);
    // Since the variable is not used in the app, this is added to avoid a
    // compiler warning
    Q_UNUSED(res);

    // initial load
    onSystemLanguageChanged();

    registerQmlTypes();

    // Create scene document from main.qml asset, the parent is set
    // to ensure the document gets destroyed properly at shut down.
    QmlDocument *qml = QmlDocument::create("asset:///main.qml").parent(this);

    // 逻辑层入口：QML 里用 music.xxx 调用
    m_pMusic = new MusicController(this);
    qml->setContextProperty("music", m_pMusic);

    // ★ 先应用用户保存的主题，再建 UI：避免启动时先亮后暗地闪一下
    m_pMusic->applySavedTheme();

    /*
     * ★ 播放器也做成 context property。
     *
     * 为什么不放 QML 的 attachedObjects 里：那样只有主页面能看见它，
     * 独立组件文件（播放条、MV 页…）要用的话就得把对象当属性传进去，
     * 而 Cascades 对象塞进 property variant / 自定义属性会踩
     * "Unable to set property"（播放器按钮直接失效）。
     * 做成 context property 后任何页面都能直接用 player.play()。
     */
    m_pPlayer = new bb::multimedia::MediaPlayer(this);
    qml->setContextProperty("player", m_pPlayer);

    // 把播放器挂给逻辑层：时间条拖动 seek 要用（seekTime 不是 Q_INVOKABLE）
    m_pMusic->attachPlayer(m_pPlayer);

    // 一首播完自动切下一首（QML 里没法给 context property 写信号处理器）
    connect(m_pPlayer, SIGNAL(playbackCompleted()),
            m_pMusic, SLOT(playbackCompleted()));

    /*
     * ★ 生命周期：进后台（锁屏 / 切到别的应用）/ 回前台。
     *
     *   为什么需要：缺 run_when_backgrounded 时，应用一进后台就被 Navigator
     *   【挂起】—— 声音是 mm-renderer（系统里独立的音频服务）在放，所以锁屏
     *   后还听得见；但"这首播完了"这个事件（playbackCompleted，走 BPS 投递）
     *   是发给【我们进程】的，进程被挂起就收不到 —— 表现就是
     *   「锁屏后一首放完停在那，不切下一首」。
     *
     *   权限已经补在 bar-descriptor.xml 里了；这里挂上生命周期事件是为了：
     *     · thumbnail  只记日志：日志断在这里 = 进程被挂起了，一眼可判；
     *     · fullscreen 兜底：万一播完事件还是漏了，回到前台补切一次
     *       （判定很严，见 MusicController::onAppForegrounded，不会乱跳歌）。
     *
     *   ★ thumbnail 里绝不能停播放 —— 音乐 app 就是要在后台继续放。
     */
    bool okThumb = connect(Application::instance(), SIGNAL(thumbnail()),
                           m_pMusic, SLOT(onAppThumbnailed()));
    bool okFull = connect(Application::instance(), SIGNAL(fullscreen()),
                          m_pMusic, SLOT(onAppForegrounded()));
    Q_ASSERT(okThumb);
    Q_ASSERT(okFull);
    Q_UNUSED(okThumb);
    Q_UNUSED(okFull);

    m_coverPrimeTimer->setSingleShot(true);
    m_coverPrimeTimer->setInterval(50);
    connect(m_coverPrimeTimer, SIGNAL(timeout()), this, SLOT(advanceCoverPrime()));
    connect(Application::instance(), SIGNAL(thumbnail()), this, SLOT(onCoverThumbnailed()));
    connect(Application::instance(), SIGNAL(fullscreen()), this, SLOT(onCoverHidden()));
    connect(Application::instance(), SIGNAL(invisible()), this, SLOT(onCoverHidden()));

    // Create root object for the UI
    AbstractPane *root = qml->createRootObject<AbstractPane>();

    // ★ QML 加载失败是黑屏的头号原因，而 Cascades 的 QML 错误默认
    //   只进 slog2，gdb/终端里完全看不到。这里主动打到 stderr，
    //   调试会话里就能直接看到出错的 QML 行号。
    if (qml->hasErrors() || !root) {
        fprintf(stderr, "[NeteaseMusic] main.qml load FAILED:\n");
        foreach (const QDeclarativeError &e, qml->errors()) {
            fprintf(stderr, "[NeteaseMusic]   %s\n",
                    qPrintable(e.toString()));
        }
        fflush(stderr);
    }

    // Set created root object as the application scene
    Application::instance()->setScene(root);

    // 读取本地登录态。放在 setScene 之后，这样 QML 已经绑定好属性，
    // 能立刻反映登录状态。
    m_pMusic->init();
    createAppCover();
}

SceneCover* ApplicationUI::createCoverScene(bool compact)
{
    SceneCover *cover = new SceneCover();
    cover->setObjectName(compact ? "medium" : "high");
    // Documents stay alive for the whole application lifetime, as do these covers.
    QmlDocument *document = QmlDocument::create("asset:///AppCover.qml").parent(this);
    if (!document) {
        delete cover;
        return 0;
    }
    document->setContextProperty("music", m_pMusic);
    document->setContextProperty("appCover", cover);
    Container *content = document->createRootObject<Container>();
    if (document->hasErrors() || !content) {
        fprintf(stderr, "[COVER] AppCover.qml load FAILED level=%s\n", compact ? "medium" : "high");
        foreach (const QDeclarativeError &e, document->errors())
            fprintf(stderr, "[COVER]   %s\n", qPrintable(e.toString()));
        fflush(stderr);
        delete content;
        delete document;
        delete cover;
        return 0;
    }
    cover->setContent(content);
    LayoutUpdateHandler *layout = content->findChild<LayoutUpdateHandler*>("coverRootLayout");
    if (layout) {
        layout->setProperty("coverLevel", cover->objectName());
        const bool connected = connect(layout, SIGNAL(layoutFrameChanged(QRectF)),
                                       this, SLOT(onCoverLayoutChanged(QRectF)));
        Q_ASSERT(connected);
        Q_UNUSED(connected);
    } else {
        fprintf(stderr, "[COVER] missing coverRootLayout level=%s\n", qPrintable(cover->objectName()));
    }
    connect(cover, SIGNAL(sizeChanged(QSize)), this, SLOT(onCoverSizeChanged(QSize)));
    return cover;
}

void ApplicationUI::createAppCover()
{
    // Follow the SDK Music/Video asset variants: only Passport (1440x1440)
    // provides Medium. On Z10/10.3.1 the unsupported level gets a 334x0 canvas.
    // Keep the scene graphs alive so cached images can finish loading in foreground.
    m_coverHigh = createCoverScene(false);
    if (!m_coverHigh)
        return;
    const bool supportsMedium = m_coverHigh->content()->property("supportsMediumCover").toBool();
    if (supportsMedium)
        m_coverMedium = createCoverScene(true);
    if (supportsMedium && !m_coverMedium) {
        delete m_coverHigh;
        delete m_coverMedium;
        m_coverHigh = m_coverMedium = 0;
        return;
    }
    MultiCover *cover = new MultiCover();
    m_coverHigh->setParent(cover);
    cover->add(m_coverHigh, CoverDetailLevel::High);
    if (m_coverMedium) {
        m_coverMedium->setParent(cover);
        cover->add(m_coverMedium, CoverDetailLevel::Medium);
    }
    fprintf(stderr, "[COVER] build=passport-layout-v5 installed=high%s\n", supportsMedium ? "+medium" : "");
    Application::instance()->setCover(cover);
    logAppCoverSizes();
}

void ApplicationUI::onCoverSizeChanged(const QSize &size)
{
    SceneCover *cover = qobject_cast<SceneCover*>(sender());
    fprintf(stderr, "[COVER] sizeChanged level=%s size=%dx%d\n",
            cover ? qPrintable(cover->objectName()) : "unknown", size.width(), size.height());
    fflush(stderr);
}

void ApplicationUI::onCoverLayoutChanged(const QRectF &frame)
{
    if (frame.width() <= 0 || frame.height() <= 0)
        return;
    const QString level = sender()->property("coverLevel").toString();
    SceneCover *cover = level == QLatin1String("high") ? m_coverHigh : m_coverMedium;
    if (!cover)
        return;
    cover->setProperty("layoutReady", true);
    scheduleCoverPrime();
}

void ApplicationUI::onCoverThumbnailed()
{
    m_coverInThumbnail = true;
    logAppCoverSizes();
    scheduleCoverPrime();
}

void ApplicationUI::scheduleCoverPrime()
{
    if (!m_coverInThumbnail)
        return;
    SceneCover *covers[] = { m_coverHigh, m_coverMedium };
    bool pending = false;
    for (int i = 0; i < 2; ++i) {
        SceneCover *cover = covers[i];
        if (!cover || !cover->property("layoutReady").toBool() || cover->property("primed").toBool())
            continue;
        cover->setProperty("primePending", true);
        pending = true;
    }
    // Let the initial layout/render event finish. Never reset an active timer:
    // subsequent layout events must not postpone restoring a hidden cover.
    if (pending && !m_coverPrimeTimer->isActive())
        m_coverPrimeTimer->start();
}

void ApplicationUI::advanceCoverPrime()
{
    if (!m_coverInThumbnail) {
        onCoverHidden();
        return;
    }
    SceneCover *covers[] = { m_coverHigh, m_coverMedium };
    bool restorePending = false;
    for (int i = 0; i < 2; ++i) {
        SceneCover *cover = covers[i];
        if (!cover || !cover->property("primePending").toBool())
            continue;
        Container *content = qobject_cast<Container*>(cover->content());
        const bool layoutPulse = content && content->property("supportsMediumCover").toBool();
        if (!cover->property("priming").toBool()) {
            cover->setProperty("priming", true);
            // Passport needs a content/layout update after its first cover frame.
            // Keep both roots visible: hiding both did not refresh MultiCover there.
            // One physical pixel changes the child geometry without changing track data.
            if (layoutPulse)
                content->setLeftPadding(1.0f);
            else
                cover->content()->setVisible(false);
            restorePending = true;
            fprintf(stderr, "[COVER] prime %s level=%s\n", layoutPulse ? "inset" : "hide", qPrintable(cover->objectName()));
        } else {
            if (layoutPulse)
                content->resetLeftPadding();
            else
                cover->content()->setVisible(true);
            cover->setProperty("priming", false);
            cover->setProperty("primePending", false);
            cover->setProperty("primed", true);
            fprintf(stderr, "[COVER] prime %s level=%s\n", layoutPulse ? "layout restored" : "show", qPrintable(cover->objectName()));
        }
    }
    fflush(stderr);
    // Separate hide/show into different event-loop turns: an unchanged NOTIFY or
    // invisible text character does not create a real visual change in SceneCover.
    if (restorePending)
        m_coverPrimeTimer->start();
}

void ApplicationUI::onCoverHidden()
{
    m_coverInThumbnail = false;
    m_coverPrimeTimer->stop();
    SceneCover *covers[] = { m_coverHigh, m_coverMedium };
    for (int i = 0; i < 2; ++i) {
        SceneCover *cover = covers[i];
        if (!cover)
            continue;
        if (cover->property("priming").toBool()) {
            Container *content = qobject_cast<Container*>(cover->content());
            if (content && content->property("supportsMediumCover").toBool())
                content->resetLeftPadding();
            cover->content()->setVisible(true);
            fprintf(stderr, "[COVER] prime cancelled/restored level=%s\n", qPrintable(cover->objectName()));
        }
        cover->setProperty("priming", false);
        cover->setProperty("primePending", false);
    }
    fflush(stderr);
}

void ApplicationUI::logAppCoverSizes()
{
    SceneCover *covers[] = { m_coverHigh, m_coverMedium };
    for (int i = 0; i < 2; ++i) {
        SceneCover *cover = covers[i];
        if (!cover)
            continue;
        const QSize size = cover->size();
        fprintf(stderr, "[COVER] thumbnail/setup level=%s size=%dx%d art=%d lyrics=%d\n",
                qPrintable(cover->objectName()), size.width(), size.height(),
                m_pMusic->coverHasArt() ? 1 : 0, m_pMusic->property("lyricsEnabled").toBool() ? 1 : 0);
    }
    fflush(stderr);
}

void ApplicationUI::registerQmlTypes()
{
    // 数据全部以 QVariantMap / bb::cascades::ArrayDataModel 的形式交给 QML，
    // 不需要注册自定义类型。以后需要注册枚举/QML 组件时放在这里：
    //     qmlRegisterUncreatableType<...>("com.netease.music", 1, 0, "Name", reason);
}

void ApplicationUI::onSystemLanguageChanged()
{
    QCoreApplication::instance()->removeTranslator(m_pTranslator);
    // Initiate, load and install the application translation files.
    QString locale_string = QLocale().name();
    QString file_name = QString("NeteaseMusic_%1").arg(locale_string);
    if (m_pTranslator->load(file_name, "app/native/qm")) {
        QCoreApplication::instance()->installTranslator(m_pTranslator);
    }
}
