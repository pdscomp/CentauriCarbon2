/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-12 14:30:43
 * @Description  : resonance_tester is a functional module in the Elegoo
 * firmware used to detect and analyze resonance phenomena in the printer at
 * different frequencies. Resonance is a phenomenon where mechanical systems
 * experience amplified vibrations at specific frequencies, which can lead to
 * decreased print quality, surface ripples, or ringing issues in 3D printers.
 * By using resonance_tester, users can identify these resonance frequencies
 * and take measures (such as adjusting input shaper parameters) to mitigate
 * or eliminate these problems.
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "resonance_tester.h"
#include "printer.h"
#include "input_shaper.h"
#include "shaper_calibrate.h"
#include "extras_factory.h"
#include <cmath>
#include <ctime>
#include <chrono>
#include <iomanip>
#include "json.h"

namespace elegoo {
namespace extras {

        // #undef SPDLOG_DEBUG
        // #define SPDLOG_DEBUG SPDLOG_INFO

TestAxis::TestAxis(const std::string& axis,
    const std::vector<double>& vd)
{
    if (axis.empty())
    {
        if (vd.size() == 2)
        {
            name = "axis=" + std::to_string(vd[0]) + "," + std::to_string(vd[1]);
        }
        else
        {
            name = "axis=0.000,0.000";
        }
    }
    else
    {
        name = axis;
    }

    if (vd.empty())
    {
        if (axis == "x")
        {
            vib_dir = {1.0, 0.0};
        }
        else
        {
            vib_dir = {0.0, 1.0};
        }
    }
    else
    {
        double magnitude = 0.0;
        for (double d : vd)
        {
            magnitude += d * d;
        }
        magnitude = std::sqrt(magnitude);

        for (double d : vd)
        {
            vib_dir.push_back(d / magnitude);
        }
    }
}

TestAxis::~TestAxis()
{

}


bool TestAxis::matches(const std::string& chip_axis)
{
    if (vib_dir.size() > 0 && vib_dir[0] != 0.0 &&
        chip_axis.find('x') != std::string::npos)
    {
        return true;
    }

    if (vib_dir.size() > 1 && vib_dir[1] != 0.0
        && chip_axis.find('y') != std::string::npos)
    {
        return true;
    }

    return false;
}

std::string TestAxis::get_name()
{
    return name;
}

std::pair<double, double> TestAxis::get_point(double l)
{
    return std::make_pair(vib_dir[0] * l, vib_dir[1] * l);
}


std::shared_ptr<TestAxis> parse_axis(std::shared_ptr<GCodeCommand> gcmd,
    std::string raw_axis)
{
    if (raw_axis.empty()) {
        return nullptr;
    }

    std::string axis = raw_axis;
    for (auto& ch : axis) {
        ch = tolower(ch);
    }

    if (axis == "x" || axis == "y") {
        return std::make_shared<TestAxis>(axis);
    }

    std::vector<std::string> dirs = elegoo::common::split(axis, ",");
    if(dirs.size()!=2) {
        throw elegoo::common::CommandError("Invalid format of axis: " + raw_axis);
    }

    double dir_x;
    double dir_y;
    try {
        dir_x = std::stod(elegoo::common::strip(dirs[0]));
        dir_y = std::stod(elegoo::common::strip(dirs[1]));
    } catch (const std::invalid_argument&) {
        throw elegoo::common::CommandError("Unable to parse axis direction '" + raw_axis + "'");
    }

    std::vector<double> vd;
    vd.push_back(dir_x);
    vd.push_back(dir_y);
    return std::make_shared<TestAxis>("", vd);
}


VibrationPulseTest::VibrationPulseTest(
    std::shared_ptr<ConfigWrapper> config)
{
    this->printer = config->get_printer();
    this->gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    this->min_freq = config->getdouble("min_freq", 5., 1.);
    this->max_freq = config->getdouble("max_freq", 10000. / 75., min_freq, 300.);
    this->accel_per_hz = config->getdouble("accel_per_hz", 75., DOUBLE_NONE, DOUBLE_NONE, 0.);
    this->hz_per_sec = config->getdouble("hz_per_sec", 1., 0.1, 2.);
    this->probe_points = config->getdoublevectors("probe_points", {}, {"\n",","});
}

VibrationPulseTest::~VibrationPulseTest()
{
    SPDLOG_DEBUG("~VibrationPulseTest");
}

std::vector<std::vector<double>> VibrationPulseTest::get_start_test_points()
{
    return this->probe_points;
}

void VibrationPulseTest::prepare_test(std::shared_ptr<GCodeCommand> gcmd)
{
    this->freq_start = gcmd->get_double("FREQ_START", min_freq, 1.);
    this->freq_end = gcmd->get_double("FREQ_END", max_freq,freq_start, 300.);
    this->hz_per_sec = gcmd->get_double("HZ_PER_SEC", hz_per_sec, DOUBLE_NONE,2., 0.);
}

void VibrationPulseTest::run_test(std::shared_ptr<TestAxis> axis,
    std::shared_ptr<GCodeCommand> gcmd)
{
    SPDLOG_DEBUG("{} #1 ",__func__);
    std::shared_ptr<ToolHead> toolhead =
        any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));

    std::vector<double> pos = toolhead->get_position();
    double sign = 1.0;
    double freq = this->freq_start;

    SPDLOG_DEBUG("{} #1 ",__func__);
    double systime = get_monotonic();
    auto toolhead_info = toolhead->get_status(systime);
    double old_max_accel = toolhead_info["max_accel"];
    double old_minimum_cruise_ratio = toolhead_info["minimum_cruise_ratio"];
    double max_accel = this->freq_end * this->accel_per_hz;

    SPDLOG_DEBUG("{} #1 ",__func__);
    std::string gcode_command = "SET_VELOCITY_LIMIT ACCEL=" +
        std::to_string(max_accel) + " MINIMUM_CRUISE_RATIO=0";
    gcode->run_script_from_command(gcode_command);

    SPDLOG_DEBUG("{} #1 ",__func__);
    std::shared_ptr<InputShaper> input_shaper =
        any_cast<std::shared_ptr<InputShaper>>(printer->lookup_object("input_shaper", nullptr));

    if (input_shaper && !gcmd->get_int("INPUT_SHAPING", 0))
    {
        SPDLOG_DEBUG("{} #1 ",__func__);
        input_shaper->disable_shaping();
        gcmd->respond_info("Disabled [input_shaper] for resonance testing", true);
    }
    else
    {
        input_shaper = nullptr;
    }

    gcmd->respond_info("Testing frequency " + std::to_string(freq) + " Hz", true);

    SPDLOG_DEBUG("{} #1 freq:{},freq_end:{}",__func__,freq,freq_end);
    // Main test loop
    json feedback;
    feedback["command"] = "resonance_tester";
    feedback["axis"] = axis->get_name();
    feedback["result"] = "running";
    while (freq <= this->freq_end + 0.000001)
    {
        feedback["freq"] = freq;
        gcmd->respond_feedback(feedback);

        double t_seg = 0.25 / freq;
        int32_t accel = this->accel_per_hz * freq;
        double max_v = accel * t_seg;

        std::map<std::string, std::string> params;
        params["S"] = std::to_string(accel);
        // S只能是整数！
        toolhead->cmd_M204(gcode->create_gcode_command("M204", "M204", params));
        double L = 0.5 * accel * std::pow(t_seg, 2);

        std::pair<double, double> axis_point = axis->get_point(L);

        double nX = pos[0] + sign * axis_point.first;
        double nY = pos[1] + sign * axis_point.second;
        toolhead->move({nX, nY, pos[2], pos[3]}, max_v);
        toolhead->move({pos[0], pos[1], pos[2], pos[3]}, max_v);

        sign = -sign;
        double old_freq = freq;
        freq += 2.0 * t_seg * this->hz_per_sec;

        SPDLOG_DEBUG("{} #1 freq:{},old_freq:{} {} {} {}",__func__,freq,old_freq,t_seg,this->hz_per_sec,sign);
        if (std::floor(freq) > std::floor(old_freq))
        {
            gcmd->respond_info("Testing frequency " + std::to_string(freq) + " Hz", true);
        }
    }

    SPDLOG_DEBUG("{} #1 ",__func__);
    gcode_command = "SET_VELOCITY_LIMIT ACCEL=" + std::to_string(old_max_accel) + " MINIMUM_CRUISE_RATIO=" + std::to_string(old_minimum_cruise_ratio);
    gcode->run_script_from_command(gcode_command);

    if (input_shaper)
    {
        SPDLOG_DEBUG("{} #1 ",__func__);
        input_shaper->enable_shaping();
        gcmd->respond_info("Re-enabled [input_shaper]", true);
    }
    SPDLOG_DEBUG("{} #1 ",__func__);
}

double VibrationPulseTest::get_max_freq()
{
    return freq_end;
}

ResonanceTester::ResonanceTester(std::shared_ptr<ConfigWrapper> config)
{
    SPDLOG_DEBUG("ResonanceTester init!");
    printer = config->get_printer();
    move_speed = config->getdouble("move_speed", 50,
        DOUBLE_NONE, DOUBLE_NONE, 0);

    test = std::make_shared<VibrationPulseTest>(config);

    if (config->get("accel_chip_x", "").empty())
    {
        accel_chip_names.push_back({"xy", elegoo::common::strip(config->get("accel_chip"))});
    }
    else
    {
        accel_chip_names.push_back({"x", elegoo::common::strip(config->get("accel_chip_x"))});
        accel_chip_names.push_back({"y", elegoo::common::strip(config->get("accel_chip_y"))});
        if (accel_chip_names[0].second == accel_chip_names[1].second)
        {
            accel_chip_names = {{"xy", accel_chip_names[0].second}};
        }
    }

    this->max_smoothing = config->getdouble("max_smoothing", DOUBLE_NONE, 0.05);
    gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    gcode->register_command("MEASURE_AXES_NOISE",
        [this](std::shared_ptr<GCodeCommand> gcmd){
            cmd_MEASURE_AXES_NOISE(gcmd);
        }, false,
        "Measures noise of all enabled accelerometer chips");

    gcode->register_command("TEST_RESONANCES",
        [this](std::shared_ptr<GCodeCommand> gcmd){
            cmd_TEST_RESONANCES(gcmd);
        }, false,
        "Runs the resonance test for a specifed axis");

    gcode->register_command("SHAPER_CALIBRATE",
        [this](std::shared_ptr<GCodeCommand> gcmd){
            SPDLOG_DEBUG("SHAPER_CALIBRATE #1");
            cmd_SHAPER_CALIBRATE(gcmd);
            SPDLOG_DEBUG("SHAPER_CALIBRATE #2");
        }, false,
        "Simular to TEST_RESONANCES but suggest input shaper config");

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:connect",
        std::function<void()>([this](){
            SPDLOG_DEBUG("ResonanceTester connect~~~~~~~~~~~~~~~~~");
            connect();
            SPDLOG_DEBUG("ResonanceTester connect~~~~~~~~~~~~~~~~~ success!");
        })
    );

    std::shared_ptr<elegoo::extras::PrinterGCodeMacro> gcode_macro = any_cast<std::shared_ptr<elegoo::extras::PrinterGCodeMacro>>(config->get_printer()->load_object(config, "gcode_macro"));
    shaper_calibrate_gcode = gcode_macro->load_template(config, "shaper_calibrate_gcode", "\n");
    
    this->status = json();
    // this->status["method"] = "resonance_tester";
    this->status["command"] = "resonance_tester";
    SPDLOG_DEBUG("ResonanceTester init success!!");
}

ResonanceTester::~ResonanceTester()
{
    SPDLOG_DEBUG("~ResonanceTester");
}


void ResonanceTester::connect()
{
    for (auto chip_info : accel_chip_names)
    {
        std::string chip_axis = chip_info.first;
        std::string chip_name = chip_info.second;
        accel_chips.emplace_back(chip_axis, any_cast<std::shared_ptr<AccelChip>>(printer->lookup_object(chip_name)));
    }
}

void ResonanceTester::cmd_TEST_RESONANCES(std::shared_ptr<GCodeCommand> gcmd)
{
    std::string axis_str = gcmd->get("AXIS");
    for (auto& ch : axis_str) {
        ch = tolower(ch);
    }
    std::shared_ptr<TestAxis> axis = parse_axis(gcmd,axis_str);
    std::string chips_str = gcmd->get("CHIPS", "");
    std::string test_point_str = gcmd->get("POINT", "");
    std::vector<double> test_point;
    if (!test_point_str.empty())
    {
        std::vector<std::string> test_coords;
        test_coords = elegoo::common::split(test_point_str, ",");
        if(test_coords.size()!=3) {
            throw elegoo::common::CommandError("Invalid POINT parameter, must be 'x,y,z'");
        }

        try {
            for (size_t i = 0; i < test_coords.size(); i++) {
                test_point.push_back(std::stod(elegoo::common::strip(test_coords.at(i))));
            }
        }
        catch (const std::invalid_argument&)
        {
            SPDLOG_ERROR("Invalid POINT parameter, must be 'x,y,z' where x, y and z are valid floating point numbers");
            throw elegoo::common::CommandError("Invalid POINT parameter, must be 'x,y,z' where x, y and z are valid floating point numbers");
        }

    }

    std::vector<std::shared_ptr<AccelChip>> accel_chips;
    if(!chips_str.empty())
    {
        accel_chips = parse_chips(chips_str);
    }

    std::string output_str = gcmd->get("OUTPUT","resonances");
    for (auto& ch : output_str) {
        ch = tolower(ch);
    }
    std::vector<std::string> outputs = elegoo::common::split(output_str, ",");
    for (const std::string& output : outputs)
    {
        if (output != "resonances" && output != "raw_data")
        {
            SPDLOG_ERROR("Unsupported output '" + output + "', only 'resonances' and 'raw_data' are supported");
            throw elegoo::common::CommandError("Unsupported output '" + output + "', only 'resonances' and 'raw_data' are supported");
        }
    }

    if(outputs.empty()) {
        SPDLOG_ERROR("No output specified, at least one of 'resonances' or 'raw_data' must be set in OUTPUT parameter");
        throw elegoo::common::CommandError("No output specified, at least one of 'resonances' or 'raw_data' must be set in OUTPUT parameter");
    }

    std::string name_suffix = gcmd->get("NAME", get_current_time_string());
    if (!is_valid_name_suffix(name_suffix))
    {
        SPDLOG_ERROR("Invalid NAME parameter");
        throw elegoo::common::CommandError("Invalid NAME parameter");
    }

    bool csv_output = (std::find(outputs.begin(), outputs.end(), "resonances") != outputs.end());
    bool raw_output = (std::find(outputs.begin(), outputs.end(), "raw_data") != outputs.end());

    std::shared_ptr<ShaperCalibrate> helper;
    if (csv_output) {
        helper = std::make_shared<ShaperCalibrate>(printer);
    }

    std::vector<std::shared_ptr<TestAxis>> axes;
    axes.push_back(axis);
    std::shared_ptr<CalibrationData> data =
        run_test(gcmd, axes, helper, raw_output ? name_suffix : "",
        accel_chips,test_point)[axis];

    if (csv_output) {
        std::string csv_name = save_calibration_data(
            "resonances", name_suffix, helper, axis, data, {}, test_point, get_max_calibration_freq());
        gcmd->respond_info("Resonances data written to " + csv_name + " file", true);
    }
}

void ResonanceTester::cmd_SHAPER_CALIBRATE(std::shared_ptr<GCodeCommand> gcmd)
{
    try
    {
        shaper_calibrate_gcode->run_gcode_from_command();
        //progress = 0.0
        this->status["axis"] = "-";
        this->status["freq"] = 0;
        this->status["result"] = "running";
        gcmd->respond_feedback(this->status);
        SPDLOG_INFO("{} #1 status:{}",__func__,this->status.dump());

        std::string axis = gcmd->get("AXIS", "");
        for (auto& ch : axis) {
            ch = tolower(ch);
        }

        std::vector<std::shared_ptr<TestAxis>> calibrate_axes;
        if (axis.empty())
        {
            calibrate_axes.push_back(std::make_shared<TestAxis>("x"));
            calibrate_axes.push_back(std::make_shared<TestAxis>("y"));
        }
        else if (std::string("xy").find(axis) == std::string::npos)
        {
            SPDLOG_ERROR("Unsupported axis '" + axis + "'");
            throw elegoo::common::CommandError("Unsupported axis '" + axis + "'");
        }
        else
        {
            calibrate_axes.push_back(std::make_shared<TestAxis>(axis));
        }

        std::string chips_str = gcmd->get("CHIPS", "");
        std::vector<std::shared_ptr<AccelChip>> accel_chips;
        if(!chips_str.empty())
        {
            accel_chips = parse_chips(chips_str);
        }

        double max_smoothing = gcmd->get_double("MAX_SMOOTHING", this->max_smoothing, 0.05f);

        std::string name_suffix = gcmd->get("NAME", get_current_time_string());
        if (!is_valid_name_suffix(name_suffix))
        {
            SPDLOG_ERROR("Invalid NAME parameter");
            throw elegoo::common::CommandError("Invalid NAME parameter");
        }

        std::shared_ptr<InputShaper> input_shaper =
            any_cast<std::shared_ptr<InputShaper>>(printer->lookup_object("input_shaper"));

        std::shared_ptr<ShaperCalibrate> helper =
            std::make_shared<ShaperCalibrate>(printer);

        auto calibration_data = run_test(gcmd, calibrate_axes, helper, {}, accel_chips);

        SPDLOG_DEBUG("{} #1",__func__);
        std::shared_ptr<PrinterConfig> configfile =
            any_cast<std::shared_ptr<PrinterConfig>>(printer->lookup_object("configfile"));

        for (auto axis : calibrate_axes)
        {
            std::string axis_name = axis->get_name();
            gcmd->respond_info("Calculating the best input shaper parameters for "
                + axis_name + " axis", true);

            calibration_data.at(axis)->normalize_to_frequencies();
            double systime = get_monotonic();
            std::shared_ptr<ToolHead> toolhead =
                any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            json  toolhead_info = toolhead->get_status(systime);
            double scv = toolhead_info["square_corner_velocity"];
            double max_freq = get_max_calibration_freq();

            auto shaper = helper->find_best_shaper(calibration_data.at(axis),{},{}, scv,{}, max_smoothing, {},max_freq);
            auto best_shaper = shaper.first;
            auto all_shapers = shaper.second;

            gcmd->respond_info("Recommended shaper_type_" + axis_name + " = " + best_shaper.name +
                                ", shaper_freq_" + axis_name + " = " + std::to_string(best_shaper.freq) + " Hz",true);
            if(input_shaper)
            {
                helper->apply_params(input_shaper, axis_name, best_shaper.name, std::to_string(best_shaper.freq));
            }
            helper->save_params(configfile, axis_name, best_shaper.name, std::to_string(best_shaper.freq));
            configfile->cmd_SAVE_CONFIG(gcode->create_gcode_command("SAVE_CONFIG", "SAVE_CONFIG", std::map<std::string, std::string>()));
            // std::string csv_name = save_calibration_data(
            //     "calibration_data", name_suffix, helper, axis,
            //     calibration_data[axis], all_shapers,{}, max_freq);
            // gcmd->respond_info("Shaper calibration data written to " + csv_name + " file", true);
        }

        this->status["result"] = "completed";
        gcmd->respond_feedback(this->status);
        SPDLOG_INFO("{} #1 status:{}",__func__,this->status.dump());
        //
        gcmd->respond_info("The SAVE_CONFIG command will update the printer config file\n"
                            "with these parameters and restart the printer.", true);
    }
    catch (...)
    {
        this->status["result"] = "failed";
        gcmd->respond_feedback(this->status);
        SPDLOG_INFO("{} #1 status:{}",__func__,this->status.dump());
    }
}

void ResonanceTester::cmd_MEASURE_AXES_NOISE(std::shared_ptr<GCodeCommand> gcmd)
{
    double meas_time = gcmd->get_double("MEAS_TIME", 2);

    std::vector<std::pair<std::string, std::shared_ptr<AccelQueryHelper>>> raw_values;
    for (auto chip : accel_chips)
    {
        raw_values.push_back({chip.first, chip.second->start_internal_client()});
    }

    std::shared_ptr<ToolHead> toolhead =
        any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
    toolhead->dwell(meas_time);

    for (auto val : raw_values)
    {
        val.second->finish_measurements();
    }

    std::shared_ptr<ShaperCalibrate> helper =
        std::make_shared<ShaperCalibrate>(printer);
    for (auto val : raw_values)
    {
        if (!val.second->has_valid_samples())
        {
            SPDLOG_ERROR(val.first + "-axis accelerometer measured no data");
            throw elegoo::common::CommandError(val.first + "-axis accelerometer measured no data");
        }

        auto data = helper->process_accelerometer_data(val.second);
        double vx = 0.;
        double vy = 0.;
        double vz = 0.;
        for(auto ii = 0 ; ii < data->psd_x.size(); ++ii)
        {
            vx += data->psd_x.at(ii);
            vy += data->psd_y.at(ii);
            vz += data->psd_z.at(ii);
        }

        char buf[128] = {};
        snprintf(buf, sizeof(buf),"Axes noise for %s-axis accelerometer: %.6f (x), %.6f (y), %.6f (z)",val.first.c_str(), vx, vy, vz);
        // 向 GCode 命令发送响应信息
        gcmd->respond_info(std::string(buf),true);
    }
}

json ResonanceTester::get_status(double eventtime)
{
    SPDLOG_DEBUG("{} #1 ",__func__);
    return this->status;
}

bool ResonanceTester::is_valid_name_suffix(const std::string& name_suffix)
{
    std::string modified_suffix = name_suffix;

    modified_suffix.erase(remove(modified_suffix.begin(), modified_suffix.end(), '-'), modified_suffix.end());

    modified_suffix.erase(remove(modified_suffix.begin(), modified_suffix.end(), '_'), modified_suffix.end());

    for (char c : modified_suffix)
    {
        if (!std::isalnum(c))
        {
            return false;
        }
    }
    return true;
}

std::string ResonanceTester::get_filename(
    const std::string& base, const std::string& name_suffix,
    std::shared_ptr<TestAxis> axis, const std::vector<double>& point,
    std::string chip_name)
{
    std::string name = base;

    if (axis)
    {
        name += "_" + axis->get_name();
    }

    if (!chip_name.empty())
    {
        size_t pos = 0;
        while ((pos = chip_name.find(" ", pos)) != std::string::npos)
        {
            chip_name.replace(pos, 1, "_");
            pos += 1;
        }
        name += "_" + chip_name;
    }

    if (point.size() == 3)
    {
        name += "_";
        std::stringstream point_stream;
        point_stream << point.at(0) << "_"
                     << point.at(1) << "_"
                     << point.at(2);
        name += point_stream.str();
    }

    name += "_" + name_suffix;

    return "/tmp/" + name + ".csv";
}

std::string ResonanceTester::save_calibration_data(const std::string& base_name, const std::string& name_suffix,
    std::shared_ptr<ShaperCalibrate> shaper_calibrate,
    std::shared_ptr<TestAxis> axis,
    std::shared_ptr<CalibrationData> calibration_data,
    std::vector<CalibrationResult> all_shapers,
    const std::vector<double>& point,
    double max_freq)
{
    std::string output = get_filename(base_name, name_suffix, axis, point);
    shaper_calibrate->save_calibration_data(output, calibration_data, all_shapers, max_freq);
    return output;
}

std::map<std::shared_ptr<TestAxis>, std::shared_ptr<CalibrationData>>
    ResonanceTester::run_test(std::shared_ptr<GCodeCommand> gcmd,
        std::vector<std::shared_ptr<TestAxis>> axes,
        std::shared_ptr<ShaperCalibrate> helper,
        const std::string& raw_name_suffix,
        std::vector<std::shared_ptr<AccelChip>> accel_chips,
        const std::vector<double>& test_point)
{
    std::shared_ptr<ToolHead> toolhead =
        any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));

    std::map<std::shared_ptr<TestAxis>, std::shared_ptr<CalibrationData>> calibration_data;
    for (auto axis : axes)
    {
        calibration_data[axis] = nullptr;
    }

    test->prepare_test(gcmd);

    std::vector<std::vector<double>> test_points;
    if (!test_point.empty())
    {
        test_points.push_back(test_point);
    }
    else
    {
        test_points = test->get_start_test_points();
    }
;
    for (std::vector<double> point : test_points)
    {
        toolhead->manual_move(point, move_speed);
        gcmd->respond_info("Probing point (" + std::to_string(point[0]) +
            ", " + std::to_string(point[1]) + ", " + std::to_string(point[2]) + ")", true);

        for (auto axis : axes)
        {
            toolhead->wait_moves();
            toolhead->dwell(0.500);

            if(axes.size() > 1)
            {
                gcmd->respond_info("Testing axis " + axis->get_name(), true);
            }

            std::vector<std::tuple<std::string,
                std::shared_ptr<AccelQueryHelper>,
                std::string>> raw_values;
            if (accel_chips.empty())
            {
                for (auto chip : this->accel_chips)
                {
                    if (axis->matches(chip.first))
                    {
                        json feedback = {};
                        feedback["command"] = "resonance_tester";
                        feedback["axis"] = axis->get_name();
                        feedback["freq"] = 0.;
                        try
                        {
                            auto aclient = chip.second->start_internal_client();
                            raw_values.push_back(std::make_tuple(chip.first, aclient, chip.second->name));
                                
                            json accel_status = chip.second->get_accel_status();
                            if(accel_status.contains("accel_status") && accel_status["accel_status"].get<bool>())
                            {
                                feedback["result"] = "normal";
                                gcmd->respond_feedback(feedback);
                                SPDLOG_INFO("{} chip_name:{} accel_chips.size:{} feedback.dump:{}",__func__,chip.second->get_name(),accel_chips.size(),feedback.dump());
                            }
                            else
                            {
                                feedback["result"] = "abnormal";
                                gcmd->respond_feedback(feedback);
                                SPDLOG_ERROR("{} chip_name:{} accel_chips.size:{} feedback.dump:{}",__func__,chip.second->get_name(),accel_chips.size(),feedback.dump());
                                throw elegoo::common::CommandError("accel status abnormal");
                            }
                        }
                        catch(...)
                        {
                            feedback["result"] = "abnormal";
                            gcmd->respond_feedback(feedback);
                            SPDLOG_ERROR("{} chip_name:{} accel_chips.size:{} feedback.dump:{}",__func__,chip.second->get_name(),accel_chips.size(),feedback.dump());
                            throw elegoo::common::CommandError("accel status abnormal");
                        }
                    }
                }
            }
            else
            {
                for (auto chip : accel_chips)
                {
                    json feedback = {};
                    feedback["command"] = "resonance_tester";
                    feedback["axis"] = axis->get_name();
                    feedback["freq"] = 0.;
                    try
                    {
                        auto aclient = chip->start_internal_client();
                        raw_values.push_back(std::make_tuple("", aclient, chip->get_name()));
                        
                        json accel_status = chip->get_accel_status();
                        if(accel_status.contains("accel_status") && accel_status["accel_status"].get<bool>())
                        {
                            feedback["result"] = "normal";
                            gcmd->respond_feedback(feedback);
                            SPDLOG_INFO("{} chip_name:{} accel_chips.size:{} feedback.dump:{}",__func__,chip->get_name(),accel_chips.size(),feedback.dump());
                        }
                        else
                        {
                            feedback["result"] = "abnormal";
                            gcmd->respond_feedback(feedback);
                            SPDLOG_INFO("{} chip_name:{} accel_chips.size:{} feedback.dump:{}",__func__,chip->get_name(),accel_chips.size(),feedback.dump());
                            throw elegoo::common::CommandError("accel status abnormal");
                        }
                    }
                    catch(...)
                    {
                        feedback["result"] = "abnormal";
                        gcmd->respond_feedback(feedback);
                        SPDLOG_INFO("{} chip_name:{} accel_chips.size:{} feedback.dump:{}",__func__,chip->get_name(),accel_chips.size(),feedback.dump());
                        throw elegoo::common::CommandError("accel status abnormal");
                    }
                }
            }

            test->run_test(axis, gcmd);

            for (auto val : raw_values)
            {
                std::get<1>(val)->finish_measurements();
                if (!raw_name_suffix.empty())
                {
                    std::string raw_name = get_filename(
                        "raw_data", raw_name_suffix, axis,
                        test_points.size() > 1 ? point : std::vector<double>(),
                        accel_chips.size() > 0 ? std::get<2>(val) : ""
                    );

                    std::get<1>(val)->write_to_file(raw_name);
                    gcmd->respond_info("Writing raw accelerometer data to " + raw_name + " file", true);
                }
            }

            if (helper == nullptr)
            {
                continue;
            }

            for (auto val : raw_values)
            {
                if (!std::get<1>(val)->has_valid_samples())
                {
                    elegoo::common::CommandError("accelerometer '" + std::get<2>(val) + "' measured no data");
                }

                std::shared_ptr<CalibrationData> new_data = helper->process_accelerometer_data(std::get<1>(val));

                if (!calibration_data[axis])
                {
                    calibration_data[axis] = new_data;
                }
                else
                {
                    calibration_data[axis]->add_data(new_data);
                }
            }
        }
    }

    return calibration_data;
}

std::vector<std::shared_ptr<AccelChip>> ResonanceTester::parse_chips(const std::string& accel_chips)
{
    std::vector<std::shared_ptr<AccelChip>> parsed_chips;
    std::vector<std::string> chip_names = elegoo::common::split(accel_chips, ",");
    std::string chip_lookup_name;
    for (size_t i = 0; i < chip_names.size(); i++) {
        if (chip_names.at(i).find("adxl345") != std::string::npos) {
            chip_lookup_name = elegoo::common::strip(chip_names.at(i));
        } else {
            chip_lookup_name = "adxl345 " + elegoo::common::strip(chip_names.at(i));
        }
        parsed_chips.push_back(any_cast<std::shared_ptr<AccelChip>>(printer->lookup_object(chip_lookup_name)));
    }

    return parsed_chips;
}

double ResonanceTester::get_max_calibration_freq()
{
    return 1.5 * test->get_max_freq();
}

std::string ResonanceTester::get_current_time_string()
{
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    // 将 time_t 转换为 tm 结构
    std::tm tm_now = *std::localtime(&now_time);

    // 使用 std::put_time 来格式化输出
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y%m%d_%H%M%S");

    return oss.str();
}

std::shared_ptr<ResonanceTester> resonance_tester_load_config(
        std::shared_ptr<ConfigWrapper> config) {
    SPDLOG_DEBUG("{} #1",__func__);
    return std::make_shared<ResonanceTester>(config);
}

}
}
