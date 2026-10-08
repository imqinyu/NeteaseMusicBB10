// SettingsPage - 设置页（由应用菜单的 settingsAction push 进来）
//
// 结构照 NMDemoUI/assets/SettingsPage.qml 的设计：主题 / 音质 / 缓存，
// 外加项目原有的「调试」。
//
// 参照 ModPlayer 的做法：设置是「推入的一个页面」而不是弹窗，
// 返回交给 NavigationPane 的原生返回键/左滑。

import bb.cascades 1.4

Page {
    id: settingsPage
    objectName: "settingsPage"

    titleBar: TitleBar {
        title: qsTr("设置")
    }

    function refresh() {
        cacheInfoLabel.text = music.cacheInfo()
    }

    onCreationCompleted: {
        refresh()
    }

    ScrollView {
        Container {
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }

            // ================= 主题 =================
            Header {
                title: qsTr("主题")
            }
            Container {
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }
                topPadding: 5.0
                bottomPadding: 5.0
                rightPadding: 15.0
                leftPadding: 15.0
                Label {
                    text: qsTr("应用暗色主题")
                    verticalAlignment: VerticalAlignment.Center
                    layoutProperties: StackLayoutProperties {
                        spaceQuota: 1
                    }
                    textStyle {
                        base: SystemDefaults.TextStyles.BodyText
                    }
                }
                ToggleButton {
                    id: themeToggle
                    verticalAlignment: VerticalAlignment.Center
                    // 初始化期间置位，避免"只是打开设置页"就触发一次 setTheme
                    property bool ready: false
                    /*
                     * 初始状态读【当前实际主题】（而不是 music.theme()），
                     * 这样"跟随系统"时也能得到正确的开关位置。
                     * 不用绑定：控件交互会和绑定打架（值被弹回）。
                     */
                    onCreationCompleted: {
                        checked = (Application.themeSupport.theme.colorTheme.style
                                   == VisualStyle.Dark)
                        ready = true
                    }
                    onCheckedChanged: {
                        if (!ready)
                            return
                        music.setTheme(checked ? "Dark" : "Bright")
                    }
                }
            }
            

            // ================= 内容 =================
            Header {
                title: qsTr("内容")
            }
            Container {
                topPadding: 5.0
                bottomPadding: 5.0
                rightPadding: 15.0
                leftPadding: 15.0
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }
                Label {
                    text: qsTr("隐藏 VIP 歌曲")
                    verticalAlignment: VerticalAlignment.Center
                    layoutProperties: StackLayoutProperties {
                        spaceQuota: 1
                    }
                    textStyle {
                        base: SystemDefaults.TextStyles.BodyText
                    }
                }
                ToggleButton {
                    id: hideVipToggle
                    verticalAlignment: VerticalAlignment.Center
                    // 同主题开关：不用绑定（交互会和绑定打架），创建时读一次
                    property bool ready: false
                    onCreationCompleted: {
                        checked = music.hideVip
                        ready = true
                    }
                    onCheckedChanged: {
                        if (!ready)
                            return
                        music.setHideVip(checked)
                    }
                }
            }
            // ---- 歌词开关（打开后主界面迷你条显示歌词）----
            Container {
                topPadding: 5.0
                bottomPadding: 5.0
                rightPadding: 15.0
                leftPadding: 15.0
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }
                Label {
                    text: qsTr("主界面显示歌词")
                    verticalAlignment: VerticalAlignment.Center
                    layoutProperties: StackLayoutProperties {
                        spaceQuota: 1
                    }
                    textStyle {
                        base: SystemDefaults.TextStyles.BodyText
                    }
                }
                ToggleButton {
                    id: lyricsToggle
                    verticalAlignment: VerticalAlignment.Center
                    property bool ready: false
                    // ★ 同上：跟随后端（别的入口改了开关时也能同步过来）
                    property bool lyricsOn: music.lyricsEnabled
                    onLyricsOnChanged: {
                        if (checked != lyricsOn)
                            checked = lyricsOn
                    }
                    onCreationCompleted: {
                        checked = music.lyricsEnabled
                        ready = true
                    }
                    onCheckedChanged: {
                        if (!ready)
                            return
                        /*
                         * ★ 走【属性赋值】而不是调 setLyricsEnabled()：
                         *   Q_PROPERTY 的 WRITE 一定在元对象里，比 Q_INVOKABLE
                         *   方法更稳（后者在某些构建/部署下会"莫名其妙"取不到）。
                         *   赋值同样会触发 setter，持久化到 QSettings 照旧。
                         */
                        music.lyricsEnabled = checked
                    }
                }
            }
            // ---- 歌词翻译开关（显示时副标题变成这句的翻译）----
            Container {
                topPadding: 5.0
                bottomPadding: 5.0
                rightPadding: 15.0
                leftPadding: 15.0
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }
                Label {
                    text: qsTr("显示歌词翻译")
                    verticalAlignment: VerticalAlignment.Center
                    layoutProperties: StackLayoutProperties {
                        spaceQuota: 1
                    }
                    textStyle {
                        base: SystemDefaults.TextStyles.BodyText
                    }
                }
                ToggleButton {
                    id: lyricsTransToggle
                    verticalAlignment: VerticalAlignment.Center
                    /*
                     * ★ 歌词显示没打开时，翻译开关整块禁用（置灰）。
                     *   C++ 那边（setLyricsEnabled）在关歌词时已经先把翻译
                     *   关掉了，所以这里是"先关掉、再禁用"，不会出现
                     *   "虽然灰着但还亮着"的别扭状态。
                     */
                    enabled: music.lyricsEnabled
                    property bool ready: false
                    /*
                     * ★ 跟随后端：C++ 把翻译关掉时（例如用户刚关掉歌词开关），
                     *   这个开关必须【立刻】熄灭。
                     *   不能直接把 checked 绑到 music.lyricsTransEnabled ——
                     *   用户拨动开关时"交互 vs 绑定"会打架（本文件其它开关也
                     *   都是"创建时读一次"的写法）。这里用"中间属性 +
                     *   onXxxChanged 手动同步一次"，取值相同就不赋值，
                     *   不会来回震荡。
                     *   （之前只在 onCreationCompleted 读一次，所以必须退出
                     *   设置页再重进才看得到变化 —— 真机反馈的问题。）
                     */
                    property bool transOn: music.lyricsTransEnabled
                    onTransOnChanged: {
                        if (checked != transOn)
                            checked = transOn
                    }
                    onCreationCompleted: {
                        checked = music.lyricsTransEnabled
                        ready = true
                    }
                    onCheckedChanged: {
                        if (!ready)
                            return
                        // ★ 同上：属性赋值，稳过调方法
                        music.lyricsTransEnabled = checked
                    }
                }
            }

            // ================= 调试 =================
            //开关不起作用
//            Header {
//                title: qsTr("调试")
//            }
//            Container {
//                topPadding: 5.0
//                bottomPadding: 5.0
//                rightPadding: 15.0
//                leftPadding: 15.0
//                layout: StackLayout {
//                    orientation: LayoutOrientation.LeftToRight
//                }
//                Label {
//                    text: qsTr("输出控制台日志")
//                    verticalAlignment: VerticalAlignment.Center
//                    layoutProperties: StackLayoutProperties {
//                        spaceQuota: 1
//                    }
//                    textStyle {
//                        base: SystemDefaults.TextStyles.BodyText
//                    }
//                }
//                ToggleButton {
//                    checked: music.consoleLogEnabled()
//                    onCheckedChanged: {
//                        music.setConsoleLogEnabled(checked)
//                    }
//                }
//            }

            // ================= 音质 =================
            Header {
                title: qsTr("音质")
            }
            RadioGroup {
                
                Option {
                    text: qsTr("标准 128k")
                    value: 128000
                    selected: music.bitrate == 128000
                }
                Option {
                    text: qsTr("较高 192k")
                    value: 192000
                    selected: music.bitrate == 192000
                }
                Option {
                    text: qsTr("极高 320k")
                    value: 320000
                    selected: music.bitrate == 320000
                }
                onSelectedValueChanged: {
                    music.setBitrate(selectedValue)
                }
            }




           

            // ================= 缓存 =================
            Header {
                title: qsTr("缓存")
            }
            DropDown {
                options: [
                    Option {
                        text: "512MB "
                    
                    },
                    Option {
                        text: "1G"

                    },
                    Option {
                        text: "2G"

                    },
                    Option {
                        text: "3G"

                    },
                    Option {
                        text: "5G"

                    }
                ]
                title: "音乐缓存上限"
                /*
                 * ★ 这里按【下标】映射上限（MB），因为 Option 没写 value：
                 *     0→512  1→1024  2→2048  3→3072  4→5120
                 *   进页面时按当前设置选中对应项；改动立刻落盘并按新上限清理。
                 */
                onCreationCompleted: {
                    var limits = [512, 1024, 2048, 3072, 5120]
                    var cur = music.audioCacheLimit()
                    for (var i = 0; i < limits.length; i ++) {
                        if (limits[i] == cur) {
                            selectedIndex = i
                            break
                        }
                    }
                }
                onSelectedIndexChanged: {
                    var limits = [512, 1024, 2048, 3072, 5120]
                    if (selectedIndex >= 0 && selectedIndex < limits.length)
                        music.setAudioCacheLimit(limits[selectedIndex])
                }

            }
            Label {
                id: cacheInfoLabel
                multiline: true
                textStyle {
                    base: SystemDefaults.TextStyles.SmallText
                    color: ui.palette.textOnPlain
                }
            }
            Button {
                text: qsTr("清除图片缓存")
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: {
                    cacheInfoLabel.text = qsTr("已删除 %1 个文件").arg(music.clearImageCache())
                }
            }
            Button {
                text: qsTr("清除歌单缓存")
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: {
                    music.clearPlaylistCache()
                    cacheInfoLabel.text = qsTr("歌单缓存已清除")
                }
            }
            Button {
                // 只删已下载的歌曲文件，不影响正在播放
                text: qsTr("清除音乐缓存")
                horizontalAlignment: HorizontalAlignment.Fill
                onClicked: {
                    cacheInfoLabel.text = qsTr("已删除 %1 个音乐文件")
                                          .arg(music.clearAudioCache())
                }
            }




        }
    }
}
