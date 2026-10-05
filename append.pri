# ============================================================================
#  NeteaseMusic - 项目级额外配置
#
#  为什么单独放一个文件：
#    config.pri 是 Momentics IDE 自动生成的，你在 IDE 里刷新项目/加文件时
#    它会被整个重写。所以「需要长期保留」的配置放这里，再由 .pro 引入。
#
#  （以下规则全部继承自 BBTieba 的实测经验，改动前先读注释）
# ============================================================================

# ---------------------------------------------------------------------------
# 1) 逻辑层源文件（src/nm 下全是纯 Qt，可在桌面 Qt 上编译测试）
#
#    qmake 的三个坑（都有实测依据）：
#      a) contains(SOURCES, NmJson.cpp) 匹配不到 $$BASEDIR/src/nm/util/NmJson.cpp。
#         qmake 的 contains() 是「未锚定的整串正则匹配」，必须写
#         .*NmJson\.cpp.* 才可靠；
#      b) $$files() 返回反斜杠路径，而 config.pri 里是正斜杠，字符串不同
#         会被当成两个文件，所以先统一成正斜杠；
#      c) $$unique() 在 Qt 4.8 这套 qmake 里是 no-op，不能用来去重。
#
#    结论：逐个文件判断，只追加缺失的那些。
# ---------------------------------------------------------------------------
NM_EXISTING_SOURCES = $$replace(SOURCES, \\\\, /)
NM_EXISTING_HEADERS = $$replace(HEADERS, \\\\, /)

NM_LAYER_SOURCES = \
    $$files($$BASEDIR/src/nm/util/*.cpp) \
    $$files($$BASEDIR/src/nm/net/*.cpp) \
    $$files($$BASEDIR/src/nm/session/*.cpp) \
    $$files($$BASEDIR/src/nm/model/*.cpp) \
    $$files($$BASEDIR/src/nm/api/*.cpp) \
    $$quote($$BASEDIR/src/MusicController.cpp)

NM_LAYER_HEADERS = \
    $$files($$BASEDIR/src/nm/util/*.hpp) \
    $$files($$BASEDIR/src/nm/net/*.hpp) \
    $$files($$BASEDIR/src/nm/session/*.hpp) \
    $$files($$BASEDIR/src/nm/model/*.hpp) \
    $$files($$BASEDIR/src/nm/api/*.hpp) \
    $$quote($$BASEDIR/src/MusicController.hpp)

SOURCES = $$NM_EXISTING_SOURCES
HEADERS = $$NM_EXISTING_HEADERS

NM_ADDED_SOURCES =
for(NM_FILE, NM_LAYER_SOURCES) {
    # basename() 必须内联进 contains()：qmake 的 for() 变量在函数参数里是
    # 提前展开的，先赋给中间变量会拿到列表最后一个值。
    !contains(SOURCES, .*$$basename(NM_FILE).*) {
        SOURCES += $$NM_FILE
        NM_ADDED_SOURCES += X
    }
}

for(NM_FILE, NM_LAYER_HEADERS) {
    !contains(HEADERS, .*$$basename(NM_FILE).*) {
        HEADERS += $$NM_FILE
    }
}

message(NeteaseMusic: append.pri added $$size(NM_ADDED_SOURCES) source(s); total sources = $$size(SOURCES))

# ---------------------------------------------------------------------------
# 2) 头文件搜索路径：代码里用 #include "net/NmHttpClient.hpp" 这种相对
#    src/nm 的写法，所以要把 src/nm 和 src 都加进来。
# ---------------------------------------------------------------------------
INCLUDEPATH += \
    $$quote($$BASEDIR/src) \
    $$quote($$BASEDIR/src/nm)

# ---------------------------------------------------------------------------
# 3) zlib：网易接口会返回 gzip 压缩的响应，Qt 4.8 不会自动解压。
#    BB10 目标系统自带 libz，链上即可。
#    代码用 NM_HAVE_ZLIB 宏控制，未定义时不发 Accept-Encoding。
#    如果链接报 cannot find -lz，把下面两行注释掉即可（功能不受影响）。
# ---------------------------------------------------------------------------
DEFINES += NM_HAVE_ZLIB
LIBS += -lz

# ---------------------------------------------------------------------------
# 3b) bb.multimedia：QML 里的 MediaPlayer 控件需要。
#     （官方 AudioPlayer 示例即使只用 QML MediaPlayer 也链了它）
# ---------------------------------------------------------------------------
LIBS += -lbbmultimedia

# ---------------------------------------------------------------------------
# 3c) bb.system：bb::system::SystemToast（notifyError 的轻提示）
#     ★ 没有它链接会报 undefined reference to bb::system::SystemToast
# ---------------------------------------------------------------------------
LIBS += -lbbsystem

# ---------------------------------------------------------------------------
# 4) Qt 模块
#    network：QNetworkAccessManager 等
#    gui    ：QImage —— 封面下载后要缩成小图再落盘（列表滑动流畅度的关键）
# ---------------------------------------------------------------------------
QT += network gui

# ---------------------------------------------------------------------------
# 5) 中间文件位置（默认会跑到工程外面去，既污染工作区又清不干净）
# ---------------------------------------------------------------------------
OBJECTS_DIR = $$PWD/.obj
MOC_DIR     = $$PWD/.moc
RCC_DIR     = $$PWD/.rcc
UI_DIR      = $$PWD/.ui

# ---------------------------------------------------------------------------
# 6) 编译告警
# ---------------------------------------------------------------------------
QMAKE_CXXFLAGS += -Wall
