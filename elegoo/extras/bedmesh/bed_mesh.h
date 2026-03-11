/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-23 18:34:00
 * @Description  : bed_mesh is a functional module in the Elegoo firmware used to
 * achieve automatic bed leveling. By creating a mesh model to represent the flatness
 * of the bed, the bed_mesh module can compensate for any unevenness in the bed, ensuring
 * that the distance between the print bed and the nozzle remains consistent. This improves
 * print quality and reliability.
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
#include "gcode_move.h"
#include "extras/exclude_object.h"

class ConfigWrapper;
class GCodeCommand;
class GCodeDispatch;
class Printer;
class WebHooks;
class WebRequest;
class PrinterConfig;
class ToolHead;

namespace elegoo
{
    namespace extras
    {

        class GCodeMove;
        class ProbeManager;
        class BedMeshCalibrate;
        class ProfileManager;
        class ZMesh;
        class MoveSplitter;
        class PrinterProbe;
        class ProbePointsHelper;
        class ExcludeObject;
        class PrintStats;
        class BedMesh : public std::enable_shared_from_this<BedMesh>
        {
        public:
            static constexpr int FADE_DISABLE = 0x7FFFFFFF;
            BedMesh() = default;
            ~BedMesh() = default;
            void init(std::shared_ptr<ConfigWrapper> config);
            void handle_connect();
            void set_mesh(std::shared_ptr<ZMesh> mesh);
            double get_z_factor(double z_pos);
            std::vector<double> get_position();
            void move(const std::vector<double> &newpos, double speed);
            json get_status(double eventtime);
            void update_status();
            std::shared_ptr<ZMesh> get_mesh();
            void cmd_BED_MESH_OUTPUT(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_BED_MESH_MAP(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_BED_MESH_CLEAR(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_BED_MESH_OFFSET(std::shared_ptr<GCodeCommand> gcmd);
            std::function<void(const std::string &)> save_profile;
            std::shared_ptr<GCodeMoveTransform> move_transform;
            void set_execute_calirate_from_slicer(int execute_calirate_from_slicer)
            {
                this->execute_calirate_from_slicer = execute_calirate_from_slicer;
            }
            int get_execute_calirate_from_slicer()
            {
                return this->execute_calirate_from_slicer;
            }
            void set_adaptive_ignore_state(int state)
            {
                this->adaptive_ignore_state = state;
            }
            int get_adaptive_ignore_state()
            {
                return this->adaptive_ignore_state;
            }
            void set_print_surface(int value);
            int get_print_surface();

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<BedMeshCalibrate> bmc;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<MoveSplitter> splitter;
            std::shared_ptr<ProfileManager> pmgr;
            std::shared_ptr<WebHooks> webhooks;
            std::shared_ptr<ToolHead> toolhead;
            std::shared_ptr<ZMesh> z_mesh;
            std::shared_ptr<PrintStats> print_stats;
            std::vector<double> last_position;
            double horizontal_move_z;
            double fade_start;
            double fade_end;
            double fade_dist;
            bool log_fade_complete;
            double base_fade_target;
            double fade_target;
            double tool_offset;
            double adaptive_margin;
            json status;

            double magic[3] = {0., 0., 0.};
            double magic_pos[3] = {0., 0., 0.};
            uint8_t direction[3] = {0, 0, 0};
            uint8_t last_direction[3] = {0, 0, 0};
            std::vector<double> last_split_move_position;
            void _handle_dump_request(std::shared_ptr<WebRequest> web_request);

            int execute_calirate_from_slicer;
            int adaptive_ignore_state;
            int print_surface;
        };

        enum ZrefMode
        {
            DISABLED = 0,
            IN_MESH = 1,
            PROBE = 2
        };

        class BedMeshCalibrate
        {
        public:
            BedMeshCalibrate(std::shared_ptr<ConfigWrapper> config,
                             std::shared_ptr<BedMesh> bedmesh);
            ~BedMeshCalibrate() = default;

            void print_generated_points(bool truncate = false);
            bool set_adaptive_mesh(std::shared_ptr<GCodeCommand> gcmd);
            void update_config(std::shared_ptr<GCodeCommand> gcmd);
            json dump_calibration(std::shared_ptr<GCodeCommand> gcmd = nullptr);
            void cmd_BED_MESH_CALIBRATE(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_SET_PRINT_SURFACE(std::shared_ptr<GCodeCommand> gcmd);
            std::string probe_finalize(const std::vector<double> &offsets,
                                       std::vector<std::vector<double>> &positions);
            json get_web_feedback();

        private:
            void init_mesh_config(std::shared_ptr<ConfigWrapper> config);
            void verify_algorithm();
            void dump_points(const std::vector<std::vector<double>> &probed_pts,
                             const std::vector<std::vector<double>> &corrected_pts,
                             const std::vector<double> &offsets);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<BedMesh> bedmesh;
            std::shared_ptr<WebHooks> webhooks;
            std::shared_ptr<ProbeManager> probe_mgr;
            json orig_config;
            json mesh_config;
            double radius;
            std::vector<double> origin;
            std::vector<double> mesh_min;
            std::vector<double> mesh_max;
            double adaptive_margin;
            std::string profile_name;
            std::shared_ptr<GCodeDispatch> gcode;
            std::set<std::string> ALGOS;
        };

        class ProbeManager : public std::enable_shared_from_this<ProbeManager>
        {
        public:
            ProbeManager() = default;
            ~ProbeManager() = default;
            void init(std::shared_ptr<ConfigWrapper> config, json orig_config, std::function<std::string(const std::vector<double> &, std::vector<std::vector<double>> &)> finalize_cb);
            void start_probe(std::shared_ptr<GCodeCommand> gcmd);
            std::vector<double> get_zero_ref_pos();
            ZrefMode get_zero_ref_mode();
            std::map<int, std::vector<std::vector<double>>>
            get_substitutes();
            void generate_points(json mesh_config,
                                 const std::vector<double> &mesh_min,
                                 const std::vector<double> &mesh_max,
                                 double radius, const std::vector<double> &origin,
                                 const std::string &probe_method = "automatic");
            std::vector<std::vector<double>> get_base_points();
            std::vector<std::vector<double>> get_std_path();
            json get_web_feedback();
            void notify_failed();

        private:
            void init_faulty_regions(std::shared_ptr<ConfigWrapper> config);
            void process_faulty_regions(const std::vector<double> &min_pt,
                                        const std::vector<double> &max_pt,
                                        double radius);
            void gen_faulty_path(
                const std::vector<double> &last_pt,
                int idx, bool ascnd_x, bool dir_change,
                std::function<void(std::vector<double>, bool)> yield_fn);

            void gen_dir_change(
                std::vector<double> last_pt,
                std::vector<double> next_pt, bool ascnd_x,
                std::function<void(std::vector<double>)> yield_fn);
            void gen_arc(
                std::vector<double> origin, double radius,
                int start, int step, int count,
                std::function<void(std::vector<double>)> yield_fn);

        private:
            std::shared_ptr<Printer> printer;
            json orig_config;

            std::vector<std::pair<std::vector<double>, std::vector<double>>> faulty_regions;
            std::vector<std::vector<double>> base_points;
            std::vector<double> zero_ref_pos;
            ZrefMode zref_mode;
            std::map<int, std::vector<std::vector<double>>> substitutes;
            bool is_round;
            std::shared_ptr<ProbePointsHelper> probe_helper;
            double cfg_overshoot;
            double overshoot;
        };

        class MoveSplitter
        {
        public:
            MoveSplitter(std::shared_ptr<ConfigWrapper> config,
                         std::shared_ptr<GCodeDispatch> gcode);
            ~MoveSplitter();

            void initialize(std::shared_ptr<ZMesh> mesh, double fade_offset);
            void build_move(const std::vector<double> prev_pos,
                            const std::vector<double> next_pos, double factor);
            std::vector<double> split();
            bool traverse_complete;

        private:
            double calc_z_offset(const std::vector<double> pos);
            void set_next_move(double distance_from_prev);

        private:
            double split_delta_z;
            double magic;
            double move_check_distance;
            std::shared_ptr<ZMesh> z_mesh;
            double fade_offset;
            std::shared_ptr<GCodeDispatch> gcode;
            std::vector<double> prev_pos;
            std::vector<double> next_pos;
            std::vector<double> current_pos;
            std::vector<bool> axis_move;
            double z_factor;
            double z_offset;
            double distance_checked;
            double total_move_length;
        };

        class ZMesh
        {
        public:
            ZMesh(json params, const std::string &name);
            ~ZMesh();

            std::vector<std::vector<double>> get_mesh_matrix();
            std::vector<std::vector<double>> get_probed_matrix();
            json get_mesh_params();
            std::string get_profile_name();
            void print_probed_matrix();
            void print_mesh(double move_z = 0);
            void build_mesh(const std::vector<std::vector<double>> &z_matrix);
            double set_zero_reference(double xpos, double ypos);
            void set_mesh_offsets(const std::vector<double> &offsets);
            double get_x_coordinate(int index);
            double get_y_coordinate(int index);
            double calc_z(double x, double y);
            std::vector<double> get_z_range();
            double get_z_average();
            double get_offset() { return offset; }

        private:
            std::pair<double, int> get_linear_index(double coord, int axis);
            void sample_direct(const std::vector<std::vector<double>> &z_matrix);
            void sample_lagrange(const std::vector<std::vector<double>> &z_matrix);
            std::pair<std::vector<double>, std::vector<double>> get_lagrange_coords();
            double calc_lagrange(const std::vector<double> &lpts, double c, int vec, int axis = 0);
            void sample_bicubic(const std::vector<std::vector<double>> &z_matrix);
            std::vector<double> get_x_ctl_pts(int x, int y);
            std::vector<double> get_y_ctl_pts(int x, int y);
            double cardinal_spline(const std::vector<double> &p, double tension);

        private:
            std::function<void(const std::vector<std::vector<double>> &)> sample;
            std::vector<std::vector<double>> probed_matrix;
            std::vector<std::vector<double>> mesh_matrix;
            std::vector<double> mesh_offsets;
            json mesh_params;
            std::string profile_name;
            int mesh_x_count;
            int mesh_y_count;
            double mesh_x_min;
            double mesh_x_max;
            double mesh_x_dist;
            double mesh_y_min;
            double mesh_y_max;
            double mesh_y_dist;
            int x_mult;
            int y_mult;
            double offset = 0.;
        };

        class ProfileManager
        {
        public:
            ProfileManager(std::shared_ptr<ConfigWrapper> config,
                           std::shared_ptr<BedMesh> bedmesh);
            ~ProfileManager();
            json get_profiles();
            void save_profile(const std::string &prof_name);
            void load_profile(const std::string &prof_name);
            void remove_profile(const std::string &prof_name);
            void cmd_BED_MESH_PROFILE(std::shared_ptr<GCodeCommand> gcmd);

        private:
            std::pair<std::string, std::string> split_once(const std::string &arg, char delimiter);

        private:
            json profiles;
            std::shared_ptr<Printer> printer;
            std::shared_ptr<BedMesh> bedmesh;
            std::shared_ptr<GCodeDispatch> gcode;
            std::string name;
        };

        std::shared_ptr<BedMesh> bed_mesh_load_config(std::shared_ptr<ConfigWrapper> config);

    }
}