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
- 已知型号的 RAM 地址和向量范围正确；
- 目标版本号排序不高于当前版本时必须单独确认。

| 型号 | USB PID | 状态 |
|---|---:|---|
| Keychron G6 HE 8K / `54LMG6HE` | `0xd086` | 已实机验证 `1.0.0+82 → 1.0.0+84` |
| Keychron G6 HE 8K / `54LMG6HE` | `0xd09d` | 协议兼容；该台（`hardware_revision 0503`）已实机写入内测包 `1.0.0+1` 并回读通过 |
| 其他返回同协议的 Keychron 鼠标 | 动态识别 | 协议兼容，首次写入前仍需对应型号实机验收 |

“找到设备”只代表接口和协议检查通过，不等于该型号已经完成升级验收。

### 为什么 USB PID 是提示而不是阻断

USB PID 是设备固件和板级配置提供的描述符字段，不是某个型号的固定属性。同一个型号在
不同生产批次、硬件版本和出厂固件下可能枚举出不同的 PID——G6 HE 8K 本身就同时记录了
`0xd086` 和 `0xd09d` 两个值。

因此型号身份由以下三类证据确定：升级协议返回的型号字符串、签名固件内嵌的型号字符串、
以及该型号已知的 RAM 与向量布局。对已知型号出现未记录的 PID 时，程序会给出提示、降低
验证等级，并在确认对话框中显示，但不会阻断写入。协议级失败仍然阻断，凡配置了 profile
的型号其 RAM/向量范围检查也照旧强制执行。

需要恢复“PID 不匹配即阻断”的严格行为时，可加 `--strict-product-id`。

## 安装与使用

1. 打开 `Keychron-Mouse-Firmware-Updater-v1.3.0-macOS-arm64.dmg`。
2. 将 **Keychron Mouse Firmware Updater** 拖入 Applications。
3. 首次运行若 macOS 拦截，请右键应用并选择“打开”。
4. 将鼠标切换到有线模式并 USB 直连 Mac。
5. 在应用里选择鼠标，再选择匹配的签名固件。
6. 阅读版本先后关系、型号和信任状态提示后确认写入。

写入期间程序会阻止 Mac 自动休眠。若传输异常，会尝试让设备退出升级状态；仍然
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

目标版本号排序不高于当前版本时（更早的构建、同版本重刷、或另一条发布线），
还必须增加 `--allow-version-change`。对 `devices`、`probe`、`upgrade` 增加
`--strict-product-id` 可把“未记录的 USB PID”从提示恢复为阻断。

### 版本先后关系与发布线

工具对版本号只做**纯数字排序**，并如实报告结果：`newer`（目标靠后）、`older`
（目标靠前）、`same`（相等）、`unknown`（无法解析）。它**不会**把结果解释成
“升级”或“降级”，因为版本号代表什么属于厂商的发布约定，通用工具无从得知。

厂商可能同时维护多条发布线，各自使用独立的构建号（例如内测线与正式线）。因此：

- 数字更小的目标不一定是更旧或更差的镜像；
- 数字更大的目标也不一定就是“升级”；
- 只要目标不排序靠后，程序都会要求单独确认，而不是替用户下结论。

此外，**版本号相同不代表内容相同**。程序会同时比较镜像 SHA-256：只有在记录了该
型号该版本正在运行的镜像哈希、且与所选文件一致时，才会判定为 `already_current`
并跳过写入；否则会提示这是“同版本重刷”并进入确认流程。设备协议本身无法回读运行中
镜像的摘要，所以这一判断依赖本项目实际观测并记录过的镜像。

## 固件与签名边界

- 应用不会内置或自动选择固件，必须由用户选择与设备型号匹配的厂商签名包。
- 已知发布哈希显示为 `known_release_hash`；其他固件显示
  `bootloader_signature_only`。
- 未配置厂商公钥时，电脑端可以检查签名 TLV 是否存在和结构是否合理，但签名真实性
  最终仍由设备 Bootloader 验证。
- MIT 许可证只覆盖本仓库的应用代码；固件保留其原权利归属。

## G6 HE 行为等价源码研究

仓库同时归档了独立的
[`reverse_engineering/reimplementation`](reverse_engineering/reimplementation/README.md)
研究子项目。它包含可移植 C11 行为模型、489 项主机测试及一个默认拒绝启用未知硬件
参数的 Zephyr/NCS 工程骨架。

该子项目不是完整固件、不能刷写鼠标，也不提供签名或绕过安全启动的方法。PCB
生产包、厂商固件新包、规格书、反编译输出和私有工程记录不在该公开源码归档中。

## License

Application source code: [MIT](LICENSE). Third-party components:
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
