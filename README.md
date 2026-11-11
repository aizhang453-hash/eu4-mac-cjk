# eu4-mac-cjk

让苹果电脑 (Mac) 上的《欧陆风云4》能显示中文。

Windows 上显示中文要装 [EU4dll](https://github.com/matanki-saito/EU4dll) (双字节补丁), 它不支持苹果电脑。
这个项目照着它的做法, 给苹果版游戏写一个插件 (dylib, 苹果电脑上的「dll」)。

**还在做, 现在还不能用。** 目前主菜单、进游戏以后的界面、提示框、地图上的国名省名都能显示中文了:

![地图](docs/map.jpg)

![主菜单](docs/main-menu.jpg)

- 游戏: Steam 版《欧陆风云4》1.37.5
- 中文模组: Steam 创意工坊「Chinese Language Mod for 1.37」

非官方项目, 跟 Paradox 和 EU4dll 的作者都没有关系。

*A double-byte (CJK) text patch for the macOS version of Europa Universalis IV — a Mac counterpart of EU4dll. Work in progress.*

## 许可

MIT。做法参考了 EU4dll (MIT 许可)。
