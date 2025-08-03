#pragma once
#include <fstream>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>

class FileObject {
private:
    std::fstream file;
    std::string filename;
    std::string encoding;
    bool is_open;

public:
    FileObject(const std::string& fname, const std::string& enc = "utf-8-sig")
        : filename(fname), encoding(enc), is_open(false) {
        file.open(filename, std::ios::in | std::ios::out);
        if (!file.is_open())
            throw std::runtime_error("无法打开文件：" + filename);
        is_open = true;
    }

    void write(const std::string& content, const std::vector<long long>& cursor_pos) {
        if (!is_open)
            throw std::runtime_error("文件已关闭");

        // 检查cursor_pos中是否有浮点数
        for (const auto& pos : cursor_pos) {
            if (pos < 0)
                throw std::runtime_error("光标位置不能为负数");
        }

        if (cursor_pos.size() == 2) {
            // 设置光标位置
            file.seekp(cursor_pos[0], std::ios::beg);
            // 写入内容
            file.write(content.c_str(), content.length());
        } else {
            file.seekp(0, std::ios::end);
            file.write(content.c_str(), content.length());
        }
        
        if (file.fail())
            throw std::runtime_error("写入文件失败");
    }

    std::string read(long long line = -1) {
        if (!is_open)
            throw std::runtime_error("文件已关闭");

        std::string content;
        file.seekg(0, std::ios::beg);

        if (line < 0) {
            // 读取整个文件
            std::string temp;
            while (std::getline(file, temp)) {
                content += temp + "\n";
            }
        } else {
            // 读取指定行
            std::string temp;
            for (long long i = 0; i <= line; ++i) {
                if (!std::getline(file, temp))
                    throw std::runtime_error("文件行数不足");
                if (i == line)
                    content = temp;
            }
        }

        if (file.fail() && !file.eof())
            throw std::runtime_error("读取文件失败");

        return content;
    }

    void close() {
        if (is_open) {
            file.close();
            is_open = false;
        }
    }

    ~FileObject() {
        close();
    }
};
