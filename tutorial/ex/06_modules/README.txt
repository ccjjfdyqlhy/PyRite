模块路径是相对【当前工作目录】解析的，不是相对脚本自身。
所以本目录的示例要这样运行：

    cd tutorial/ex/06_modules && ../../../../PyRite main.pr

或者在仓库根目录用绝对/相对 CWD 路径：
    cd tutorial/ex/06_modules && /path/to/PyRite main.pr
