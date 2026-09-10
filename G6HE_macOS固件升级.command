#!/bin/zsh

set -u

TOOL_DIR="${0:A:h}"
DEFAULT_FIRMWARE="$HOME/Downloads/G6HE_v1.0.0+84_202609101503.signed.bin"
FIRMWARE_PATH="${1:-$DEFAULT_FIRMWARE}"
PYTHON_BIN="/opt/homebrew/bin/python3"

if [[ ! -x "$PYTHON_BIN" ]]; then
  PYTHON_BIN="/usr/bin/python3"
fi

clear
echo "G6 HE macOS 固件升级"
echo ""
echo "固件：$FIRMWARE_PATH"
echo "请确认鼠标已用 USB 线连接，升级期间不要拔线。"
echo ""

if ! "$PYTHON_BIN" "$TOOL_DIR/g6he_mac_tool.py" upgrade "$FIRMWARE_PATH" --dry-run; then
  echo ""
  read "REPLY?检查未通过。按回车关闭窗口。"
  exit 1
fi

echo ""
read "CONFIRM?如需开始升级，请输入 54LMG6HE 后按回车；其他输入将取消："
if [[ "$CONFIRM" != "54LMG6HE" ]]; then
  echo "已取消，没有写入固件。"
  read "REPLY?按回车关闭窗口。"
  exit 0
fi

echo ""
"$PYTHON_BIN" -u "$TOOL_DIR/g6he_mac_tool.py" upgrade "$FIRMWARE_PATH" --confirm 54LMG6HE
RESULT=$?
echo ""
if [[ $RESULT -eq 0 ]]; then
  echo "处理完成。请查看上方版本验证结果。"
else
  echo "升级未完成。请保留上方错误信息。"
fi
read "REPLY?按回车关闭窗口。"
exit $RESULT
