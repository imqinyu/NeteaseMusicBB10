<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="zh_CN">
<!--
    NeteaseMusic 翻译文件（项目根目录）

    为什么这个文件必须存在：

      config.pri（Momentics 自动生成）里有
          TRANSLATIONS = $$quote($${TARGET}.ts)
      解析出来就是「项目根目录下的 NeteaseMusic.ts」。生成的 Makefile 里会出现
          $(COPY_FILE) --parents NeteaseMusic.ts ...
      如果这个文件不存在，make 会直接报
          make: *** No rule to make target 'NeteaseMusic.ts'
      从而整个构建失败 —— 表现是「改动没生效 / 跑的还是旧包 / 黑屏」，而且
      报错信息完全看不出跟翻译有关，很难查。

    所以：即使暂时不翻译，也请保留这个空壳文件。

    真正做翻译时的流程（Momentics 里）：
      1. 用 lupdate 从源码收集字符串到本文件；
      2. 用 Qt Linguist 填译文；
      3. 用 lrelease 生成 .qm，放到 translations/ 下（bar-descriptor.xml 里
         已声明把 translations/*.qm 打包进 app/native/qm）。
-->
</TS>
