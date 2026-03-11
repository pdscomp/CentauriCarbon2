/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:45
 * @LastEditors  : Ben
 * @LastEditTime : 2024-11-28 21:19:46
 * @Description  : Simple math helper functions
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <array>
#include <memory>
#include <map>
#include <functional>


class Printer;

using Vector3 = std::array<double, 3>;

// 计算两个向量的叉乘
Vector3 matrix_cross(const Vector3& m1, const Vector3& m2);

// 计算两个向量的点积
double matrix_dot(const Vector3& m1, const Vector3& m2);

// 计算向量的模平方
double matrix_magsq(const Vector3& m1);

// 向量相加
Vector3 matrix_add(const Vector3& m1, const Vector3& m2);

// 向量相减
Vector3 matrix_sub(const Vector3& m1, const Vector3& m2);

// 向量与标量相乘
Vector3 matrix_mul(const Vector3& m1, double s);

Vector3 trilateration(const std::array<Vector3, 3>& sphere_coords, const std::array<double, 3>& radius2);

std::map<std::string, double> coordinate_descent(
    const std::vector<std::string>& adj_params,
    std::map<std::string, double> params,
    std::function<double(const std::map<std::string, double>&)> error_func
);

std::map<std::string, double> background_coordinate_descent(
    std::shared_ptr<Printer> printer, 
    const std::vector<std::string>& adj_params,
    const std::map<std::string, double>& params,
    std::function<double(const std::map<std::string, double>&)> error_func);