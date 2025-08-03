#pragma once
#include <vector>
#include <memory>
#include <cmath>
#include "BigNumber.hpp"

class TenseValue {
private:
    std::vector<std::vector<long double>> values;
    std::vector<std::vector<bool>> signs;  // true ��ʾ����
    size_t rows, cols;

public:
    TenseValue(const std::vector<std::vector<long double>>& v, 
               const std::vector<std::vector<bool>>& s) 
        : values(v), signs(s), rows(v.size()), cols(v[0].size()) {}

    // ���б�����������
    static std::shared_ptr<TenseValue> fromListLiteral(
        const std::vector<std::vector<std::shared_ptr<BigNumber>>>& list) {
        if (list.empty()) throw std::runtime_error("�޷��ӿ��б�������");
        
        size_t rows = list.size();
        size_t cols = list[0].size();
        
        std::vector<std::vector<long double>> values(rows, std::vector<long double>(cols));
        std::vector<std::vector<bool>> signs(rows, std::vector<bool>(cols));
        
        for (size_t i = 0; i < rows; i++) {
            if (list[i].size() != cols) 
                throw std::runtime_error("�����ÿһ�б�������ͬ��������");
                
            for (size_t j = 0; j < cols; j++) {
                const auto& num = list[i][j];
                signs[i][j] = num->isNegative();
                values[i][j] = std::abs(num->toDouble());
            }
        }
        
        return std::make_shared<TenseValue>(values, signs);
    }

    // ת��Ϊ�б�
    std::vector<std::vector<std::shared_ptr<BigNumber>>> toList() const {
        std::vector<std::vector<std::shared_ptr<BigNumber>>> result;
        for (size_t i = 0; i < rows; i++) {
            std::vector<std::shared_ptr<BigNumber>> row;
            for (size_t j = 0; j < cols; j++) {
                long double val = values[i][j];
                if (signs[i][j]) val = -val;
                if (std::floor(val) == val) {
                    // ����ֵ
                    row.push_back(std::make_shared<BigNumber>((long long)val));
                } else {
                    // ����ֵ
                    row.push_back(std::make_shared<BigNumber>(std::to_string(val)));
                }
            }
            result.push_back(row);
        }
        return result;
    }

    // ����ӷ�
    std::shared_ptr<TenseValue> add(const TenseValue& other) const {
        if (rows != other.rows || cols != other.cols)
            throw std::runtime_error("����ά�Ȳ�ƥ�䣬�޷���ӡ�");
            
        std::vector<std::vector<long double>> result_values(rows, std::vector<long double>(cols));
        std::vector<std::vector<bool>> result_signs(rows, std::vector<bool>(cols));
        
        for (size_t i = 0; i < rows; i++) {
            for (size_t j = 0; j < cols; j++) {
                long double v1 = values[i][j] * (signs[i][j] ? -1 : 1);
                long double v2 = other.values[i][j] * (other.signs[i][j] ? -1 : 1);
                long double sum = v1 + v2;
                result_values[i][j] = std::abs(sum);
                result_signs[i][j] = sum < 0;
            }
        }
        
        return std::make_shared<TenseValue>(result_values, result_signs);
    }

    // ����˷�
    std::shared_ptr<TenseValue> multiply(const TenseValue& other) const {
        if (cols != other.rows)
            throw std::runtime_error("����ά�Ȳ�ƥ�䣬�޷���ˡ�");
            
        std::vector<std::vector<long double>> result_values(rows, std::vector<long double>(other.cols));
        std::vector<std::vector<bool>> result_signs(rows, std::vector<bool>(other.cols));
        
        for (size_t i = 0; i < rows; i++) {
            for (size_t j = 0; j < other.cols; j++) {
                long double sum = 0;
                bool sign = false;
                for (size_t k = 0; k < cols; k++) {
                    long double v1 = values[i][k];
                    long double v2 = other.values[k][j];
                    bool s = signs[i][k] ^ other.signs[k][j];
                    sum += (s ? -1 : 1) * v1 * v2;
                }
                result_values[i][j] = std::abs(sum);
                result_signs[i][j] = sum < 0;
            }
        }
        
        return std::make_shared<TenseValue>(result_values, result_signs);
    }

    // �������棨ʹ�ø�˹-Լ����Ԫ����
    std::shared_ptr<TenseValue> inverse() const {
        if (rows != cols)
            throw std::runtime_error("ֻ�з���������档");
            
        size_t n = rows;
        std::vector<std::vector<long double>> augmented(n, std::vector<long double>(2 * n, 0.0));
        std::vector<std::vector<bool>> aug_signs(n, std::vector<bool>(2 * n, false));
        
        // �����������
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                augmented[i][j] = values[i][j];
                aug_signs[i][j] = signs[i][j];
            }
            augmented[i][n + i] = 1.0;  // ��λ���󲿷�
        }
        
        // ��˹-Լ����Ԫ
        for (size_t i = 0; i < n; i++) {
            long double pivot = augmented[i][i];
            if (std::abs(pivot) < 1e-10)
                throw std::runtime_error("���󲻿��档");
                
            // ��һ����ǰ��
            for (size_t j = 0; j < 2 * n; j++) {
                augmented[i][j] /= pivot;
            }
            
            // ��Ԫ
            for (size_t k = 0; k < n; k++) {
                if (k != i) {
                    long double factor = augmented[k][i];
                    for (size_t j = 0; j < 2 * n; j++) {
                        augmented[k][j] -= factor * augmented[i][j];
                    }
                }
            }
        }
        
        // ��ȡ�������
        std::vector<std::vector<long double>> result_values(n, std::vector<long double>(n));
        std::vector<std::vector<bool>> result_signs(n, std::vector<bool>(n));
        
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                result_values[i][j] = std::abs(augmented[i][j + n]);
                result_signs[i][j] = augmented[i][j + n] < 0;
            }
        }
        
        return std::make_shared<TenseValue>(result_values, result_signs);
    }
};
