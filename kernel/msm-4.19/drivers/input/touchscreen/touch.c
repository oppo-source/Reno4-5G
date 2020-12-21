/***************************************************
 * File:touch.c
 * VENDOR_EDIT
 * Copyright (c)  2008- 2030  Oppo Mobile communication Corp.ltd.
 * Description:
 *             tp dev
 * Version:1.0:
 * Date created:2016/09/02
 * Author: hao.wang@Bsp.Driver
 * TAG: BSP.TP.Init
*/

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include "oppo_touchscreen/tp_devices.h"
#include "oppo_touchscreen/touchpanel_common.h"
#include <soc/oppo/oppo_project.h>
#include <soc/oppo/device_info.h>

#define MAX_LIMIT_DATA_LENGTH         100

extern char *saved_command_line;
int g_tp_dev_vendor = TP_UNKNOWN;
int g_tp_prj_id = 0;

/*if can not compile success, please update vendor/oppo_touchsreen*/
struct tp_dev_name tp_dev_names[] = {
    {TP_OFILM, "OFILM"},
    {TP_BIEL, "BIEL"},
    {TP_TRULY, "TRULY"},
    {TP_BOE, "BOE"},
    {TP_G2Y, "G2Y"},
    {TP_TPK, "TPK"},
    {TP_JDI, "JDI"},
    {TP_TIANMA, "TIANMA"},
    {TP_SAMSUNG, "SAMSUNG"},
    {TP_DSJM, "DSJM"},
    {TP_BOE_B8, "BOEB8"},
    {TP_UNKNOWN, "UNKNOWN"},
};

#define GET_TP_DEV_NAME(tp_type) ((tp_dev_names[tp_type].type == (tp_type))?tp_dev_names[tp_type].name:"UNMATCH")

bool __init tp_judge_ic_match(char *tp_ic_name)
{
    pr_err("[TP] tp_ic_name = %s \n", tp_ic_name);
    //pr_err("[TP] boot_command_line = %s \n", saved_command_line);

    switch(get_project()) {
    case 19101:
    case 19102:
    case 19501:
    case 19335:
        g_tp_prj_id = 19101;
        if (strstr(tp_ic_name, "synaptics-s3908") && (strstr(saved_command_line, "mdss_dsi_oppo19101boe_nt37800_1080_2400_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19101boe_nt37800_1080_2340_cmd"))) {
            g_tp_dev_vendor = TP_BOE;
            return true;
        }
        if (strstr(tp_ic_name, "synaptics-s3706") && (strstr(saved_command_line, "mdss_dsi_oppo19125samsung_s6e3fc2x01_1080_2340_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19125samsung_ams641rw01_1080_2340_cmd"))) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        if (strstr(tp_ic_name, "sec-s6sy771") && (strstr(saved_command_line, "mdss_dsi_oppo19101samsung_sofx01f_a_1080_2400_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19101samsung_amb655uv01_1080_2400_cmd"))) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        pr_err("[TP] Driver does not match the project\n");
        break;

    case 19125:
    case 19126:
    case 19127:
    case 19128:
    case 19521:
    case 19522:
        g_tp_prj_id = 19125;
        if (strstr(tp_ic_name, "synaptics-s3706") && (strstr(saved_command_line, "mdss_dsi_oppo19125samsung_s6e3fc2x01_1080_2340_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19125samsung_ams641rw01_1080_2340_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19101samsung_sofx01f_a_1080_2400_cmd"))) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        if (strstr(tp_ic_name, "Goodix-gt9886") && strstr(saved_command_line, "mdss_dsi_oppo19125samsung_ams644va04_1080_2400_video")) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        pr_err("[TP] Driver does not match the project\n");
        break;

    case 19191:
    case 19192:
    case 19591:
        g_tp_prj_id = 19191;
        if (strstr(tp_ic_name, "sec-s6sy771") && (strstr(saved_command_line, "mdss_dsi_oppo19101samsung_sofx01f_a_1080_2400_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19101samsung_amb655uv01_1080_2400_cmd"))) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        if (strstr(tp_ic_name, "synaptics-s3908") && (strstr(saved_command_line, "mdss_dsi_oppo19101boe_nt37800_1080_2400_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19101boe_nt37800_1080_2340_cmd"))) {
            g_tp_dev_vendor = TP_BOE;
            return true;
        }
        pr_err("[TP] Driver does not match the project\n");
        break;

    case 19015:
    case 19016:
    case 19525:
    case 19518:
        g_tp_prj_id = 19015;
        if (strstr(tp_ic_name, "Goodix-gt9886")) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        pr_err("[TP] Driver does not match the project\n");
        break;

    case 19517:
        g_tp_prj_id = 19517;
        if (strstr(tp_ic_name, "Goodix-gt9886")) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        pr_err("[TP] Driver does not match the project\n");
        break;

    default:
        pr_err("Invalid project\n");
        break;
    }

    pr_err("Lcd module not found\n");

    return false;
}

int tp_util_get_vendor(struct hw_resource *hw_res, struct panel_info *panel_data)
{
    char* vendor;

    panel_data->test_limit_name = kzalloc(MAX_LIMIT_DATA_LENGTH, GFP_KERNEL);
    if (panel_data->test_limit_name == NULL) {
        pr_err("[TP]panel_data.test_limit_name kzalloc error\n");
    }
    switch(get_project()) {
    case 19125:
    case 19126:
    case 19127:
    case 19128:
    case 19521:
    case 19522:
        g_tp_prj_id = 19125;
        if ( (strstr(saved_command_line, "mdss_dsi_oppo19125samsung_s6e3fc2x01_1080_2340_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19125samsung_ams641rw01_1080_2340_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19101samsung_sofx01f_a_1080_2400_cmd"))) {
            g_tp_dev_vendor = TP_SAMSUNG;
        }
        if (strstr(saved_command_line, "mdss_dsi_oppo19125samsung_ams644va04_1080_2400_video")) {
            g_tp_dev_vendor = TP_SAMSUNG;
       }
       break;
    }
   panel_data->tp_type = g_tp_dev_vendor;

    if (panel_data->tp_type == TP_UNKNOWN) {
        pr_err("[TP]%s type is unknown\n", __func__);
        return 0;
    }

    vendor = GET_TP_DEV_NAME(panel_data->tp_type);

    strcpy(panel_data->manufacture_info.manufacture, vendor);

    snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
            "tp/%d/FW_%s_%s.img",
            g_tp_prj_id, panel_data->chip_name, vendor);

    if (strstr(saved_command_line, "mdss_dsi_oppo19015samsung_ams644vk01_1080_2400_cmd") && g_tp_prj_id == 19015) {
        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
                "tp/%d/FW_%s_%s_19551.img",
                g_tp_prj_id, panel_data->chip_name, vendor);
    }
    if (g_tp_prj_id == 19015 && (strstr(saved_command_line, "mdss_dsi_oppo19125samsung_s6e3fc2x01_1080_2340_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19125samsung_ams641rw01_1080_2340_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19101samsung_sofx01f_a_1080_2400_cmd"))) {
        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
                "tp/%d/FW_%s_%s_19125.img",
                g_tp_prj_id, panel_data->chip_name, vendor);
    }

    if (panel_data->test_limit_name) {
        snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
            "tp/%d/LIMIT_%s_%s.img",
            g_tp_prj_id, panel_data->chip_name, vendor);
    }

    switch(get_project()) {
    case 19125:
    case 19126:
    case 19127:
    case 19128:
    case 19521:
    case 19522:
        memcpy(panel_data->manufacture_info.version, "0xbd2830000", 11);
        break;

    case 19015:
    case 19016:
    case 19525:
    case 19517:
        memcpy(panel_data->manufacture_info.version, "0xbd3180000", 11);
        break;
    default:
        break;
    }

    panel_data->manufacture_info.fw_path = panel_data->fw_name;

    pr_info("[TP]vendor:%s fw:%s limit:%s\n",
        vendor,
        panel_data->fw_name,
        panel_data->test_limit_name == NULL ? "NO Limit" : panel_data->test_limit_name);

    return 0;
}

