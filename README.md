# Keychron Mouse Firmware Updater for macOS

[简体中文](README.md) | [English](README.en.md)

适用于 Apple Silicon Mac 的 Keychron 鼠标图形化固件升级工具。使用流程是：

1. USB 直连并选择 Keychron 鼠标；
2. 选择该鼠标对应的 `.signed.bin` 固件；
3. 校验设备、固件和版本关系后开始升级。

应用内置独立升级引擎，不需要安装 Python、Wine 或 Windows。为了避免用户误刷，
通用化仅针对已实测的 Keychron HID 升级协议，不会绕过设备 Bootloader 的签名验证。

> Clean-room interoperability implementation. The current build is ad-hoc signed and
> is not Apple-notarized.

## 支持逻辑

程序会发现 Keychron USB VID `0x3434` 下、usage page `0x008c` / usage `0x0001`
的鼠标升级接口，然后读取设备返回的型号、当前版本和协议能力。

固件只有同时满足以下条件才可进入升级确认：

- MCUboot 容器、摘要、key hash 和签名 TLV 结构有效；
- 设备协议为 1、DFU 为 0，并支持标准升级模式；
- 固件内容包含与所选设备完全一致的型号；
- 已知型号的 USB PID、RAM 地址和向量范围正确；
- 降级必须单独确认。

| 型号 | USB PID | 状态 |
|---|---:|---|
| Keychron G6 HE 8K / `54LMG6HE` | `0xd086` | 已实机验证 `1.0.0+82 → 1.0.0+84` |
| 其他返回同协议的 Keychron 鼠标 | 动态识别 | 协议兼容，首次升级前仍需对应型号实机验收 |

“找到设备”只代表接口和协议检查通过，不等于该型号已经完成升级验收。

## 安装与使用

1. 打开 `Keychron-Mouse-Firmware-Updater-v1.1.0-macOS-arm64.dmg`。
2. 将 **Keychron Mouse Firmware Updater** 拖入 Applications。
3. 首次运行若 macOS 拦截，请右键应用并选择“打开”。
4. 将鼠标切换到有线模式并 USB 直连 Mac。
5. 在应用里选择鼠标，再选择匹配的签名固件。
6. 阅读版本、型号和信任状态提示后确认升级。

升级期间程序会阻止 Mac 自动休眠。若传输异常，会尝试让设备退出升级状态；仍然
不要主动拔线或关闭应用。

## 从源码构建

构建机需要 Apple Command Line Tools、Homebrew 和 `hidapi`：

```bash
brew install hidapi
./scripts/build_release.sh
```

产物位于 `dist/`：macOS DMG、`.app` ZIP 和版本对应的 SHA-256 校验文件。

## 命令行

```bash
python3 keychron_mouse_updater.py devices
python3 keychron_mouse_updater.py probe --device DEVICE_ID
python3 keychron_mouse_updater.py inspect firmware.signed.bin
python3 keychron_mouse_updater.py upgrade firmware.signed.bin --device DEVICE_ID --dry-run
python3 keychron_mouse_updater.py upgrade firmware.signed.bin --device DEVICE_ID --confirm DEVICE_MODEL
```

有意降级时还必须增加 `--allow-downgrade`。

## 固件与签名边界

- 应用不会内置或自动选择固件，必须由用户选择与设备型号匹配的厂商签名包。
- 已知发布哈希显示为 `known_release_hash`；其他固件显示
  `bootloader_signature_only`。
- 未配置厂商公钥时，电脑端可以检查签名 TLV 是否存在和结构是否合理，但签名真实性
  最终仍由设备 Bootloader 验证。
- MIT 许可证只覆盖本仓库的应用代码；固件保留其原权利归属。

## License

Application source code: [MIT](LICENSE). Third-party components:
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
