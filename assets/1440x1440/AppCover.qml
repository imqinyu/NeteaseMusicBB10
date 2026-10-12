// SceneCover content. Passport supports both High and Medium detail levels.
// Size is the navigator's cover canvas, not the device's screen resolution.
import bb.cascades 1.4

Container {
    id: coverContent
    property bool supportsMediumCover: true
    property bool coverCompact: appCover.objectName == "medium"
    implicitLayoutAnimationsEnabled: false
    preferredWidth: appCover.size.width > 0 ? appCover.size.width : 310
    preferredHeight: appCover.size.height > 0 ? appCover.size.height : (coverCompact ? 195 : 211)
    minWidth: preferredWidth
    maxWidth: preferredWidth
    minHeight: preferredHeight
    maxHeight: preferredHeight
    background: Color.Black
    layout: DockLayout {}

    ImageView {
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        visible: music.coverHasArt
        scalingMethod: ScalingMethod.AspectFill
        // Active Frame animations are disabled. Cached images must appear without an effect.
        implicitLayoutAnimationsEnabled: false
        loadEffect: ImageViewLoadEffect.None
        imageSource: music.coverImagePath
        onImageChanged: {
            console.log("[COVER] imageChanged level=" + appCover.objectName + " art=" + music.coverHasArt);
        }
        attachedObjects: LayoutUpdateHandler {
            onLayoutFrameChanged: {
                console.log("[COVER] art level=" + appCover.objectName + " frame=" + layoutFrame.width + "x" + layoutFrame.height);
            }
        }
    }

    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Bottom
        background: Color.create("#CC000000")
        // Don't double padding/minHeight on Passport; its cover is much smaller than 1440x1440.
        minHeight: Math.min(130, coverContent.preferredHeight * 0.45)
        leftPadding: Math.min(40, Math.max(12, coverContent.preferredWidth * 0.045))
        rightPadding: leftPadding
        topPadding: coverCompact ? 6 : 10
        bottomPadding: coverCompact ? 8 : 16
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }
        Label {
            text: music.coverTitle
            multiline: true
            textStyle {
                base: SystemDefaults.TextStyles.TitleText
                color: Color.White
            }
        }
        Label {
            text: music.coverSubtitle
            multiline: true
            textStyle {
                base: SystemDefaults.TextStyles.SubtitleText
                color: Color.create("#cccccc")
            }
        }
        attachedObjects: LayoutUpdateHandler {
            onLayoutFrameChanged: {
                console.log("[COVER] info level=" + appCover.objectName + " y=" + layoutFrame.y + " frame=" + layoutFrame.width + "x" + layoutFrame.height);
            }
        }
    }
    attachedObjects: LayoutUpdateHandler {
        objectName: "coverRootLayout"
        onLayoutFrameChanged: {
            console.log("[COVER] root level=" + appCover.objectName + " frame=" + layoutFrame.width + "x" + layoutFrame.height);
        }
    }
}
