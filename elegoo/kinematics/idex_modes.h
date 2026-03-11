/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-20 18:11:21
 * @LastEditors  : Ben
 * @LastEditTime : 2024-11-28 21:24:21
 * @Description  : Support for duplication and mirroring modes for IDEX printers
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once


class DualCarriages
{
public:
    DualCarriages();
    ~DualCarriages();

    void get_rails();
    void get_primary_rail();
    void toggle_active_dc_rail();
    void home();
    void get_status();
    void get_kin_range();
    void get_dc_order();
    void activate_dc_mode();
    void cmd_SET_DUAL_CARRIAGE();
    void cmd_SAVE_DUAL_CARRIAGE_STATE();
    void cmd_RESTORE_DUAL_CARRIAGE_STATE();

private:
    void handle_ready();

};


class DualCarriagesRail
{
public:
    DualCarriagesRail();
    ~DualCarriagesRail();

    void get_rail();
    void is_active();
    void get_axis_position();
    void apply_transform();
    void activate();
    void inactivate();
    void override_axis_scaling();

private:

};