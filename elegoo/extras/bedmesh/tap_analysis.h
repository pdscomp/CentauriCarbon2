/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-02-21 12:14:07
 * @LastEditors  : Coconut
 * @LastEditTime : 2025-02-21 12:14:07
 * @Description  : Load cell probe support
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include <memory>
#include <array>
#include <vector>
#include <string>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include "json.h"

enum
{
    TAP_ANALYSIS_ERROR_NONE = 0,
    TAP_ANALYSIS_ERROR_MOVE_NOT_FOUND,
    TAP_ANALYSIS_ERROR_COASTING_MOVE_ACCELERATION,
    TAP_ANALYSIS_ERROR_TOO_FEW_PROBING_MOVES,
    TAP_ANALYSIS_ERROR_TOO_MANY_PROBING_MOVES,
    TAP_ANALYSIS_ERROR_TAP_CHRONOLOGY,
    TAP_ANALYSIS_ERROR_TAP_ELBOW_ROTATION,
    TAP_ANALYSIS_ERROR_TAP_SEGMENT_TOO_SHORT,
    TAP_ANALYSIS_ERROR_TAP_BREAK_CONTACT_TOO_EARLY,
    TAP_ANALYSIS_ERROR_TAP_BREAK_CONTACT_TOO_LATE,
    TAP_ANALYSIS_ERROR_TAP_PULLBACK_TOO_SHORT,
};

#define Q1_INT_BITS 1
#define Q1_FRAC_BITS (32 - (1 + Q1_INT_BITS))
static inline int32_t as_fixed_q1(double val)
{
    double fixed_val = int(val * std::pow(2, Q1_FRAC_BITS));
    return fixed_val;
}

#define Q16_INT_BITS 16
#define Q16_FRAC_BITS (32 - (1 + Q16_INT_BITS))
static inline int32_t as_fixed_q16(double val)
{
    double fixed_val = int(val * std::pow(2, Q16_FRAC_BITS));
    return fixed_val;
}

class ValidationError : public std::exception
{
public:
    explicit ValidationError(const std::string &message) : message(message + "\n") {}
    const char *what() const noexcept { return message.c_str(); }

private:
    std::string message;
};

#define TAP_ANALYSIS_MAX_SECTIONS 6
#define TAP_ANALYSIS_SECTION_WIDTH 6

class DigitalFilter
{
public:
    DigitalFilter(const std::vector<std::vector<double>> &filter_sections, const std::vector<std::vector<double>> &initial_state);
    ~DigitalFilter() = default;
    std::vector<std::vector<int32_t>> _convert_filters_to_q1();
    std::vector<double> sosfilt(const std::vector<double> &data, std::vector<std::vector<double>> filter_state);
    std::vector<double> sosfiltfilt(const std::vector<double> &data);
    std::vector<std::vector<double>> filter_sections;
    std::vector<std::vector<double>> initial_state;
};

struct TrapezoidalMove
{
    double print_time, move_t;
    double start_v, accel;
    double start_x, start_y, start_z;
    double x_r, y_r, z_r;
    json to_dict() const
    {
        json p;
        p["print_time"] = print_time;
        p["move_t"] = move_t;
        p["start_v"] = start_v;
        p["accel"] = accel;
        p["start_x"] = start_x;
        p["start_y"] = start_y;
        p["start_z"] = start_z;
        p["x_r"] = x_r;
        p["y_r"] = y_r;
        p["z_r"] = z_r;
        return p;
    }
};

struct ForcePoint
{
public:
    ForcePoint(double time, double force) : time(time), force(force) {};
    ~ForcePoint() = default;
    double time;
    double force;
    json to_dict() const
    {
        json p;
        p["time"] = time;
        p["force"] = force;
        return p;
    }
};

struct ForceLine
{
public:
    ForceLine(double slope, double intercept) : slope(slope), intercept(intercept)
    {
    }
    ~ForceLine() = default;
    double slope;
    double intercept;

    // 求两直线夹角，顺时针方向>0
    double angle(const ForceLine &line, double time_scale = 0.001)
    {
        long double radians = std::atan2(slope * time_scale, 1) - std::atan2(line.slope * time_scale, 1);
        return radians / M_PI * 180.;
    }

    // 已知X，查找Y
    double find_force(double time) const
    {
        return slope * time + intercept;
    }

    // 已知Y，查找X
    double find_time(double force) const
    {
        return (force - intercept) / slope;
    }

    // 求两直线交点
    ForcePoint intersection(const ForceLine &line)
    {
        // k1x + b1 = k2x + b2 ---> x=(-b1 + b2)/(k1 - k2)
        long double numerator = -intercept + line.intercept;
        long double denominator = slope - line.slope;
        // 斜率相等平行
        if (denominator == 0.)
            return ForcePoint(0., 0.);
        long double intersection_time = numerator / denominator;
        long double intersection_force = find_force(intersection_time);
        return ForcePoint(intersection_time, intersection_force);
    }

    json to_dict() const
    {
        json l;
        l["slope"] = slope;
        l["intercept"] = intercept;
        return l;
    }
};

struct PrefixStats
{
    std::vector<double> sum_x, sum_y, sum_x2, sum_xy, sum_y2;
    std::vector<int> count;
    double mean_x = 0.0;
    double mean_y = 0.0;

    void compute(const std::vector<double> &x, const std::vector<double> &y)
    {

        size_t n = x.size();
        sum_x.resize(n + 1, 0.0);
        sum_y.resize(n + 1, 0.0);
        sum_x2.resize(n + 1, 0.0);
        sum_xy.resize(n + 1, 0.0);
        sum_y2.resize(n + 1, 0.0);
        count.resize(n + 1, 0);

        // 全局均值，用于中心化
        double total_x = 0.0, total_y = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            total_x += x[i];
            total_y += y[i];
        }
        mean_x = total_x / n;
        mean_y = total_y / n;

        for (size_t i = 0; i < n; ++i)
        {
            double cx = x[i] - mean_x;
            double cy = y[i] - mean_y;
            sum_x[i + 1] = sum_x[i] + cx;
            sum_y[i + 1] = sum_y[i] + cy;
            sum_x2[i + 1] = sum_x2[i] + cx * cx;
            sum_xy[i + 1] = sum_xy[i] + cx * cy;
            sum_y2[i + 1] = sum_y2[i] + cy * cy;
            count[i + 1] = count[i] + 1;
        }
    }

    double error(int start, int end) const
    {
        if (end <= start || end > sum_x.size() - 1)
            return 0.0;

        int N = end - start;
        if (N < 2)
            return 0.0;

        double sx = sum_x[end] - sum_x[start];
        double sy = sum_y[end] - sum_y[start];
        double sx2 = sum_x2[end] - sum_x2[start];
        double sxy = sum_xy[end] - sum_xy[start];
        double sy2 = sum_y2[end] - sum_y2[start];

        // 使用中心化误差计算公式
        double x_mean = sx / N;
        double y_mean = sy / N;

        double cov_xy = sxy - sx * y_mean;
        double var_x = sx2 - sx * x_mean;

        if (var_x <= 1e-12) // 避免除0
            return 0.0;

        double m = cov_xy / var_x;
        double b = y_mean - m * x_mean;

        // RSS: Sum of squared residuals
        double rss = sy2 - 2 * m * sxy - 2 * b * sy + m * m * sx2 + 2 * m * b * sx + N * b * b;
        return std::max(0.0, rss); // 数值误差可能导致负值，保护
    }
};

class ForceGraph
{
public:
    // PrefixStats stats;

    ForceGraph(const std::vector<double> &time, const std::vector<double> &force);
    std::pair<double, double> _lstsq_line(const std::vector<double> &x, const std::vector<double> &y, int start = 0, int end = 0);
    double _lstsq_error(const std::vector<double> &x, const std::vector<double> &y, int start = 0, int end = 0);
    double _two_lines_error(const std::vector<double> &time, const std::vector<double> &force, int i);
    double _two_lines_best_fit(const std::vector<double> &time, const std::vector<double> &force);
    double _two_lines_best_fit2(const std::vector<double> &time, const std::vector<double> &force);

    std::pair<std::vector<double>, std::vector<double>> _slice_nd(int start_idx, int end_idx, int discard_left = 0, int discard_right = 0);
    int find_elbow(int start_idx, int end_idx);
    int index_near(double instant);
    ForceLine _points_to_line(const ForcePoint &a, const ForcePoint &b);
    ForceLine line(int start_idx, int end_idx, int discard_left = 0, int discard_right = 0);
    double noise_std(int start_idx, int end_idx, const ForceLine &line);
    bool is_clear_signal(int start_idx, int end_idx, const ForceLine &line, int reference_idx,
                         int force_idx);
    int _split_by_force(int start_idx, int end_idx);
    std::pair<std::vector<ForcePoint>, std::vector<ForceLine>> tap_decompose(double homing_start_time, double homing_end_time, double pullback_start_time,
                                                                             double pullback_cruise_time, double pullback_cruise_duration,
                                                                             double homing_speed, double pullback_speed,
                                                                             int discard = 0);

    std::vector<double> time;
    std::vector<double> force;
    double contact_time;
};

class TapAnalysis
{
public:
    const static int DEFAULT_DISCARD_POINTS = 3;
    const static int PROBE_START = -6;
    const static int PROBE_CRUISE = -5;
    const static int PROBE_HALT = -4;
    const static int PULLBACK_START = -3;
    const static int PULLBACK_CRUISE = -2;
    const static int PULLBACK_END = -1;

    TapAnalysis(const std::vector<double> &time, const std::vector<double> &raw_force, const std::vector<TrapezoidalMove> &moves, std::shared_ptr<DigitalFilter> tap_filter,
                const std::string &path = "", int discard = TapAnalysis::DEFAULT_DISCARD_POINTS);
    ~TapAnalysis() = default;
    // std::vector<std::vector<double>> _extract_pos_history();
    std::vector<double> get_toolhead_position(double print_time);
    double _recalculate_homing_end();
    int analyze();
    int _try_analysis();
    int validate_order();
    int validate_elbow_rotation();
    int validate_elbow_clearance();
    int validate_break_contact_time(double break_contact_time);
    std::vector<double> calculate_angles();
    json to_dict();
    json get_json();

    // Input
    std::vector<double> time;
    std::vector<double> raw_force;
    std::vector<double> force;
    std::vector<TrapezoidalMove> moves;
    int discard;
    bool is_valid;
    std::shared_ptr<ForceGraph> force_graph;
    double tap_time;

    std::vector<double> tap_pos;
    std::vector<ForcePoint> tap_points;
    std::vector<ForceLine> tap_lines;
    std::vector<double> tap_angles;
    std::vector<std::vector<double>> tap_r_squared;
    std::vector<int> r_squared_widths;
    double home_end_time;
    double pullback_start_time;
    double pullback_end_time;
    double pullback_cruise_time;
    double pullback_duration;
    std::string path;
};
