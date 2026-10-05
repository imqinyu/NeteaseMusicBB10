// AppCover - 多任务视图（Active Frame）封面（默认版，未单独适配的机型走这份）
//
// ── 版式 ──────────────────────────────────────────────────────────────
//   占位层：纯黑（没封面 / 未在播放时显示的就是它，配底下的「未在播放」一行字）
//   封面层：真封面铺满
//   信息条：歌词 / 歌名 + 翻译 / 作者，高度自适应（带最小高度）
//
// ── 结构为什么是两个元素 ──────────────────────────────────────────────
//   占位那个【从 app 启动就是 visible】，所以第一帧就能渲染出来
//   —— 「未在播放」靠它显示。
//   如果合成一个 ImageView、靠 imageSource 切换，第一帧会被系统吃掉，
//   静态状态下（未在播放 / 还没歌词）就再也补不回来了。
//
// ── 其余几条必须守住的规则（都是踩出来的）────────────────────────────
//
// ★★ 信息条用 minHeight 给一个【下限】，不要用 preferredHeight。
//   · minHeight      → 高度不低于它，歌词多时照旧长高 ✓
//   · preferredHeight → 高度被写死，歌词少时底部空一大块 ✗（嫌丑）
//
// ★★ imageSource 必须【永远非空】（C++ 的 coverImagePath 已保证）：
//   空 → 非空的那次赋值不会触发重绘。
//
// ★★ scalingMethod 只能用【字面量】，不能写成三元表达式！
//   写成 `x ? ScalingMethod.AspectFill : ...` 时 Qt 4.8 会把枚举当成整数，
//   运行时报 "Invalid scaling method." 并刷屏。
//
// ── 试过但【不要】再走的路（都无效或有副作用）────────────────────────
//   · preferredHeight 固定高度 → 歌词少时底部空一大块，很难看；
//   · 加 maxLineCount → BB10 的 Label 没这个属性，整个文件加载失败、cover 消失；
//   · 合成单个 ImageView 靠 imageSource 切换 → 第一帧被吃掉，静态时补不回来；
//   · 在 cover 里引用 `player.mediaState` → MediaPlayer 自带本地窗口，会把渲染带崩；
//   · 用 Connections 显式赋值 → Cascades 里没有这个类型，文件直接加载失败；
//   · 只补发 coverInfoChanged（值不变）→ 绑定不重算，无效；
//   · 只改一个零宽空格来"推"重绘 → 渲染结果等价，被系统优化掉，无效。
//
// ★★ 一律用【固定数值】，绝对不要用 ui.sdu()！
//    SceneCover 的 content 处在独立的 QML 上下文里，`ui` 这个对象【取不到】；
//    一旦用到它，整个 content 渲染不出来 —— 表现就是多任务视图里一片黑。
//
// ★ content 里【不要引用外层 id】（coverRoot.xxx）—— 拿不到外层作用域，
//   只能用注册在文档上下文上的 music。content【自己】的 id 是安全的。
// ★ 封面里【不要】放可交互控件 —— 多任务视图里的控件拿不到触摸事件。

import bb.cascades 1.4

SceneCover {
    id: coverRoot

    content: Container {
        background: Color.Black
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        layout: DockLayout {
        }

        // ---- 占位层：没封面 / 未在播放（纯黑，配信息条里的「未在播放」）----
        Container {
            visible: ! music.coverHasArt
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            background: Color.Black
        }

        // ---- 封面层：铺满 ----
        ImageView {
            visible: music.coverHasArt
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            scalingMethod: ScalingMethod.AspectFill
            imageSource: music.coverImagePath
        }

        // ---- 底部信息条（minHeight 是下限，歌词多时照旧长高）----
        Container {
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Bottom
            background: Color.create("#CC000000")
            minHeight: 130
            leftPadding: 20
            rightPadding: 20
            topPadding: 10
            bottomPadding: 16
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }
            Label {
                // 歌词（开着且出词了）/ 歌名 / 「未在播放」—— 全在 C++ 里判好
                text: music.coverTitle
                multiline: true
                textStyle {
                    base: SystemDefaults.TextStyles.TitleText
                    color: Color.White
                }
            }
            Label {
                // 翻译 / 「歌名 - 作者」/ 作者 —— 同上
                text: music.coverSubtitle
                multiline: true
                textStyle {
                    base: SystemDefaults.TextStyles.SubtitleText
                    color: Color.create("#cccccc")
                }
            }
        }
    }
}
