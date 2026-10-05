// NowPlayingPage - 正在播放页（全屏）
//
// ★ 布局严格照官方 Music ui 的 NewPlayerScreen.qml：
//     [ NowPlayingHeader ]  标题栏：歌名(9.6/W400) + 艺术家(8/Normal)，高 11du
//     [ Divider ]
//     [ 专辑名 + 随机/循环 ]  NowPlayingAlbumListHeader（高 10.8du）
//     [ 封面 ]               ALBUM_COVER_HEIGHT = 76.8du（全宽正方形）
//     [ 时间条 ]             NowPlayingTimeSlider（细线 + 圆点，左已播/右 -剩余）
//     控制：上一首 / 下一首 在 ActionBar（OnBar），播放·暂停用 Signature
//
// 官方那套 TimeSlider / Repeat/Shuffle 按钮是 mm.extension 的自定义组件、
// 依赖 PlaybackController，搬不过来；这里按视觉复刻，行为接 music / player。
// 循环（列表循环 ⇄ 单曲循环）与随机（开/关）是两个独立按钮，见 repeatIcon / shuffleIcon。

import bb.cascades 1.4
import bb.multimedia 1.0

Page {
    id: nowPlayingPage
    objectName: "nowPlayingPage"

    actions: [
        ActionItem {
            /*
             * ★ 音乐详情（Properties）挂在【ActionBar】上，不用页面下拉菜单：
             *   Page 的 Menu.definition 在"有 ActionBar 的页面"上会被收进
             *   overflow，实际非常难发现（真机反馈"这个 action 找不到"）。
             * ★ 本页是独立推出来的、拿不到 main 的 root，所以走
             *   music.requestOpenProperties()，由 main.qml 代推页面。
             */
            //音乐详情不能挤占音乐控制的位置！老老实实放overflow就行 黑莓官方就是这么干得
            title: qsTr("音乐详情")
            imageSource: "asset:///icons/ic_info.png"
            ActionBar.placement: ActionBarPlacement.InOverflow
            enabled: nowPlayingPage.hasTrack
            onTriggered: {
                music.requestOpenProperties()
            }
        },
        ActionItem {
            title: qsTr("上一首")
            imageSource: "asset:///icons/ic_media_previous.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: {
                music.prev()
            }
        },
        ActionItem {
            // 文字随状态切换：播放中显示「暂停」，否则显示「播放」
            title: player.mediaState == MediaState.Started ? qsTr("暂停") : qsTr("播放")
            imageSource: player.mediaState == MediaState.Started
                         ? "asset:///icons/ic_pause.png"
                         : "asset:///icons/ic_play.png"
            ActionBar.placement: ActionBarPlacement.Signature
            onTriggered: {
                if (player.mediaState == MediaState.Started)
                    player.pause()
                else if (music.playUrlVersion > 0)
                    player.play()
            }
        },
        ActionItem {
            title: qsTr("下一首")
            imageSource: "asset:///icons/ic_media_next.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: {
                music.next()
            }
        },
        ActionItem {
            title: qsTr("评论")
            imageSource: "asset:///icons/ic_chat_multiperson.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: {
                music.requestCommentsForCurrent()
            }
        },
        ActionItem {
            title: qsTr("打开专辑")
            imageSource: "asset:///icons/ic_music_library.png"
            // 放不下就进溢出菜单（Default），不占动作栏
            ActionBar.placement: ActionBarPlacement.Default
            /*
             * ★ 必须走 requestOpenAlbum（按专辑 id 拉 /album），
             *   不能走 requestOpenSearch（按专辑名搜歌曲）—— 搜出来的
             *   不是这张专辑的歌，而且拿不到专辑信息填头部。
             */
            enabled: nowPlayingPage.hasTrack
                     && music.currentTrack.albumId
                     && String(music.currentTrack.albumId).length > 0
                     && String(music.currentTrack.albumId) !== "0"
            onTriggered: {
                music.requestOpenAlbum(String(music.currentTrack.albumId),
                                       music.currentTrack.albumName)
            }
        },
        ActionItem {
            // 播放队列（就是当前播放列表 music.songs）。
            // ★ 独立推出来的页拿不到 main.qml 的 root，所以用 music 的
            //   「请求 + 版本号」让 main 代推（kind == "queue"）。
            title: qsTr("播放队列")
            imageSource: "asset:///icons/ic_playlist_audio.png"
            ActionBar.placement: ActionBarPlacement.Default
            enabled: music.songCount() > 0
            onTriggered: {
                music.requestOpenQueue()
            }
        }
    ]

    // 是否真的有在播放的曲目（必须看 name，空 map 在 JS 里是真值）
    property bool hasTrack: music.currentTrackVersion > 0
                            && music.currentTrack
                            && music.currentTrack.name !== undefined
                            && music.currentTrack.name.length > 0

    // 时间条用现成的 ProgressBar 组件（见页面底部），其内部自行维护时间标签。

    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        background: ui.palette.plainBase

        // ★ 原来这里的「长按 → 打开专辑 / 播放队列」已挪到上面的 actions 里
        //   （用户要求放进动作栏，长按菜单里不再重复一份）。

        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        // ---- 顶部标题栏（NowPlayingHeader：11du，左右 2du）----
        Container {
            horizontalAlignment: HorizontalAlignment.Fill
            preferredHeight: ui.sdu(11)
            minHeight: ui.sdu(11)
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }
            Container {
                verticalAlignment: VerticalAlignment.Center
                layout: StackLayout {
                    orientation: LayoutOrientation.TopToBottom
                }
                // 歌名：官方 9.6pt / W400
                Label {
                    text: nowPlayingPage.hasTrack ? music.currentTrack.name
                                                  : qsTr("未在播放")
                    textStyle {
                        base: SystemDefaults.TextStyles.TitleText
                        fontSize: FontSize.PointValue
                        fontSizeValue: 9.6
                        fontWeight: FontWeight.W400
                    }
                }
                // 艺术家：官方 8pt / Normal / 次要色
                Label {
                    text: nowPlayingPage.hasTrack ? music.currentTrack.artistsText : ""
                    topMargin: ui.sdu(0)
                    textStyle {
                        base: SystemDefaults.TextStyles.SubtitleText
                        fontSize: FontSize.PointValue
                        fontSizeValue: 8.0
                        fontWeight: FontWeight.Normal
                        color: ui.palette.textOnPlain
                    }
                }
            }
        }

       // Divider {
       // }

        // ---- 专辑名 + 随机/循环（NowPlayingAlbumListHeader：10.8du）----
        Container {
            horizontalAlignment: HorizontalAlignment.Fill
            minHeight: ui.sdu(8)
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }

            // 专辑名（用当前歌曲的专辑名，比歌单名更贴合"正在播放"语境）
            Label {
                text: nowPlayingPage.hasTrack
                      ? (music.currentTrack.albumName ? music.currentTrack.albumName : "")
                      : ""
                verticalAlignment: VerticalAlignment.Center
                layoutProperties: StackLayoutProperties {
                    spaceQuota: 1
                }
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                }
            }

            /*
             * 两个独立控件：
             *   循环：列表循环 ⇄ 单曲循环（music.repeatMode）
             *   随机：开 / 关（music.shuffle，和循环互不影响）
             * ★ 两者都只改设置、不碰当前曲目：切完之后正在放的这首继续放完，
             *   下一首怎么挑交给 C++ 的 next()（不再立刻把歌切掉）。
             */
            ImageView {
                id: repeatIcon
                preferredWidth: ui.sdu(5.5)
                preferredHeight: ui.sdu(5.5)
                verticalAlignment: VerticalAlignment.Center
                scalingMethod: ScalingMethod.AspectFit
                imageSource: music.repeatMode == 2
                             ? "asset:///icons/ic_repeat_one_active.png"
                             : "asset:///icons/ic_repeat.png"
                gestureHandlers: [
                    TapHandler {
                        onTapped: music.toggleRepeatMode()
                    }
                ]
            }
            ImageView {
                id: shuffleIcon
                leftMargin: ui.sdu(2)
                preferredWidth: ui.sdu(5.5)
                preferredHeight: ui.sdu(5.5)
                verticalAlignment: VerticalAlignment.Center
                scalingMethod: ScalingMethod.AspectFit
                imageSource: music.shuffle
                             ? "asset:///icons/ic_shuffle_all_active.png"
                             : "asset:///icons/ic_shuffle_all.png"
                gestureHandlers: [
                    TapHandler {
                        onTapped: music.toggleShuffle()
                    }
                ]
            }
        }

        // ---- 内容区：封面（全宽 76.8du）+ 时间条 ----
        Container {
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Center
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }

            ImageView {
                horizontalAlignment: HorizontalAlignment.Center
                preferredWidth: ui.sdu(76.8)
                preferredHeight: ui.sdu(76.8)
                scalingMethod: ScalingMethod.AspectFit
                /*
                 * ★ 大封面走 music.bigImagePath()（独立的 640px 缓存）。
                 *   列表那份缓存为了滑动流畅把图缩到 160px，铺到 76.8du
                 *   （≈230~250px）上会明显发糊；这里必须用大图。
                 * ★ 表达式里带上 imageCacheVersion：本页是直接调函数取路径的
                 *   （不像列表那样走模型字段），不引用版本号的话图下完了
                 *   绑定不会重新求值，封面就一直停在占位图上。
                 */
                imageSource: (music.imageCacheVersion >= 0
                              && nowPlayingPage.hasTrack
                              && music.bigImagePath(music.currentTrack.artUrl).length > 0)
                             ? music.bigImagePath(music.currentTrack.artUrl)
                             : "asset:///images/ic_default.png"
            }

            // 时间条：直接用现成的 ProgressBar 组件（assets/ProgressBar.qml）。
            // 它内部就是 Slider + 自带时间标签（照官方 TimeSlider 结构），
            // 拖动松手 → music.seekTo()。★ 不要再在外面加标签，会重复。
            ProgressBar {
                topMargin: ui.sdu(2)
                leftPadding: ui.sdu(2.5)
                rightPadding: ui.sdu(2.5)
                duration: player.duration > 0 ? player.duration : 1
                position: music.playerPosition
            }
        }
    }
}
