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
#include <bb/cascades/Container>
#include <bb/cascades/SceneCover>
#include <bb/cascades/AbstractCover>
#include <bb/cascades/MultiCover>
#include <bb/cascades/LocaleHandler>

#include <bb/multimedia/MediaPlayer>

#include <QDeclarativeError>
#include <QNetworkProxyFactory>
#include <QTranslator>
#include <cstdio>

using namespace bb::cascades;

ApplicationUI::ApplicationUI() :
        QObject(),
        m_pTranslator(0),
        m_pLocaleHandler(0),
        m_pMusic(0),
        m_pPlayer(0)
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

    // 多任务视图的封面（Active Frame）：单独一个 QML，用它自己的
    // QmlDocument 加载（里面的 music / player 是上面注册的 context property）
    QmlDocument *qmlCover = QmlDocument::create("asset:///AppCover.qml").parent(this);
    if (qmlCover) {
        if (qmlCover->hasErrors()) {
            foreach (const QDeclarativeError &e, qmlCover->errors())
                fprintf(stderr, "[NeteaseMusic] AppCover.qml error: %s\n", qPrintable(e.toString()));

        } else {
            qmlCover->setContextProperty("music", m_pMusic);
            qmlCover->setContextProperty("player", m_pPlayer);
            /*
             * ★ 封面现在是 MultiCover（AppCover.qml 里按 CoverDetailLevel
             *   给不同分辨率机型提供 High / Medium 两档版式）。
             *   MultiCover 本身就是 AbstractCover，直接交给 setCover 即可，
             *   不能再按 Container 取根对象再包一层 SceneCover ——
             *   那样取到的是 null，多任务视图会没有封面。
             */
            AbstractCover *cover = qmlCover->createRootObject<AbstractCover>();
            if (cover)
                Application::instance()->setCover(cover);
        }
    }

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
