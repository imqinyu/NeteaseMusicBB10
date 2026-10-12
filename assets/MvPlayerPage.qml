// MV playback uses the MediaPlayer-created screen window (NDK MediaPlayer example).
import bb.cascades 1.4
import bb.multimedia 1.0

Page {
    id: mvPage
    objectName: "mvPlayerPage"
    property string videoUrl: ""
    property string videoTitle: ""
    property string videoArtist: ""
    property string windowKey: "mvSurfaceWindow"
    property bool pageActive: false
    property bool resumeAudio: false
    property string audioSource: ""
    property string playbackError: ""
    property bool completed: false
    property bool startingPlayback: false
    property bool videoReady: pageActive && videoUrl.length > 0 && mvSurface.windowGroup.length > 0
    onVideoReadyChanged: {
        if (videoReady)
            tryPlay()
    }
    titleBar: TitleBar { title: videoTitle }
    Menu.definition: MenuDefinition {
        actions: [
            ActionItem {
                title: qsTr("正在播放")
                imageSource: "asset:///icons/ic_play_on.png"
                onTriggered: {
                    mvPage.stopPlayback(true)
                    music.requestOpenNowPlaying()
                }
            }
        ]
    }
    actions: [
        ActionItem {
            title: mvPlayer.mediaState == MediaState.Started ? qsTr("暂停") : qsTr("播放")
            imageSource: mvPlayer.mediaState == MediaState.Started ? "asset:///icons/ic_pause.png" : "asset:///icons/ic_play.png"
            ActionBar.placement: ActionBarPlacement.Signature
            onTriggered: {
                if (mvPlayer.mediaState == MediaState.Started)
                    mvPlayer.pause()
                else {
                    mvPage.pageActive = true
                    mvPage.tryPlay()
                }
            }
        },
        InvokeActionItem {
            title: qsTr("浏览器打开")
            imageSource: "asset:///icons/ic_browser.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            query {
                invokeActionId: "bb.action.OPEN"
                uri: mvPage.videoUrl
                invokeTargetId: "sys.browser"
            }
        },
        ActionItem {
            title: qsTr("复制链接")
            imageSource: "asset:///icons/ic_copy.png"
            onTriggered: {
                music.copyToClipboard(mvPage.videoUrl)
                music.notifyError(qsTr("已复制链接"))
            }
        }
    ]
    Container {
        layout: StackLayout { orientation: LayoutOrientation.TopToBottom }
        Container {
            id: videoFrame
            property real frameWidth: 0
            horizontalAlignment: HorizontalAlignment.Fill
            // Use the actual Cascades layout width, including orientation changes.
            preferredHeight: frameWidth > 0 ? frameWidth * 9 / 16 : ui.sdu(40)
            minHeight: preferredHeight
            maxHeight: preferredHeight
            background: Color.Black
            layout: DockLayout {}
            attachedObjects: LayoutUpdateHandler {
                onLayoutFrameChanged: {
                    if (layoutFrame.width > 0)
                        videoFrame.frameWidth = layoutFrame.width
                }
            }
            ForeignWindowControl {
                id: mvSurface
                windowId: mvPage.windowKey
                visible: boundToWindow
                updatedProperties: WindowProperty.Size | WindowProperty.Position | WindowProperty.Visible
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill
                onBoundToWindowChanged: {
                    music.trace("MV surface bound=" + boundToWindow + " window=" + windowId)
                }
            }
        }
        Container {
            visible: mvPage.playbackError.length > 0 || !mvSurface.boundToWindow || mvPage.completed
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            Label {
                multiline: true
                text: mvPage.playbackError.length > 0 ? mvPage.playbackError
                      : mvPage.completed ? qsTr("播放完毕") : qsTr("正在加载视频…")
            }
        }
        ProgressBar {
            externalSeek: true
            duration: mvPlayer.duration > 0 ? mvPlayer.duration : 1
            position: mvPlayer.position
            onSeekRequested: music.seekVideo(mvPlayer, msec)
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
        }
        // ---- 视频窗口下方：分段「简介 / 评论」----
        SegmentedControl {
            id: mvMode
            // 切到「评论」才去拉，避免进页面就多发一次请求
            onSelectedValueChanged: {
                if (selectedValue == "comments")
                    music.requestMvComments()
            }
            Option {
                text: qsTr("简介")
                value: "intro"
                selected: true
            }
            Option {
                text: qsTr("评论")
                value: "comments"
            }
        }

        // ---- 简介 ----
        Container {
            visible: mvMode.selectedValue == "intro"
            leftPadding: 16
            rightPadding: 16
            topPadding: 12
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }
            Label {
                text: mvPage.videoTitle
                textStyle {
                    base: SystemDefaults.TextStyles.TitleText
                }
            }
            Label {
                text: mvPage.videoArtist
                textStyle {
                    base: SystemDefaults.TextStyles.SubtitleText
                    color: ui.palette.textOnPlain
                }
            }
        }

        // ---- 评论（复用歌曲评论那套模型 / 解析）----
        Container {
            visible: mvMode.selectedValue == "comments"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            // 加载中
            Container {
                visible: music.loading
                leftPadding: 16
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }
                ActivityIndicator {
                    running: true
                    preferredWidth: ui.sdu(6)
                    preferredHeight: ui.sdu(6)
                    verticalAlignment: VerticalAlignment.Center
                }
                Label {
                    text: qsTr("正在加载评论…")
                    verticalAlignment: VerticalAlignment.Center
                    textStyle {
                        base: SystemDefaults.TextStyles.SmallText
                    }
                }
            }
            // 空状态
            Label {
                visible: (!music.loading) && music.commentsVersion > 0
                         && music.commentCount() == 0
                // ★ Label 不支持 padding（会刷 "Padding is not supported"）→ 用 margin
                leftMargin: 16
                rightMargin: 16
                topMargin: 16
                text: qsTr("还没有评论")
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                    color: ui.palette.textOnPlain
                }
            }
            ListView {
                dataModel: music.comments
                property variant controller: music
                listItemComponents: [
                    ListItemComponent {
                        type: ""
                        Container {
                            leftPadding: 16
                            rightPadding: 16
                            topPadding: 8
                            bottomPadding: 8
                            layout: StackLayout {
                                orientation: LayoutOrientation.TopToBottom
                            }
                            Label {
                                text: ListItemData.userName
                                textStyle {
                                    base: SystemDefaults.TextStyles.SubtitleText
                                    color: ui.palette.primary
                                }
                            }
                            Label {
                                text: ListItemData.content
                                multiline: true
                                textStyle {
                                    base: SystemDefaults.TextStyles.BodyText
                                }
                            }
                            Label {
                                // ★ Label 不支持 padding（真机日志会刷
                                //   "Padding is not supported for this control"），用 margin 代替
                                text: ListItemData.timeText
                                topMargin: 4
                                textStyle {
                                    base: SystemDefaults.TextStyles.SmallText
                                    color: ui.palette.textOnPlain
                                }
                            }
                        }
                    }
                ]
            }
        }
    }

    function startPlayback() {
        pageActive = true
        tryPlay()
    }
    function tryPlay() {
        if (!videoReady || startingPlayback || mvPlayer.mediaState == MediaState.Started)
            return
        playbackError = ""
        // Pause music once per video session, and only restore the same paused source.
        if (player.mediaState == MediaState.Started) {
            resumeAudio = true
            audioSource = player.sourceUrl.toString()
            player.pause()
        }
        if (mvPlayer.sourceUrl.toString() != videoUrl)
            mvPlayer.sourceUrl = videoUrl
        completed = false
        startingPlayback = true
        // play() prepares the media and creates the video window. Never wait for binding here.
        var result = mvPlayer.play()
        music.trace("MV play result=" + result + " window=" + mvSurface.windowId + " group=" + mvSurface.windowGroup)
        if (result != MediaError.None) {
            startingPlayback = false
            playbackError = qsTr("视频播放失败（错误 %1），可重试或在浏览器打开").arg(result)
            restoreMusic()
        }
    }
    function restoreMusic() {
        if (resumeAudio && player.mediaState == MediaState.Paused
                && player.sourceUrl.toString() == audioSource)
            player.play()
        resumeAudio = false
    }
    function stopPlayback(restoreAudio) {
        pageActive = false
        startingPlayback = false
        mvPlayer.stop()
        if (restoreAudio)
            restoreMusic()
        else
            resumeAudio = false
    }
    attachedObjects: [
        MediaPlayer {
            id: mvPlayer
            videoOutput: VideoOutput.PrimaryDisplay
            windowId: mvSurface.windowId
            windowGroupId: mvSurface.windowGroup
            onError: {
                mvPage.startingPlayback = false
                mvPage.playbackError = qsTr("视频播放失败（错误 %1），可重试或在浏览器打开").arg(mediaError)
                music.trace("MV error=" + mediaError + " position=" + position)
                mvPage.restoreMusic()
            }
            onMediaStateChanged: {
                if (mediaState == MediaState.Started || mediaState == MediaState.Paused || mediaState == MediaState.Stopped)
                    mvPage.startingPlayback = false
            }
            onPlaybackCompleted: {
                mvPage.completed = true
                mvPage.startingPlayback = false
                mvPlayer.stop()
            }
        }
    ]
}
