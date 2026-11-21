SerialPlot @VERSION@ — macOS (@ARCH@)
构建日期 / Built: @BUILD_DATE@

============================================================
中文说明
============================================================

安装
----
1. 把左边的 SerialPlot.app 拖到 Applications（应用程序）文件夹。
2. 在「应用程序」中双击 SerialPlot 启动。

首次打开（重要）
----------------
此安装包没有使用 Apple Developer ID 证书签名，也没有经过 Apple 公证
（notarization），因此 Gatekeeper 会阻止直接双击打开。请任选一种方式：

  * 在「应用程序」中按住 Control 键点按（或右键点按）SerialPlot，
    选择「打开」，然后在弹窗中再次点按「打开」。
  * 先双击一次，若提示无法打开，前往「系统设置 → 隐私与安全性」，
    在页面底部点按「仍要打开」。
  * 在终端中清除下载隔离属性：

        xattr -dr com.apple.quarantine /Applications/SerialPlot.app

    然后重新打开。如果系统提示“应用已损坏”，通常也是隔离属性导致的，
    执行上面的命令即可。

系统要求
--------
  * macOS @MIN_MACOS@ 或更高版本
  * 处理器架构：@ARCH@（Apple Silicon 原生）。Intel Mac 需要 x86_64
    或 universal 版本。

使用提示
--------
  * 串口设备会出现在端口列表中；如果使用 BLE 蓝牙设备，首次扫描时
    请允许 SerialPlot 使用蓝牙。
  * 快捷操作：把配置文件（.ini）拖到窗口上即可加载；也可以双击 .ini
    文件，或通过菜单中的保存/加载配置功能管理。

许可证
------
SerialPlot 以 GNU GPLv3 发布，完整的许可证文本见同目录下的
License.txt。源代码：https://github.com/hyOzd/serialplot

============================================================
English
============================================================

Install
-------
1. Drag SerialPlot.app from this window into the Applications folder.
2. Open SerialPlot from Applications.

First launch (important)
------------------------
This build is not signed with an Apple Developer ID certificate and is not
notarized by Apple, so Gatekeeper will block a normal double-click. Use one
of the following:

  * Control-click (or right-click) SerialPlot in Applications, choose
    "Open", then confirm with "Open" in the dialog.
  * Double-click once, then go to System Settings > Privacy & Security and
    click "Open Anyway" at the bottom of the page.
  * Remove the quarantine flag in Terminal:

        xattr -dr com.apple.quarantine /Applications/SerialPlot.app

    If macOS reports that the app is "damaged", the quarantine flag is
    usually the cause; the same command fixes it.

Requirements
------------
  * macOS @MIN_MACOS@ or later
  * Architecture: @ARCH@ (native Apple Silicon). Intel Macs require an
    x86_64 or universal build.

License
-------
SerialPlot is released under the GNU GPLv3. See License.txt in this disk
image for the full license text. Source code:
https://github.com/hyOzd/serialplot
