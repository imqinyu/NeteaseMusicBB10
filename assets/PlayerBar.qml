// PlayerBar - 底部播放条（每个 Tab 页复用同一个 MediaPlayer 实例）
//
// 为什么单独拆一个文件：
//   主界面改成 TabbedPane 后，每个 Tab 是一个独立 Page，TabbedPane 本身
//   没有"全局底部栏"这种东西（官方 ModPlayer 也是把播放器做成单独页面）。
//   把播放条抽成组件，三个 Tab 各放一个，靠传入的 player 引用操作同一个
//   MediaPlayer —— 只有一份播放器实例，不会出现两个声音。
//
// 用法：PlayerBar { player: player }
//   player 是主页面 attachedObjects 里那个 MediaPlayer 的 id。
//
// 组件内部只依赖 context property（music）和传进来的 player，
//  不引用外层页面的任何 id —— 这样它放到哪个页面都能工作。

import bb.cascades 1.3
import bb.multimedia 1.0

Container {
    id: bar

    /*
     * 播放器不通过属性传入，直接用 context property "player"
     * （由 applicationui.cpp 注册，全局唯一一份）。
     * 原因：把 Cascades 对象塞进自定义属性会触发
     * "Unable to set property"，播放控制会整个失效。
     */

    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 10
    background: ui.palette.plainBase
    layout: StackLayout {
        orientation: LayoutOrientation.LeftToRight
    }

    // 正在播放的封面（绑定 imageCacheVersion：封面下完自动出现）
    ImageView {
        preferredWidth: 72
        preferredHeight: 72
        verticalAlignment: VerticalAlignment.Center
        // 没封面/封面还在下载时用占位图 —— 空字符串会让图片加载器报
        // "Unrecognized scheme"，而且看起来是残缺的
        imageSource: music.currentTrackVersion && music.imageCacheVersion >= 0
                     && music.imagePath(music.currentTrack.artUrl).length > 0
                     ? music.imagePath(music.currentTrack.artUrl)
                     : "asset:///images/placeholder.png"
        scalingMethod: ScalingMethod.AspectFit
    }

    Container {
        leftPadding: 12
        verticalAlignment: VerticalAlignment.Center
        layoutProperties: StackLayoutProperties {
            spaceQuota: 1
        }
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        Label {
            text: music.currentTrackVersion
                  ? (music.currentTrack.name + "\n" + music.currentTrack.artistsText)
                  : qsTr("当前没有播放")
            multiline: true
            textStyle {
                base: SystemDefaults.TextStyles.BodyText
            }
        }

        ProgressIndicator {
            fromValue: 0
            toValue: player && player.duration > 0 ? player.duration : 1
            value: player ? player.position : 0
            state: ProgressIndicatorState.Progress
        }
    }

    Container {
        verticalAlignment: VerticalAlignment.Center
        layout: StackLayout {
            orientation: LayoutOrientation.LeftToRight
        }
        Button {
            text: qsTr("上一首")
            onClicked: {
                music.prev()
            }
        }
        Button {
            text: player && player.mediaState == MediaState.Started
                  ? qsTr("暂停") : qsTr("播放")
            onClicked: {
                if (player.mediaState == MediaState.Started) {
                    player.pause()
                } else if (music.playUrlVersion > 0) {
                    player.play()
                }
            }
        }
        Button {
            text: qsTr("下一首")
            onClicked: {
                music.next()
            }
        }
    }
}
