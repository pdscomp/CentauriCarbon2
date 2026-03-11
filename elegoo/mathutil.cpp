/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:43
 * @LastEditors  : Ben
 * @LastEditTime : 2024-11-28 21:19:51
 * @Description  : Simple math helper functions
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "mathutil.h"
#include "printer.h"
#include <future>
#include <numeric>

// 计算两个向量的叉乘
Vector3 matrix_cross(const Vector3& m1, const Vector3& m2) 
{
    return {
        m1[1] * m2[2] - m1[2] * m2[1],
        m1[2] * m2[0] - m1[0] * m2[2],
        m1[0] * m2[1] - m1[1] * m2[0]
    };
}

// 计算两个向量的点积
double matrix_dot(const Vector3& m1, const Vector3& m2) 
{
    return m1[0] * m2[0] + m1[1] * m2[1] + m1[2] * m2[2];
}

// 计算向量的模平方
double matrix_magsq(const Vector3& m1) 
{
    return m1[0] * m1[0] + m1[1] * m1[1] + m1[2] * m1[2];
}

// 向量相加
Vector3 matrix_add(const Vector3& m1, const Vector3& m2) 
{
    return {
        m1[0] + m2[0],
        m1[1] + m2[1],
        m1[2] + m2[2]
    };
}

// 向量相减
Vector3 matrix_sub(const Vector3& m1, const Vector3& m2) 
{
    return {
        m1[0] - m2[0],
        m1[1] - m2[1],
        m1[2] - m2[2]
    };
}

// 向量与标量相乘
Vector3 matrix_mul(const Vector3& m1, double s) 
{
    return {
        m1[0] * s,
        m1[1] * s,
        m1[2] * s
    };
}

Vector3 trilateration(const std::array<Vector3, 3>& sphere_coords, const std::array<double, 3>& radius2) 
{
    const Vector3& sphere_coord1 = sphere_coords[0];
    const Vector3& sphere_coord2 = sphere_coords[1];
    const Vector3& sphere_coord3 = sphere_coords[2];

    // 计算向量差
    Vector3 s21 = matrix_sub(sphere_coord2, sphere_coord1);
    Vector3 s31 = matrix_sub(sphere_coord3, sphere_coord1);

    // 计算d，单位向量ex
    double d = std::sqrt(matrix_magsq(s21));
    if (d == 0.0) throw std::runtime_error("Division by zero error in trilateration");

    Vector3 ex = matrix_mul(s21, 1.0 / d);
    double i = matrix_dot(ex, s31);

    // 计算ey单位向量
    Vector3 vect_ey = matrix_sub(s31, matrix_mul(ex, i));
    double ey_magsq = matrix_magsq(vect_ey);
    if (ey_magsq == 0.0) throw std::runtime_error("Division by zero error in trilateration");

    Vector3 ey = matrix_mul(vect_ey, 1.0 / std::sqrt(ey_magsq));

    // 计算ez单位向量
    Vector3 ez = matrix_cross(ex, ey);
    double j = matrix_dot(ey, s31);

    // 计算x, y, z
    double x = (radius2[0] - radius2[1] + d * d) / (2.0 * d);
    double y = (radius2[0] - radius2[2] - x * x + (x - i) * (x - i) + j * j) / (2.0 * j);
    double z_sq = radius2[0] - x * x - y * y;

    if (z_sq < 0.0) throw std::runtime_error("No solution for z in trilateration");

    double z = -std::sqrt(z_sq);

    // 计算最终结果
    Vector3 ex_x = matrix_mul(ex, x);
    Vector3 ey_y = matrix_mul(ey, y);
    Vector3 ez_z = matrix_mul(ez, z);

    return matrix_add(sphere_coord1, matrix_add(ex_x, matrix_add(ey_y, ez_z)));
}

std::map<std::string, double> coordinate_descent(
    const std::vector<std::string>& adj_params,
    std::map<std::string, double> params,
    std::function<double(const std::map<std::string, double>&)> error_func)
{
    std::map<std::string, double> dp;
    for (const auto& param_name : adj_params) 
    {
        dp[param_name] = 1.0;
    }

    double best_err = error_func(params);
    std::cout << "Coordinate descent initial error: " << best_err << std::endl;

    double threshold = 0.00001;
    int rounds = 0;

    // 主循环，直到步长总和小于阈值或达到最大迭代次数
    while (std::accumulate(dp.begin(), dp.end(), 0.0, 
                           [](double sum, const std::pair<std::string, double>& p) {
                               return sum + p.second;
                           }) > threshold && rounds < 10000) 
    {
        ++rounds;
        for (const auto& param_name : adj_params) {
            double orig = params[param_name];

            // 尝试增加 dp[param_name] 步长
            params[param_name] = orig + dp[param_name];
            double err = error_func(params);
            if (err < best_err) {
                // 有所改进，更新误差和步长
                best_err = err;
                dp[param_name] *= 1.1;  // 增加步长
                continue;
            }

            // 尝试减小 dp[param_name] 步长
            params[param_name] = orig - dp[param_name];
            err = error_func(params);
            if (err < best_err) {
                // 有所改进，更新误差和步长
                best_err = err;
                dp[param_name] *= 1.1;  // 增加步长
                continue;
            }

            // 没有改进，恢复原值并减小步长
            params[param_name] = orig;
            dp[param_name] *= 0.9;  // 减少步长
        }
    }

    std::cout << "Coordinate descent best error: " << best_err << " rounds: " << rounds << std::endl;

    return params;
}

std::map<std::string, double> background_coordinate_descent(
    std::shared_ptr<Printer> printer, 
    const std::vector<std::string>& adj_params,
    const std::map<std::string, double>& params,
    std::function<double(const std::map<std::string, double>&)> error_func)
{
    std::promise<std::pair<bool, std::map<std::string, double>>> promise;
    std::future<std::pair<bool, std::map<std::string, double>>> future = promise.get_future();

    // 包装坐标下降算法
    auto wrapper = [&promise, &adj_params, &params, &error_func]() {
        try {
            std::map<std::string, double> res = coordinate_descent(adj_params, params, error_func);
            promise.set_value(std::make_pair(false, res));  // 正常结果
        } catch (const std::exception& e) {
            promise.set_value(std::make_pair(true, std::map<std::string, double>()));  // 错误
        }
    };

    // 创建后台线程
    std::thread calc_thread(wrapper);

    // 获取打印机的 reactor 和 gcode 对象
    std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
    std::shared_ptr<GCodeDispatch> gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));

    double eventtime = get_monotonic();
    double last_report_time = eventtime;

    // 等待后台线程完成，同时提供状态反馈
    while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) 
    {
        if (eventtime > last_report_time + 5.0) 
        {
            last_report_time = eventtime;
            gcode->respond_info("Working on calibration...", false);
        }
        eventtime = reactor->pause(0.1);
    }

    // 获取结果
    auto result = future.get();

    // 检查是否有错误
    if (result.first) {
        throw std::runtime_error("Error in coordinate descent");
    }

    // 等待线程结束
    calc_thread.join();

    return result.second;
}