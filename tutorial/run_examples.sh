#!/usr/bin/env bash
# 依次运行 tutorial/ex 下的所有示例脚本
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PYRITE="$ROOT/PyRite"

fail=0
for f in "$ROOT"/tutorial/ex/*.pr; do
  echo "=================================================="
  echo "  $(basename "$f")"
  echo "=================================================="
  if ! "$PYRITE" "$f"; then
    fail=1
  fi
done

echo
echo "=================================================="
echo "  06_modules/main.pr （模块路径相对 CWD，需进入目录运行）"
echo "=================================================="
( cd "$ROOT/tutorial/ex/06_modules" && "$PYRITE" main.pr ) || fail=1

echo
if [ "$fail" -eq 0 ]; then
  echo "全部示例运行通过。"
else
  echo "有示例运行失败。"
fi
exit "$fail"
