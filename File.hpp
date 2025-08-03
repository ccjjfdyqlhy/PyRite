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
            throw std::runtime_error("�޷����ļ���" + filename);
        is_open = true;
    }

    void write(const std::string& content, const std::vector<long long>& cursor_pos) {
        if (!is_open)
            throw std::runtime_error("�ļ��ѹر�");

        // ���cursor_pos���Ƿ��и�����
        for (const auto& pos : cursor_pos) {
            if (pos < 0)
                throw std::runtime_error("���λ�ò���Ϊ����");
        }

        if (cursor_pos.size() == 2) {
            // ���ù��λ��
            file.seekp(cursor_pos[0], std::ios::beg);
            // д������
            file.write(content.c_str(), content.length());
        } else {
            file.seekp(0, std::ios::end);
            file.write(content.c_str(), content.length());
        }
        
        if (file.fail())
            throw std::runtime_error("д���ļ�ʧ��");
    }

    std::string read(long long line = -1) {
        if (!is_open)
            throw std::runtime_error("�ļ��ѹر�");

        std::string content;
        file.seekg(0, std::ios::beg);

        if (line < 0) {
            // ��ȡ�����ļ�
            std::string temp;
            while (std::getline(file, temp)) {
                content += temp + "\n";
            }
        } else {
            // ��ȡָ����
            std::string temp;
            for (long long i = 0; i <= line; ++i) {
                if (!std::getline(file, temp))
                    throw std::runtime_error("�ļ���������");
                if (i == line)
                    content = temp;
            }
        }

        if (file.fail() && !file.eof())
            throw std::runtime_error("��ȡ�ļ�ʧ��");

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
