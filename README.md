# eu4-mac-cjk

让苹果电脑 (Mac) 上的《欧陆风云4》能显示中文。

Windows 上显示中文要装 [EU4dll](https://github.com/matanki-saito/EU4dll) (双字节补丁), 它不支持苹果电脑。
这个项目照着它的做法, 给苹果版游戏写一个插件 (dylib, 苹果电脑上的「dll」)。

另外还有一个苹果版 [eu4dll_mac](https://github.com/PoXiao-zero/eu4dll_mac) (2026 年 5 月起, 功能更多: 拼音搜索、东亚人名姓在前等, 要用终端安装)。
本项目是另外独立写的, 特点是双击就能安装, 在 Steam 版上试过。**两个只装一个。**

Steam 创意工坊: https://steamcommunity.com/sharedfiles/filedetails/?id=3838140278

**已经能用了:** 主菜单、进游戏以后的界面、提示框、地图上的国名省名都能显示中文。

![地图](docs/map.jpg)

![主菜单](docs/main-menu.jpg)

- 游戏: Steam 版《欧陆风云4》1.37.5
- 中文模组: Steam 创意工坊「Chinese Language Mod for 1.37」(可以再加「Chinese Language Supplementary Mod for 1.37」)

## 怎么装

1. 在 Steam 创意工坊订阅中文模组「Chinese Language Mod for 1.37」(想要更全的国名人名, 再订阅「Chinese Language Supplementary Mod for 1.37」)。
2. 到 [发布页](https://github.com/aizhang453-hash/eu4-mac-cjk/releases) 下载最新的 `eu4-mac-cjk-vX.Y.zip`, 解压。
3. 先关掉游戏, 双击「安装.command」。
   如果提示「无法打开」或「无法验证开发者」: 打开 系统设置 → 隐私与安全性, 往下拉, 点「仍要打开」, 再双击一次。
4. 从 Steam 开游戏, 在 Paradox 启动器里把中文模组加进模组列表就行。

- 卸载: 双击「卸载.command」, 换回原版游戏程序。
- Steam 更新游戏、或者「验证游戏文件完整性」以后, 插件会被冲掉, 再双击一次「安装.command」。
- 出问题看日志: `~/Library/Logs/eu4-mac-cjk.log`。

### 自己编

要先装 Xcode 的命令行工具 (`xcode-select --install`)。`make` 编插件, `tools/install.sh` 装进游戏, `tools/uninstall.sh` 还原, `tools/make-release.sh v1.0` 打包。

## 现在还不行的

- 真的拼音输入法只简单试过; 输入法候选框的位置没调。
- 输入框里按住 Shift 选字、复制粘贴、鼠标点到两个汉字中间, 可能出怪字。
- 提示框换行时, 句号逗号可能跑到行首。
- 和平谈判、外交动作菜单、多人聊天、地图上临时弹出的字等还没试过。

非官方项目, 跟 Paradox 和 EU4dll 的作者都没有关系。

*A double-byte (CJK) text patch for the macOS version of Europa Universalis IV 1.37.5 (Steam) — a Mac counterpart of EU4dll. Download the zip from Releases and double-click 安装.command.*

## 许可

MIT。做法参考了 EU4dll (MIT 许可)。
