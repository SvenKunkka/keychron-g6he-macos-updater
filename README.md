# G6 HE Firmware Updater for macOS

一个适用于 Apple Silicon Mac 的 Keychron G6 HE 图形化固件升级工具。
应用内置独立升级引擎和已验证的 `1.0.0+84` 签名固件，不需要安装
Python、Wine 或 Windows。

> This is a clean-room interoperability implementation. It is not an
> Apple-notarized release.

## 下载与安装

1. 从 [Releases](../../releases/latest) 下载
   `G6HE-Firmware-Updater-v1.0.0-macOS-arm64.dmg`。
2. 打开 DMG，将 **G6 HE Firmware Updater** 拖入 Applications。
3. 首次运行若 macOS 拦截，请右键应用选择“打开”。
4. 用 USB 线直连鼠标并切换到有线模式，再点击“检测设备”和“开始升级”。

## 兼容范围

- Apple Silicon Mac
- macOS 13 或更新版本
- Keychron G6 HE 8K
- USB `VID 3434 / PID d086`
- 设备型号 `54LMG6HE`

应用会拒绝错误机型、损坏固件、未知协议/DFU 版本和需要未验证
Bootloader 切换的设备。

## 内置固件

| 文件 | 版本 | SHA-256 |
|---|---:|---|
| `G6HE_v1.0.0+84_202609101503.signed.bin` | `1.0.0+84` | `38f5bbb0ff3eec06600c766094a01c5da987c75d17192b35acc89dd0aed0695a` |

已完成一次实机升级验收：`1.0.0+82 → 1.0.0+84`，传输
350,953 bytes / 21,935 chunks，设备重启后回读版本为 `1.0.0+84`。
详见 [证据记录](reverse_engineering/evidence.md)。

## 从源码构建

构建机需要 Apple Command Line Tools、Homebrew 和 `hidapi`：

```bash
brew install hidapi
./scripts/build_release.sh
```

产物位于 `dist/`：

- macOS DMG 安装镜像
- `.app` ZIP
- SHA-256 校验文件

命令行引擎也可单独运行：

```bash
python3 g6he_mac_tool.py probe
python3 g6he_mac_tool.py upgrade firmware/*.signed.bin --dry-run
python3 g6he_mac_tool.py upgrade firmware/*.signed.bin --confirm 54LMG6HE
```

## 安全与权利边界

- 固件写入前会校验 MCUboot 结构、内嵌 SHA-512、设备型号及协议能力。
- 包内没有厂商公钥，因此 Ed25519 签名真实性最终由设备 Bootloader 校验。
- 未通过故意断电/拔线测试；升级期间请保持 USB 连接并避免电脑休眠。
- MIT 许可证只覆盖本仓库的应用代码。内置签名固件保留其原权利归属。
- Windows 原升级程序和反编译输出未包含在本仓库中。

## License

Application source code: [MIT](LICENSE). Third-party components:
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
