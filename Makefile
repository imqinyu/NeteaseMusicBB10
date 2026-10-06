# ============================================================================
#  NeteaseMusic - 包装 Makefile（从 BBTieba 移植，原理见其 README 第九节）
#
#  解决的问题：.cproject 指定的构建目标（Device-Debug 等）qmake 生成的
#  Makefile 里没有，IDE 执行 `make -j12 Device-Debug` 会直接失败。
#
#  两个必须显式指定的 CONFIG（NDK cascades10.prf 决定）：
#    1) CONFIG+=debug（并 CONFIG-=release）：否则默认命中 release 分支，
#       TEMPLATE 变成 lib，编出来是共享库，部署报
#       "Failed to create application process: Attempting to exec a shared lib"。
#    2) CONFIG+=device（或 simulator）：两套 mkspec 都没有定义这个 CONFIG，
#       不指定的话 DESTDIR 会变成 "-g/" 这种怪目录。
#
#  生成后还要跑 build/fix-destdir.ps1：DESTDIR/OBJECTS_DIR 是相对工程根写的，
#  而 make 在子目录里执行，路径会多套一层同名目录。
#
#  产物路径（与 bar-descriptor.xml 一致）：
#    Device-Debug    -> arm/o.le-v7-g/NeteaseMusic   （无 .so 后缀）
#    Device-Release  -> arm/o.le-v7/NeteaseMusic.so
#    Device-Profile  -> arm-p/o.le-v7-g/NeteaseMusic
#    Simulator-Debug -> x86/o-g/NeteaseMusic
#
#  手工构建：
#    $ndk = 'C:\NDKFiles'
#    $env:QNX_HOST   = "$ndk\host_10_3_1_12\win32\x86"
#    $env:QNX_TARGET = "$ndk\target_10_3_1_995\qnx6"
#    $env:PATH = "$env:QNX_HOST\usr\bin;$env:PATH"
#    cd D:\BBWorkspace\NeteaseMusic
#    make -j4 Device-Debug
# ============================================================================

QNX_HOST   ?=
QNX_TARGET ?=
QMAKE      ?= $(QNX_HOST)/usr/bin/qmake.exe
PROFILE    ?= ../../NeteaseMusic.pro

SPEC_DEVICE    ?= $(QNX_TARGET)/usr/share/qt4/mkspecs/blackberry-armv7le-qcc
SPEC_SIMULATOR ?= $(QNX_TARGET)/usr/share/qt4/mkspecs/blackberry-x86-qcc

# 生成 Makefile 后修正 DESTDIR / OBJECTS_DIR（原因见文件头）
FIXDIRS = powershell -NoProfile -ExecutionPolicy Bypass -File build/fix-destdir.ps1

first: Device-Debug

# --------------------------------------------------------------------------
#  在 $(1) 目录里生成 Makefile（$(2)=mkspec, $(3)=debug|release,
#  $(4)=device|simulator），修正路径后编译。
# --------------------------------------------------------------------------
define BUILD_IN
	@mkdir -p $(1)
	@echo "=== [$(1)] qmake ($(3)/$(4)) ==="
	@cd $(1) && "$(QMAKE)" -spec "$(2)" "$(PROFILE)" -o Makefile \
		"CONFIG+=$(3)" "CONFIG-=$(if $(filter $(3),debug),release,debug)" \
		"CONFIG+=$(4)" "CONFIG-=$(if $(filter $(4),device),simulator,device)"
	@$(FIXDIRS) $(1)/Makefile
	@echo "=== [$(1)] compile ==="
	@cd $(1) && $(MAKE) -f Makefile
endef

# ---------------------------------------------------------------- 配置目标
Device-Debug:
	$(call BUILD_IN,arm/o.le-v7-g,$(SPEC_DEVICE),debug,device)
	@echo "=== [Device-Debug] artifact: arm/o.le-v7-g/NeteaseMusic ==="

Device-Release:
	$(call BUILD_IN,arm/o.le-v7,$(SPEC_DEVICE),release,device)
	@powershell -NoProfile -ExecutionPolicy Bypass -File build/fix-release-artifact.ps1
	@echo "=== [Device-Release] artifact: arm/o.le-v7/NeteaseMusic.so ==="

Device-Profile:
	$(call BUILD_IN,arm-p/o.le-v7-g,$(SPEC_DEVICE),debug,device)
	@echo "=== [Device-Profile] artifact: arm-p/o.le-v7-g/NeteaseMusic ==="

Simulator-Debug:
	$(call BUILD_IN,x86/o-g,$(SPEC_SIMULATOR),debug,simulator)
	@echo "=== [Simulator-Debug] artifact: x86/o-g/NeteaseMusic ==="

# ---------------------------------------------------------------- 其它
all: Device-Debug

# 注意：recipe 行里的 '#' 不是 make 注释 —— make 会把整行交给 shell 执行，
# 中文注释会导致 "cannot execute binary file"。所以说明只能写在配方块外面。
clean:
	@powershell -NoProfile -ExecutionPolicy Bypass -File build/clean.ps1

distclean: clean
	@powershell -NoProfile -ExecutionPolicy Bypass -File build/clean.ps1 distclean

install dist: Device-Debug

.PHONY: first all Device-Debug Device-Release Device-Profile Simulator-Debug \
        clean distclean install dist
