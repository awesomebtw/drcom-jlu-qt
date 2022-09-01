# drcom-jlu-qt
跨平台的 drcom for jlu 客户端

## Forked from [this repo](https://doc.qt.io/qt-5/qsettings.html#platform-specific-notes)
原来的 repo 三年没更新了，目前做了一些改进，具体见更新日志，同时对本文档内容和排版也做了一些调整

# 功能对比
## 优势
| 功能            | 官方  | 本版  | 说明                                      |
|---------------|-----|-----|-----------------------------------------|
| 记住密码、自动登录     | √   | √   |                                         |
| 打开速度          | 慢   | 快   |                                         |
| 单实例           |     | √   | 开机自启慢的话可以直接打开不会报错说已经在运行                 |
| 快速注销          |     | √   | 官方版是真注销，本版是直接关闭 socket，所以不需要等 20s 的发包周期 |
| 快速重启客户端       |     | √   |                                         |
| 托盘图标无 bug     |     | √   | 不知道你们有没有碰到过官方 Win 版托盘有俩图标的 bug          |
| 可选不弹出校园网之窗    |     | √   |                                         |
| 可选完全隐藏登录窗口    |     | √   |                                         |
| 适配高分屏         |     | √   |                                         |
| 不需要管理员 / root |     | √   |                                         |
| Linux 版最小化到托盘 |     | √   |                                         |
| 不限制 NAT       |     | √   | 并不支持有违校方意愿的做法，请自行承担后果                   |

## TODO
* [ ] 被顶掉：警告！巨大缺陷！本版掉线后会自动重启重新登录！所以顶不掉！待改进
* [ ] 有时候重启功能不好使，点了重启当前退了没有蹦出来新的，待改进

# 注意事项
- 掉线后客户端自动重启重连尝试三次。自动重启登录成功后不弹窗口只最小化到托盘。注：自动重启功能依赖于“记住我”选项的勾选，否则没有账户密码自行重启也并没有什么用
- 连接 JLU.PC 登录的时候 MAC 地址随便填就可以，或者随便选一个网卡也可以，只有有线网要求 MAC 地址和网络中心的一致

# 截图
## Windows
![n9c6aQ.png](https://s2.ax1x.com/2019/09/02/n9c6aQ.png)

## Ubuntu
![nCtJ2Q.png](https://s2.ax1x.com/2019/09/02/nCtJ2Q.png)

> [Ubuntu 18 不显示托盘图标的 bug 的解决方案](https://askubuntu.com/questions/1056226/ubuntu-budgie-18-04-lts-system-tray-icons-not-all-showing)

# 构建依赖项
1. Qt6 (Core, Gui, Widget, Network)
2. Crypto++

# 更新日志
* v 1.0.1
  1. 代码现代化，用上 C++11/14 特性，如枚举类、智能指针、`constexpr`，同时代码风格换了；
  2. 迁移到 Qt 6，用 `CMake` 代替 `qmake`;
  3. 替换单例和加密的实现，加密现在使用 Crypto++ 库；
  4. 密码用密文保存，具体来说，使用 Crypto++ 实现的 SM4 算法加密密码；
  5. 配置文件能够正确保存到同一文件夹下的 .ini 文件；
  6. 细节上的修改
* v 1.0.0.6
  * 更换 QUdpSocket 为原生接口，增强稳定性（并不知道为什么QUdpSocket要设置一个状态
  * 增加了两个 checkbox 可选关闭校园网之窗和隐藏登录窗口
* v 1.0.0.5
  * 解决不稳定的bug，自动重启客户端重新登录，新增日志功能，方便查错
* v 1.0.0.4
  * 优化用户体验，调整掉线时的提示信息，增加掉线时直接重启客户端的提示
* v 1.0.0.3
  * 没有这个版本，上次该发布 0.2 版本时候压缩包名字打错了。。。应该为 1.0.0.2 的，所以跳过这个版本号
* v 1.0.0.2
  * 增加重启功能（能解决一些网络的错误）
  * 调整字体为微软雅黑 10 号（就是 Windows 下正常的字体）
* v 1.0.0.1
  * 修复适配高 DPI 时只窗口大小适配但字号不适配的 bug
* v 0.0.0.0
  * 实现基本功能

# 特别感谢
**[图标作者](https://github.com/lyj3516)**

**[jlu 的 drcom 协议细节](https://github.com/drcoms/jlu-drcom-client/blob/master/jlu-drcom-java/jlu-drcom-protocol.md)**

**[唯一实例](https://github.com/itay-grudev/SingleApplication)**

**[登录部分复制了 jlu 部分代码](https://github.com/mchome/dogcom)**

# 许可证
[GNU Affero General Public License v3.0](https://github.com/code4lala/drcom-jlu-qt/blob/master/LICENSE)