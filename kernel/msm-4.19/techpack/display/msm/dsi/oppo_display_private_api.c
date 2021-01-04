/***************************************************************
** Copyright (C),  2018,  OPPO Mobile Comm Corp.,  Ltd
** VENDOR_EDIT
** File : oppo_display_private_api.h
** Description : oppo display private api implement
** Version : 1.0
** Date : 2018/03/20
** Author : Jie.Hu@PSW.MM.Display.Stability
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Hu.Jie          2018/03/20        1.0           Build this moudle
******************************************************************/
#include "oppo_display_private_api.h"
/*
 * we will create a sysfs which called /sys/kernel/oppo_display,
 * In that directory, oppo display private api can be called
 */
#include <linux/notifier.h>
#include <linux/msm_drm_notify.h>
#include "../oppo_mm_kevent_fb.h"
#include <soc/oppo/device_info.h>

int hbm_mode = 0;
int spr_mode = 0;
int failsafe_mode = 0;
int seed_mode = 0;
int aod_light_mode = 0;
int lcd_closebl_flag = 0;
int lcd_closebl_flag_fp = 0;
int oppo_request_power_status = OPPO_DISPLAY_POWER_ON;
int oppo_aod_gamma_check_data = -1;

extern int oppo_underbrightness_alpha;
extern int msm_drm_notifier_call_chain(unsigned long val, void *v);
int oppo_dc2_alpha;
int oppo_dimlayer_bl_enable_v3 = 0;
int oppo_dimlayer_bl_enable_v2 = 0;
int oppo_dimlayer_bl_enable = 0;
int oppo_dimlayer_bl_enabled = 0;
int oppo_dimlayer_bl_alpha = 260;
int oppo_dimlayer_bl_alpha_value = 260;
int oppo_dimlayer_bl_enable_real = 0;
int oppo_dimlayer_dither_threshold = 0;
int oppo_dimlayer_dither_bitdepth = 6;
int oppo_dimlayer_bl_delay = 500;
int oppo_dimlayer_bl_delay_after = 0;
int oppo_boe_dc_min_backlight = 175;
int oppo_boe_dc_max_backlight = 3350;
int oppo_boe_max_backlight = 2050;
int oppo_dimlayer_bl_enable_v3_real;
int oppo_onscreenfp_status = 0;
int oppo_display_mode = 1;
bool oppo_dc_v2_on = false;

ktime_t oppo_onscreenfp_pressed_time;
u32 oppo_onscreenfp_vblank_count = 0;

#define PANEL_TX_MAX_BUF 256
#define PANEL_CMD_MIN_TX_COUNT 2
#define FFL_FP_LEVEL 150

enum PANEL_VOLTAGE_ENUM{
	PANEL_VOLTAGE_ID_VDDI = 0,
	PANEL_VOLTAGE_ID_VDDR,
	PANEL_VOLTAGE_ID_VG_BASE,
	PANEL_VOLTAGE_ID_MAX,
};

#define PANEL_VOLTAGE_VALUE_COUNT 4

typedef struct panel_voltage_bak{
	u32 voltage_id;
	u32 voltage_min;
	u32 voltage_current;
	u32 voltage_max;
	char pwr_name[20];
}PANEL_VOLTAGE_BAK;

PANEL_VOLTAGE_BAK panel_vol_bak[PANEL_VOLTAGE_ID_MAX] = {{0},{0},{2,0,1,2,""}};
u32 panel_pwr_vg_base = 0;

DEFINE_MUTEX(oppo_power_status_lock);
DEFINE_MUTEX(oppo_seed_lock);
DEFINE_MUTEX(oppo_hbm_lock);
DEFINE_MUTEX(oppo_aod_light_mode_lock);
DEFINE_MUTEX(oppo_spr_lock);
DEFINE_MUTEX(oppo_failsafe_lock);
static DECLARE_WAIT_QUEUE_HEAD(oppo_aod_wait);

static inline bool is_oppo_display_aod_mode(void)
{
	return !oppo_onscreenfp_status && !oppo_display_mode;
}

bool is_oppo_aod_ramless(void)
{
	struct dsi_display *display = get_main_display();
	if (!display || !display->panel)
		return false;
	return display->panel->oppo_priv.is_aod_ramless;
}

int oppo_set_display_vendor(struct dsi_display *display)
{
	if (!display || !display->panel ||
	    !display->panel->oppo_priv.vendor_name ||
	    !display->panel->oppo_priv.manufacture_name) {
		pr_err("failed to config lcd proc device");
		return -EINVAL;
	}
	register_device_proc("lcd", (char *)display->panel->oppo_priv.vendor_name,
			     (char *)display->panel->oppo_priv.manufacture_name);

	return 0;
}

bool is_dsi_panel(struct drm_crtc *crtc)
{
	struct dsi_display *display = get_main_display();

	if (!display || !display->drm_conn || !display->drm_conn->state) {
		pr_err("failed to find dsi display\n");
		return false;
	}

	if (crtc != display->drm_conn->state->crtc)
		return false;

	return true;
}

int dsi_panel_aod_on(struct dsi_panel *panel) {
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_AOD_ON);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_AOD_ON cmds, rc=%d\n",
		       panel->name, rc);
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_aod_off(struct dsi_panel *panel) {
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_AOD_OFF);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_AOD_OFF cmds, rc=%d\n",
		       panel->name, rc);
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_failsafe_on(struct dsi_panel *panel) {
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_FAILSAFE_ON);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_FAILSAFE_ON cmds, rc=%d\n",
		       panel->name, rc);
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_failsafe_off(struct dsi_panel *panel) {
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_FAILSAFE_OFF);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_FAILSAFE_OFF cmds, rc=%d\n",
		       panel->name, rc);
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_hbm_on(struct dsi_panel *panel) {
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_HBM_ON);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_HBM_ON cmds, rc=%d\n",
		       panel->name, rc);
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_normal_hbm_on(struct dsi_panel *panel) {
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_NORMAL_HBM_ON);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_NORMAL_HBM_ON cmds, rc=%d\n",
		       panel->name, rc);
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_hbm_off(struct dsi_panel *panel) {
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	/* if hbm already set by onscreenfinger, keep it */
	if (panel->is_hbm_enabled) {
		rc = 0;
		goto error;
	}

	dsi_panel_set_backlight(panel, panel->bl_config.bl_level);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_HBM_OFF);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_HBM_OFF cmds, rc=%d\n",
		       panel->name, rc);
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_seed_mode_unlock(struct dsi_panel *panel, int mode)
{
	int rc = 0;

	if (!dsi_panel_initialized(panel))
		return -EINVAL;
	if (oppo_dc_v2_on) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SEED_ENTER);

		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SEED_ENTER cmds, rc=%d\n",
				panel->name, rc);
		}
	}
	switch (mode) {
	case 0:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SEED_MODE0);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SEED_MODE0 cmds, rc=%d\n",
					panel->name, rc);
		}
		break;
	case 1:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SEED_MODE1);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SEED_MODE1 cmds, rc=%d\n",
					panel->name, rc);
		}
		break;
	case 2:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SEED_MODE2);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SEED_MODE2 cmds, rc=%d\n",
					panel->name, rc);
		}
		break;
	case 3:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SEED_MODE3);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SEED_MODE3 cmds, rc=%d\n",
					panel->name, rc);
		}
		break;
	case 4:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SEED_MODE4);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SEED_MODE4 cmds, rc=%d\n",
					panel->name, rc);
		}
		break;
	default:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SEED_OFF);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SEED_OFF cmds, rc=%d\n",
					panel->name, rc);
		}
		pr_err("[%s] seed mode Invalid %d\n",
			panel->name, mode);
	}
	if (!oppo_dc_v2_on) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SEED_EXIT);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SEED_EXIT cmds, rc=%d\n",
				panel->name, rc);
		}
	}

	return rc;
}

int dsi_panel_seed_mode(struct dsi_panel *panel, int mode) {
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_seed_mode_unlock(panel, mode);

	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_spr_mode(struct dsi_panel *panel, int mode) {
	int rc = 0;

	if (!panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	switch (mode) {
	case 0:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SPR_MODE0);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SPR_MODE0 cmds, rc=%d\n",
					panel->name, rc);
		}
		break;
	case 1:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SPR_MODE1);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SPR_MODE1 cmds, rc=%d\n",
					panel->name, rc);
		}
		break;
	case 2:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SPR_MODE2);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SEED_MODE2 cmds, rc=%d\n",
					panel->name, rc);
		}
		break;
	default:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SPR_MODE0);  //gaos edit default is spr mode 0
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_SPR_MODE0 cmds, rc=%d\n",
					panel->name, rc);
		}
		pr_err("[%s] seed mode Invalid %d\n",
			panel->name, mode);
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_panel_read_panel_reg(struct dsi_display_ctrl *ctrl, struct dsi_panel *panel, u8 cmd, void *rbuf,  size_t len)
{
	int rc = 0;
	struct dsi_cmd_desc cmdsreq;
	u32 flags = 0;

	if (!panel || !ctrl || !ctrl->ctrl) {
		return -EINVAL;
	}

	if (!dsi_ctrl_validate_host_state(ctrl->ctrl)) {
		return 1;
	}

	/* acquire panel_lock to make sure no commands are in progress */
	mutex_lock(&panel->panel_lock);

	if (!dsi_panel_initialized(panel)) {
		rc = -EINVAL;
		goto error;
	}

	memset(&cmdsreq, 0x0, sizeof(cmdsreq));
	cmdsreq.msg.type = 0x06;
	cmdsreq.msg.tx_buf = &cmd;
	cmdsreq.msg.tx_len = 1;
	cmdsreq.msg.rx_buf = rbuf;
	cmdsreq.msg.rx_len = len;
	cmdsreq.msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
	flags |= (DSI_CTRL_CMD_FETCH_MEMORY | DSI_CTRL_CMD_READ |
		DSI_CTRL_CMD_CUSTOM_DMA_SCHED |
		DSI_CTRL_CMD_LAST_COMMAND);

	rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &cmdsreq.msg, flags);
	if (rc <= 0) {
		pr_err("%s, dsi_display_read_panel_reg rx cmd transfer failed rc=%d\n",
			__func__,
			rc);
		goto error;
	}
error:
	/* release panel_lock */
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int dsi_display_aod_on(struct dsi_display *display) {
	int rc = 0;
	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

		/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = dsi_panel_aod_on(display->panel);
		if (rc) {
			pr_err("[%s] failed to dsi_panel_aod_on, rc=%d\n",
			       display->name, rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);

	}
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_aod_off(struct dsi_display *display) {
	int rc = 0;
	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

		/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = dsi_panel_aod_off(display->panel);
		if (rc) {
			pr_err("[%s] failed to dsi_panel_aod_off, rc=%d\n",
			       display->name, rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);

	}
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_failsafe_on(struct dsi_display *display) {
	int rc = 0;
	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

		/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = dsi_panel_failsafe_on(display->panel);
		if (rc) {
			pr_err("[%s] failed to dsi_panel_failsafe_on, rc=%d\n",
			       display->name, rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);

	}
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_failsafe_off(struct dsi_display *display) {
	int rc = 0;
	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

		/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = dsi_panel_failsafe_off(display->panel);
		if (rc) {
			pr_err("[%s] failed to dsi_panel_failsafe_off, rc=%d\n",
			       display->name, rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);

	}
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_hbm_on(struct dsi_display *display) {
	int rc = 0;
	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

		/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = dsi_panel_hbm_on(display->panel);
		if (rc) {
			pr_err("[%s] failed to dsi_panel_hbm_on, rc=%d\n",
			       display->name, rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);

	}
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_normal_hbm_on(struct dsi_display *display) {
	int rc = 0;
	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

		/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = dsi_panel_normal_hbm_on(display->panel);
		if (rc) {
			pr_err("[%s] failed to dsi_panel_normal_hbm_on, rc=%d\n",
			       display->name, rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);

	}
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_hbm_off(struct dsi_display *display) {
	int rc = 0;
	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = dsi_panel_hbm_off(display->panel);
		if (rc) {
			pr_err("[%s] failed to dsi_panel_hbm_off, rc=%d\n",
			       display->name, rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);

	}
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_seed_mode(struct dsi_display *display, int mode) {
	int rc = 0;
	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

		/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = dsi_panel_seed_mode(display->panel, mode);
		if (rc) {
			pr_err("[%s] failed to dsi_panel_seed_on, rc=%d\n",
			       display->name, rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);

	}
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_spr_mode(struct dsi_display *display, int mode) {
	int rc = 0;
	if (!display || !display->panel) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

		/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = dsi_panel_spr_mode(display->panel, mode);
		if (rc) {
			pr_err("[%s] failed to dsi_panel_spr_on, rc=%d\n",
			       display->name, rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);

	}
	mutex_unlock(&display->display_lock);
	return rc;
}

int dsi_display_read_panel_reg(struct dsi_display *display,u8 cmd, void *data, size_t len) {
	int rc = 0;
	struct dsi_display_ctrl *m_ctrl;
	if (!display || !display->panel || data == NULL) {
		pr_err("%s, Invalid params\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc) {
			pr_err("%s, failed to allocate cmd tx buffer memory\n", __func__);
			goto done;
		}
	}

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("%s, cmd engine enable failed\n", __func__);
		goto done;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	}

	rc = dsi_panel_read_panel_reg(m_ctrl,display->panel, cmd, data,len);
	if (rc < 0) {
		pr_err("%s, [%s] failed to read panel register, rc=%d,cmd=%d\n",
		       __func__,
		       display->name,
		       rc,
		       cmd);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
					  DSI_ALL_CLKS, DSI_CLK_OFF);
	}

	dsi_display_cmd_engine_disable(display);

done:
	mutex_unlock(&display->display_lock);
	pr_err("%s, return: %d\n", __func__, rc);
	return rc;
}

static ssize_t oppo_display_set_aod(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oppo_display_set_aod = %d\n", __func__, temp_save);
	if(get_oppo_display_power_status() == OPPO_DISPLAY_POWER_ON) {
		if(get_main_display() == NULL) {
			printk(KERN_INFO "oppo_display_set_aod and main display is null");
			return count;
		}
		if(temp_save == 0) {
			dsi_display_aod_on(get_main_display());
		} else if(temp_save == 1) {
			dsi_display_aod_off(get_main_display());
		}
	} else {
		printk(KERN_ERR	 "%s oppo_display_set_aod = %d, but now display panel status is not on\n", __func__, temp_save);
	}
	return count;
}

int oppo_display_get_hbm_mode(void)
{
	return hbm_mode;
}

int __oppo_display_set_hbm(int mode) {
	mutex_lock(&oppo_hbm_lock);
	if(mode != hbm_mode) {
		hbm_mode = mode;
	}
	mutex_unlock(&oppo_hbm_lock);
	return 0;
}

int oppo_display_get_seed_mode(void)
{
	return seed_mode;
}

int __oppo_display_set_seed(int mode) {
	mutex_lock(&oppo_seed_lock);
	if(mode != seed_mode) {
		seed_mode = mode;
	}
	mutex_unlock(&oppo_seed_lock);
	return 0;
}

int __oppo_display_set_aod_light_mode(int mode) {
	mutex_lock(&oppo_aod_light_mode_lock);
	if(mode != aod_light_mode) {
		aod_light_mode = mode;
	}
	mutex_unlock(&oppo_aod_light_mode_lock);
	return 0;
}

static ssize_t oppo_display_set_hbm(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	struct dsi_display *display = get_main_display();
	int temp_save = 0;
	int ret = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oppo_display_set_hbm = %d\n", __func__, temp_save);
	if (get_oppo_display_power_status() != OPPO_DISPLAY_POWER_ON) {
		printk(KERN_ERR	 "%s oppo_display_set_hbm = %d, but now display panel status is not on\n", __func__, temp_save);
		return -EFAULT;
	}

	if (!display) {
		printk(KERN_INFO "oppo_display_set_hbm and main display is null");
		return -EINVAL;
	}
	__oppo_display_set_hbm(temp_save);

	if((hbm_mode > 1) &&(hbm_mode <= 10)) {
		ret = dsi_display_normal_hbm_on(get_main_display());
	} else if(hbm_mode == 1) {
		ret = dsi_display_hbm_on(get_main_display());
	} else if(hbm_mode == 0) {
		ret = dsi_display_hbm_off(get_main_display());
	}

	if (ret) {
		pr_err("failed to set hbm status ret=%d", ret);
		return ret;
	}

	return count;
}

int __oppo_display_set_failsafe(int mode) {
	mutex_lock(&oppo_failsafe_lock);
	if(mode != failsafe_mode) {
		failsafe_mode = mode;
	}
	mutex_unlock(&oppo_failsafe_lock);
	return 0;
}

static ssize_t oppo_display_set_failsafe(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	struct dsi_display *display = get_main_display();
	int temp_save = 0;
	int ret = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oppo_display_set_failsafe = %d\n", __func__, temp_save);
	if (get_oppo_display_power_status() != OPPO_DISPLAY_POWER_ON) {
		printk(KERN_ERR	 "%s oppo_display_set_failsafe = %d, but now display panel status is not on\n", __func__, temp_save);
		return -EFAULT;
	}

	if (!display) {
		printk(KERN_INFO "oppo_display_set_failsafe and main display is null");
		return -EINVAL;
	}

	__oppo_display_set_failsafe(temp_save);

	if(failsafe_mode == 1) {
		ret = dsi_display_failsafe_on(get_main_display());
	} else if(failsafe_mode == 0) {
		ret = dsi_display_failsafe_off(get_main_display());
	}

	if (ret) {
		pr_err("failed to set failsafe status ret=%d", ret);
		return ret;
	}

	return count;
}

int oppo_panel_update_seed_mode_unlock(struct dsi_panel *panel)
{
	dsi_panel_seed_mode_unlock(panel, seed_mode);

	return 0;
}

int oppo_dsi_update_seed_mode(void)
{
	struct dsi_display *display = get_main_display();
	int ret = 0;

	if (!display) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = dsi_display_seed_mode(display, seed_mode);

	return ret;
}

static ssize_t oppo_display_set_seed(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oppo_display_set_seed = %d\n", __func__, temp_save);

	__oppo_display_set_seed(temp_save);
	if(get_oppo_display_power_status() == OPPO_DISPLAY_POWER_ON) {
		if(get_main_display() == NULL) {
			printk(KERN_INFO "oppo_display_set_seed and main display is null");
			return count;
		}

		dsi_display_seed_mode(get_main_display(), seed_mode);
	} else {
		printk(KERN_ERR	 "%s oppo_display_set_seed = %d, but now display panel status is not on\n", __func__, temp_save);
	}
	return count;
}

int oppo_update_aod_light_mode_unlock(struct dsi_panel *panel)
{
	int rc = 0;
	enum dsi_cmd_set_type type;

	if (aod_light_mode == 1)
		type = DSI_CMD_AOD_LOW_LIGHT_MODE;
	else
		type = DSI_CMD_AOD_HIGH_LIGHT_MODE;

	rc = dsi_panel_tx_cmd_set(panel, type);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_AOD_LIGHT_MODE cmds, rc=%d\n",
		       panel->name, rc);
	}

	return rc;
}

static int oppo_update_aod_light_mode(void)
{
	struct dsi_display *display = get_main_display();
	int ret = 0;

	if (!display || !display->panel) {
		printk(KERN_INFO "oppo_set_aod_light_mode and main display is null");
		return -EINVAL;
	}

	if (display->panel->is_hbm_enabled) {
		pr_err("%s error panel->is_hbm_enabled\n", __func__);
		return -EINVAL;
	}

	if (get_oppo_display_scene() != OPPO_DISPLAY_AOD_SCENE) {
		pr_err("%s error get_oppo_display_scene = %d, \n", __func__, get_oppo_display_scene());
		return -EFAULT;
	}
	mutex_lock(&display->display_lock);
	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	}

	mutex_lock(&display->panel->panel_lock);

	if (display->panel->oppo_priv.is_aod_ramless &&
	    !is_oppo_display_aod_mode()) {
		pr_err("not support update aod_light_mode at non-aod mode\n");
		ret = -EINVAL;
		goto error;
	}

	if (!dsi_panel_initialized(display->panel)) {
		pr_err("dsi_panel_aod_low_light_mode is not init\n");
		ret = -EINVAL;
		goto error;
	}

	ret = oppo_update_aod_light_mode_unlock(display->panel);

	if (ret) {
		pr_err("failed to set aod light status ret=%d", ret);
		goto error;
	}

error:
	mutex_unlock(&display->panel->panel_lock);
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	}
	mutex_unlock(&display->display_lock);

	return ret;
}

static ssize_t oppo_set_aod_light_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);

	__oppo_display_set_aod_light_mode(temp_save);
	oppo_update_aod_light_mode();

	return count;
}

int __oppo_display_set_spr(int mode) {
	mutex_lock(&oppo_spr_lock);
	if(mode != spr_mode) {
		spr_mode = mode;
	}
	mutex_unlock(&oppo_spr_lock);
	return 0;
}
int oppo_dsi_update_spr_mode(void)
{
	struct dsi_display *display = get_main_display();
	int ret = 0;

	if (!display) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = dsi_display_spr_mode(display, spr_mode);

	return ret;
}

static ssize_t oppo_display_set_spr(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oppo_display_set_spr = %d\n", __func__, temp_save);

	__oppo_display_set_spr(temp_save);
	if(get_oppo_display_power_status() == OPPO_DISPLAY_POWER_ON) {
		if(get_main_display() == NULL) {
			printk(KERN_INFO "oppo_display_set_spr and main display is null");
			return count;
		}

		dsi_display_spr_mode(get_main_display(), spr_mode);
	} else {
		printk(KERN_ERR	 "%s oppo_display_set_spr = %d, but now display panel status is not on\n", __func__, temp_save);
	}
	return count;
}

int oppo_display_audio_ready = 0;
static ssize_t oppo_display_set_audio_ready(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {

	sscanf(buf, "%du", &oppo_display_audio_ready);

	return count;
}

static ssize_t oppo_display_get_hbm(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oppo_display_get_hbm = %d\n",hbm_mode);

	return sprintf(buf, "%d\n", hbm_mode);
}

static ssize_t oppo_display_get_seed(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oppo_display_get_seed = %d\n",seed_mode);

	return sprintf(buf, "%d\n", seed_mode);
}

static ssize_t oppo_get_aod_light_mode(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oppo_get_aod_light_mode = %d\n",aod_light_mode);

	return sprintf(buf, "%d\n", aod_light_mode);
}

static ssize_t oppo_display_get_spr(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oppo_display_get_spr = %d\n",spr_mode);

	return sprintf(buf, "%d\n", spr_mode);
}

static ssize_t oppo_display_get_mipi_clk_rate_hz(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	u64 clk_rate_hz = 0;
	
	if (!display) {
		pr_err("failed to get display");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	if (!display->panel || !display->panel->cur_mode) {
		pr_err("failed to get display mode");
		mutex_unlock(&display->display_lock);
		return -EINVAL;
	}
	clk_rate_hz = display->panel->cur_mode->timing.clk_rate_hz;

	mutex_unlock(&display->display_lock);

	return sprintf(buf, "%llu\n", clk_rate_hz);
}

static ssize_t oppo_display_get_failsafe(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oppo_display_get_failsafe = %d\n",failsafe_mode);

	return sprintf(buf, "%d\n", failsafe_mode);
}
static ssize_t oppo_display_regulator_control(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;
	struct dsi_display *temp_display;
	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oppo_display_regulator_control = %d\n", __func__, temp_save);
	if(get_main_display() == NULL) {
		printk(KERN_INFO "oppo_display_regulator_control and main display is null");
		return count;
	}
	temp_display = get_main_display();
	if(temp_save == 0) {
		dsi_pwr_enable_regulator(&temp_display->panel->power_info, false);
	} else if(temp_save == 1) {
		dsi_pwr_enable_regulator(&temp_display->panel->power_info, true);
	}
	return count;
}

static ssize_t oppo_display_get_panel_serial_number(struct device *dev,
struct device_attribute *attr, char *buf) {
	int ret = 0;
	unsigned char read[30];
	PANEL_SERIAL_INFO panel_serial_info;
	uint64_t serial_number;
	struct dsi_display *display = get_main_display();
	int i;

	if (!display) {
		printk(KERN_INFO "oppo_display_get_panel_serial_number and main display is null");
		return -1;
	}

	if(get_oppo_display_power_status() != OPPO_DISPLAY_POWER_ON) {
		printk(KERN_ERR"%s display panel in off status\n", __func__);
		return ret;
	}

	/*
	 * for some unknown reason, the panel_serial_info may read dummy,
	 * retry when found panel_serial_info is abnormal.
	 */
	for (i = 0;i < 10; i++) {
		if(!strcmp(display->panel->name,"samsung amb655uv01 amoled fhd+ panel with DSC")
			|| !strcmp(display->panel->name,"samsung amb655uv01 amoled fhd+ panel with DSC 2nd")) {
			ret = dsi_display_read_panel_reg(get_main_display(), 0xA1, read, 17);
		}else if (!strcmp(display->panel->name,"samsung ams643xf01 amoled fhd+ panel")) {
			ret = dsi_display_read_panel_reg(get_main_display(), 0xA1, read, 18);
		}else if (!strcmp(display->name,"qcom,mdss_dsi_oppo19101boe_nt37800_1080_2400_cmd")) {
			u32 mtime = 0;

			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);

			if (display->panel->panel_initialized) {

				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle,
							DSI_ALL_CLKS, DSI_CLK_ON);
				}
				{
					char value[] = { 0x55,0xAA,0x52,0x08,0x01 };
					ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xF0, value, sizeof(value));
				}
				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle,
							DSI_ALL_CLKS, DSI_CLK_OFF);
				}
				mutex_unlock(&display->panel->panel_lock);
				mutex_unlock(&display->display_lock);
			}
			if(ret < 0) {
				ret = scnprintf(buf, PAGE_SIZE,
						"Get panel serial number failed, reason:%d",ret);
				msleep(20);
				continue;
			}

			ret = dsi_display_read_panel_reg(display, 0xA2, read, 4);
			if(ret < 0) {
				ret = scnprintf(buf, PAGE_SIZE,
						"Get panel serial number failed, reason:%d",ret);
				msleep(20);
				continue;
			}

			pr_err("panel model: %2x %2x %2x %2x\n", read[0], read[1], read[2], read[3]);
			ret = dsi_display_read_panel_reg(display, 0xA3, read, 7);
			if(ret < 0) {
				ret = scnprintf(buf, PAGE_SIZE,
						"Get panel serial number failed, reason:%d",ret);
				msleep(20);
				continue;
			}

			panel_serial_info.reg_index = 10;

			panel_serial_info.year		= (read[0] >> 4) + 1;
			panel_serial_info.month		= read[0] & 0x0F;
			panel_serial_info.day		= read[1]	& 0x1F;
			panel_serial_info.hour		= read[2]	& 0x1F;
			panel_serial_info.minute	= read[3]	& 0x3F;
			panel_serial_info.second	= read[4]	& 0x3F;
			mtime = (read[5] >> 4) * 1000 + (read[5] & 0xF) * 100 + (read[6] >> 4) * 10 + (read[6] & 0x0F);
			panel_serial_info.reserved[0] = mtime >> 8;
			panel_serial_info.reserved[1] = mtime & 0xFF;
		} else {
			ret = dsi_display_read_panel_reg(get_main_display(), 0xA1, read, 16);
		}
		if(ret < 0) {
			ret = scnprintf(buf, PAGE_SIZE,
					"Get panel serial number failed, reason:%d",ret);
			msleep(20);
			continue;
		}

		if (read[0] == 0x09 && read[1] == 0x40 && read[2] == 0x10 && read[3] == 0x27) {
			/* 1-4th oppo code 09401027
			 * 5th vendor code/Month
			 * 6th Year/Day
			 * 7th Hour
			 * 8th Minute
			 * 9th Second
			 * 10-11th Second(ms)
			 */
			panel_serial_info.month		= read[4] & 0x0F;
			panel_serial_info.year		= ((read[5] & 0xE0) >> 5) + 7;
			panel_serial_info.day		= read[5] & 0x1F;
			panel_serial_info.hour		= read[6] & 0x17;
			panel_serial_info.minute	= read[7];
			panel_serial_info.second	= read[8];
			panel_serial_info.reserved[0] = read[9];
			panel_serial_info.reserved[1] = read[10];
		} else if(!strcmp(display->panel->name,"samsung amb655uv01 amoled fhd+ panel with DSC")
			|| !strcmp(display->panel->name,"samsung amb655uv01 amoled fhd+ panel with DSC 2nd")) {
			/*  0xA1               11th        12th    13th    14th    15th
			 *  HEX                0x32        0x0C    0x0B    0x29    0x37
			 *  Bit           [D7:D4][D3:D0] [D5:D0] [D5:D0] [D5:D0] [D5:D0]
			 *  exp              3      2       C       B       29      37
			 *  Yyyy,mm,dd      2014   2m      12d     11h     41min   55sec
			*/
			panel_serial_info.reg_index = 10;

			panel_serial_info.year		= (read[panel_serial_info.reg_index] & 0xF0) >> 0x4;
			panel_serial_info.month		= read[panel_serial_info.reg_index]	& 0x0F;
			panel_serial_info.day		= read[panel_serial_info.reg_index + 1]	& 0x1F;
			panel_serial_info.hour		= read[panel_serial_info.reg_index + 2]	& 0x1F;
			panel_serial_info.minute	= read[panel_serial_info.reg_index + 3]	& 0x3F;
			panel_serial_info.second	= read[panel_serial_info.reg_index + 4]	& 0x3F;
			panel_serial_info.reserved[0] = read[panel_serial_info.reg_index + 5];
			panel_serial_info.reserved[1] = read[panel_serial_info.reg_index + 6];
		}else if(!strcmp(display->panel->name,"samsung ams643xf01 amoled fhd+ panel")) {
			/*  0xA1               12th        13th    14th    15th    16th
			 *  HEX                0x32        0x0C    0x0B    0x29    0x37
			 *  Bit           [D7:D4][D3:D0] [D5:D0] [D5:D0] [D5:D0] [D5:D0]
			 *  exp              3      2       C       B       29      37
			 *  Yyyy,mm,dd      2014   2m      12d     11h     41min   55sec
			*/
			panel_serial_info.reg_index = 11;

			panel_serial_info.year		= (read[panel_serial_info.reg_index] & 0xF0) >> 0x4;
			panel_serial_info.month		= read[panel_serial_info.reg_index]	& 0x0F;
			panel_serial_info.day		= read[panel_serial_info.reg_index + 1]	& 0x1F;
			panel_serial_info.hour		= read[panel_serial_info.reg_index + 2]	& 0x1F;
			panel_serial_info.minute	= read[panel_serial_info.reg_index + 3]	& 0x3F;
			panel_serial_info.second	= read[panel_serial_info.reg_index + 4]	& 0x3F;
			panel_serial_info.reserved[0] = read[panel_serial_info.reg_index + 5];
			panel_serial_info.reserved[1] = read[panel_serial_info.reg_index + 6];
		}

		serial_number = (panel_serial_info.year		<< 56)\
			+ (panel_serial_info.month		<< 48)\
			+ (panel_serial_info.day		<< 40)\
			+ (panel_serial_info.hour		<< 32)\
			+ (panel_serial_info.minute	<< 24)\
			+ (panel_serial_info.second	<< 16)\
			+ (panel_serial_info.reserved[0] << 8)\
			+ (panel_serial_info.reserved[1]);

		if (!panel_serial_info.year) {
			/*
			 * the panel we use always large than 2011, so
			 * force retry when year is 2011
			 */
			msleep(20);
			continue;
		}

		ret = scnprintf(buf, PAGE_SIZE, "Get panel serial number: %llx\n",serial_number);
		break;
	}

	return ret;
}

static char oppo_rx_reg[PANEL_TX_MAX_BUF] = {0x0};
static char oppo_rx_len = 0;
static ssize_t oppo_display_get_panel_reg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	int i, cnt = 0;

	if (!display)
		return -EINVAL;
	mutex_lock(&display->display_lock);

	for (i = 0; i < oppo_rx_len; i++)
		cnt += snprintf(buf + cnt, PANEL_TX_MAX_BUF - cnt,
				"%02x ", oppo_rx_reg[i]);
	cnt += snprintf(buf + cnt, PANEL_TX_MAX_BUF - cnt, "\n");
	mutex_unlock(&display->display_lock);

	return cnt;
}

static uint64_t production_date_serial_number = 0;
int oppo_display_get_panel_production_date_serial_number(void)
{
	int ret = 0;
	unsigned char read[30];
	PANEL_SERIAL_INFO panel_serial_info;
	struct dsi_display *display = get_main_display();
	int i;

	if (!display) {
		printk(KERN_INFO "oppo_display_get_panel_serial_number and main display is null");
		return -1;
	}

	if (get_oppo_display_power_status() != OPPO_DISPLAY_POWER_ON) {
		printk(KERN_ERR"%s display panel in off status\n", __func__);
		return ret;
	}

	/*
	 * for some unknown reason, the panel_serial_info may read dummy,
	 * retry when found panel_serial_info is abnormal.
	 */
	for (i = 0;i < 10; i++) {
	if(!strcmp(display->panel->name,"samsung amb655uv01 amoled fhd+ panel with DSC")
			|| !strcmp(display->panel->name,"samsung amb655uv01 amoled fhd+ panel with DSC 2nd")) {
			ret = dsi_display_read_panel_reg(get_main_display(), 0xA1, read, 17);
		}else if (!strcmp(display->panel->name,"samsung ams643xf01 amoled fhd+ panel")) {
			ret = dsi_display_read_panel_reg(get_main_display(), 0xA1, read, 18);
		}else if (!strcmp(display->name,"qcom,mdss_dsi_oppo19101boe_nt37800_1080_2400_cmd")) {
			u32 mtime = 0;

			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);

			if (display->panel->panel_initialized) {

				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle,
							DSI_ALL_CLKS, DSI_CLK_ON);
				}
				{
					char value[] = { 0x55,0xAA,0x52,0x08,0x01 };
					ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xF0, value, sizeof(value));
				}
				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle,
							DSI_ALL_CLKS, DSI_CLK_OFF);
				}
				mutex_unlock(&display->panel->panel_lock);
				mutex_unlock(&display->display_lock);
			}
			if(ret < 0) {
				printk(KERN_ERR"%s Get panel serial number failed,reason:%d \n",__func__,ret);
				msleep(20);
				continue;
			}

			ret = dsi_display_read_panel_reg(display, 0xA2, read, 4);
			if(ret < 0) {
				printk(KERN_ERR"%s Get panel serial number failed,reason:%d \n",__func__,ret);
				msleep(20);
				continue;
			}

			pr_err("panel model: %2x %2x %2x %2x\n", read[0], read[1], read[2], read[3]);
			ret = dsi_display_read_panel_reg(display, 0xA3, read, 7);
			if(ret < 0) {
				printk(KERN_ERR"%s Get panel serial number failed,reason:%d \n",__func__,ret);
				msleep(20);
				continue;
			}

			panel_serial_info.reg_index = 10;

			panel_serial_info.year		= (read[0] >> 4) + 1;
			panel_serial_info.month		= read[0] & 0x0F;
			panel_serial_info.day		= read[1]	& 0x1F;
			panel_serial_info.hour		= read[2]	& 0x1F;
			panel_serial_info.minute	= read[3]	& 0x3F;
			panel_serial_info.second	= read[4]	& 0x3F;
			mtime = (read[5] >> 4) * 1000 + (read[5] & 0xF) * 100 + (read[6] >> 4) * 10 + (read[6] & 0x0F);
			panel_serial_info.reserved[0] = mtime >> 8;
			panel_serial_info.reserved[1] = mtime & 0xFF;
		} else {
			ret = dsi_display_read_panel_reg(get_main_display(), 0xA1, read, 16);
		}
		if(ret < 0) {
			printk(KERN_ERR"%s Get panel serial number failed,reason:%d \n",__func__,ret);
			msleep(20);
			continue;
		}

		if (read[0] == 0x09 && read[1] == 0x40 && read[2] == 0x10 && read[3] == 0x27) {
			/* 1-4th oppo code 09401027
			 * 5th vendor code/Month
			 * 6th Year/Day
			 * 7th Hour
			 * 8th Minute
			 * 9th Second
			 * 10-11th Second(ms)
			 */
			panel_serial_info.month		= read[4] & 0x0F;
			panel_serial_info.year		= ((read[5] & 0xE0) >> 5) + 7;
			panel_serial_info.day		= read[5] & 0x1F;
			panel_serial_info.hour		= read[6] & 0x17;
			panel_serial_info.minute	= read[7];
			panel_serial_info.second	= read[8];
			panel_serial_info.reserved[0] = read[9];
			panel_serial_info.reserved[1] = read[10];
		} else if(!strcmp(display->panel->name,"samsung amb655uv01 amoled fhd+ panel with DSC")
			|| !strcmp(display->panel->name,"samsung amb655uv01 amoled fhd+ panel with DSC 2nd")) {
			/*  0xA1               11th        12th    13th    14th    15th
			 *  HEX                0x32        0x0C    0x0B    0x29    0x37
			 *  Bit           [D7:D4][D3:D0] [D5:D0] [D5:D0] [D5:D0] [D5:D0]
			 *  exp              3      2       C       B       29      37
			 *  Yyyy,mm,dd      2014   2m      12d     11h     41min   55sec
			*/
			panel_serial_info.reg_index = 10;

			panel_serial_info.year		= (read[panel_serial_info.reg_index] & 0xF0) >> 0x4;
			panel_serial_info.month		= read[panel_serial_info.reg_index]	& 0x0F;
			panel_serial_info.day		= read[panel_serial_info.reg_index + 1]	& 0x1F;
			panel_serial_info.hour		= read[panel_serial_info.reg_index + 2]	& 0x1F;
			panel_serial_info.minute	= read[panel_serial_info.reg_index + 3]	& 0x3F;
			panel_serial_info.second	= read[panel_serial_info.reg_index + 4]	& 0x3F;
			panel_serial_info.reserved[0] = read[panel_serial_info.reg_index + 5];
			panel_serial_info.reserved[1] = read[panel_serial_info.reg_index + 6];
		}else if(!strcmp(display->panel->name,"samsung ams643xf01 amoled fhd+ panel")) {
			/*  0xA1               12th        13th    14th    15th    16th
			 *  HEX                0x32        0x0C    0x0B    0x29    0x37
			 *  Bit           [D7:D4][D3:D0] [D5:D0] [D5:D0] [D5:D0] [D5:D0]
			 *  exp              3      2       C       B       29      37
			 *  Yyyy,mm,dd      2014   2m      12d     11h     41min   55sec
			*/
			panel_serial_info.reg_index = 11;

			panel_serial_info.year		= (read[panel_serial_info.reg_index] & 0xF0) >> 0x4;
			panel_serial_info.month		= read[panel_serial_info.reg_index]	& 0x0F;
			panel_serial_info.day		= read[panel_serial_info.reg_index + 1]	& 0x1F;
			panel_serial_info.hour		= read[panel_serial_info.reg_index + 2]	& 0x1F;
			panel_serial_info.minute	= read[panel_serial_info.reg_index + 3]	& 0x3F;
			panel_serial_info.second	= read[panel_serial_info.reg_index + 4]	& 0x3F;
			panel_serial_info.reserved[0] = read[panel_serial_info.reg_index + 5];
			panel_serial_info.reserved[1] = read[panel_serial_info.reg_index + 6];
		}

		production_date_serial_number = (panel_serial_info.year		<< 56)\
			+ (panel_serial_info.month		<< 48)\
			+ (panel_serial_info.day		<< 40)\
			+ (panel_serial_info.hour		<< 32)\
			+ (panel_serial_info.minute	<< 24)\
			+ (panel_serial_info.second	<< 16)\
			+ (panel_serial_info.reserved[0] << 8)\
			+ (panel_serial_info.reserved[1]);

		if (!panel_serial_info.year) {
			/*
			 * the panel we use always large than 2011, so
			 * force retry when year is 2011
			 */
			msleep(20);
			continue;
		}
		break;
	}
	printk(KERN_ERR "Get panel production date serial number: %llx\n",production_date_serial_number);
	return ret;
}

static ssize_t oppo_display_get_serial_number(struct device *dev,
struct device_attribute *attr, char *buf) {
	if(production_date_serial_number == 0){
		printk(KERN_ERR"dsi_cmd %s Failed,Get panel serial number\n",__func__);
		return sprintf(buf, "%llx\n",production_date_serial_number);
	}
	printk(KERN_INFO "oppo_display_get_serial_number = %llx\n",production_date_serial_number);

	return scnprintf(buf, PAGE_SIZE, "%llx\n",production_date_serial_number);
}

static ssize_t oppo_display_set_panel_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	char reg[PANEL_TX_MAX_BUF] = {0x0};
	char payload[PANEL_TX_MAX_BUF] = {0x0};
	u32 index = 0, value = 0, step =0;
	int ret = 0;
	int len = 0;
	char *bufp = (char *)buf;
	struct dsi_display *display = get_main_display();
	char read;

	if (!display || !display->panel) {
		pr_err("debug for: %s %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (sscanf(bufp, "%c%n", &read, &step) && read == 'r') {
		bufp += step;
		sscanf(bufp, "%x %d", &value, &len);
		if (len > PANEL_TX_MAX_BUF) {
			pr_err("failed\n");
			return -EINVAL;
		}
		dsi_display_read_panel_reg(get_main_display(),value, reg, len);

		for (index; index < len; index++) {
			printk("%x ", reg[index]);
		}
		mutex_lock(&display->display_lock);
		memcpy(oppo_rx_reg, reg, PANEL_TX_MAX_BUF);
		oppo_rx_len = len;
		mutex_unlock(&display->display_lock);
		return count;
	}

	while (sscanf(bufp, "%x%n", &value, &step) > 0) {
		reg[len++] = value;
		if (len >= PANEL_TX_MAX_BUF) {
			pr_err("wrong input reg len\n");
			return -EFAULT;
		}
		bufp += step;
	}

	for(index; index < len; index ++ ) {
		payload[index] = reg[index + 1];
	}

	/* enable the clk vote for CMD mode panels */
	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (display->panel->panel_initialized) {
		if (display->config.panel_mode == DSI_OP_CMD_MODE) {
			dsi_display_clk_ctrl(display->dsi_clk_handle,
					DSI_ALL_CLKS, DSI_CLK_ON);
		}

		ret = mipi_dsi_dcs_write(&display->panel->mipi_device, reg[0],
				payload, len -1);

		if (display->config.panel_mode == DSI_OP_CMD_MODE) {
			dsi_display_clk_ctrl(display->dsi_clk_handle,
					DSI_ALL_CLKS, DSI_CLK_OFF);
		}
	}

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	if (ret < 0) {
		return ret;
	}

	return count;
}

static ssize_t oppo_display_get_panel_id(struct device *dev,
struct device_attribute *attr, char *buf) {
	struct dsi_display *display = get_main_display();
	int ret = 0;
	unsigned char read[30];
	char DA = 0;
	char DB = 0;
	char DC = 0;

	if(get_oppo_display_power_status() == OPPO_DISPLAY_POWER_ON) {
		if(display == NULL) {
			printk(KERN_INFO "oppo_display_get_panel_id and main display is null");
			ret = -1;
			return ret;
		}

		ret = dsi_display_read_panel_reg(display,0xDA, read, 1);
		if(ret < 0) {
			pr_err("failed to read DA ret=%d\n", ret);
			return -EINVAL;
		}
		DA = read[0];

		ret = dsi_display_read_panel_reg(display, 0xDB, read, 1);
		if(ret < 0) {
			pr_err("failed to read DA ret=%d\n", ret);
			return -EINVAL;
		}
		DB = read[0];

		ret = dsi_display_read_panel_reg(display,0xDC, read, 1);
		if(ret < 0) {
			pr_err("failed to read DA ret=%d\n", ret);
			return -EINVAL;
		}
		DC = read[0];
		ret = scnprintf(buf, PAGE_SIZE, "%02x %02x %02x\n", DA, DB, DC);
	} else {
		printk(KERN_ERR	 "%s oppo_display_get_panel_id, but now display panel status is not on\n", __func__);
	}
	return ret;
}

static ssize_t oppo_display_get_panel_dsc(struct device *dev,
struct device_attribute *attr, char *buf) {
	int ret = 0;
	unsigned char read[30];

	if(get_oppo_display_power_status() == OPPO_DISPLAY_POWER_ON) {
		if(get_main_display() == NULL) {
			printk(KERN_INFO "oppo_display_get_panel_dsc and main display is null");
			ret = -1;
			return ret;
		}

		ret = dsi_display_read_panel_reg(get_main_display(),0x03, read, 1);
		if(ret < 0) {
			ret = scnprintf(buf, PAGE_SIZE, "oppo_display_get_panel_dsc failed, reason:%d",ret);
		} else {
			ret = scnprintf(buf, PAGE_SIZE, "oppo_display_get_panel_dsc: 0x%x\n",read[0]);
		}
	} else {
		printk(KERN_ERR	 "%s oppo_display_get_panel_dsc, but now display panel status is not on\n", __func__);
	}
	return ret;
}

static ssize_t oppo_display_dump_info(struct device *dev,
struct device_attribute *attr, char *buf) {
	int ret = 0;
	struct dsi_display * temp_display;

	temp_display = get_main_display();

	if(temp_display == NULL ) {
		printk(KERN_INFO "oppo_display_dump_info and main display is null");
		ret = -1;
		return ret;
	}

	if(temp_display->modes == NULL) {
		printk(KERN_INFO "oppo_display_dump_info and display modes is null");
		ret = -1;
		return ret;
	}

	ret = scnprintf(buf , PAGE_SIZE, "oppo_display_dump_info: height =%d,width=%d,frame_rate=%d,clk_rate=%llu\n",
		temp_display->modes->timing.h_active,temp_display->modes->timing.v_active,
		temp_display->modes->timing.refresh_rate,temp_display->modes->timing.clk_rate_hz);

	return ret;
}

int __oppo_display_set_power_status(int status) {
	mutex_lock(&oppo_power_status_lock);
	if(status != oppo_request_power_status) {
		oppo_request_power_status = status;
	}
	mutex_unlock(&oppo_power_status_lock);
	return 0;
}
static ssize_t oppo_display_get_power_status(struct device *dev,
struct device_attribute *attr, char *buf) {

	printk(KERN_INFO "oppo_display_get_power_status = %d\n",get_oppo_display_power_status());

	return sprintf(buf, "%d\n", get_oppo_display_power_status());
}

static ssize_t oppo_display_set_power_status(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oppo_display_set_power_status = %d\n", __func__, temp_save);

	__oppo_display_set_power_status(temp_save);

	return count;
}

static ssize_t oppo_display_get_closebl_flag(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	printk(KERN_INFO "oppo_display_get_closebl_flag = %d\n",lcd_closebl_flag);
	return sprintf(buf, "%d\n", lcd_closebl_flag);
}

static ssize_t oppo_display_set_closebl_flag(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	int closebl = 0;
	sscanf(buf, "%du", &closebl);
	pr_err("lcd_closebl_flag = %d\n",closebl);
	if(1 != closebl)
		lcd_closebl_flag = 0;
	pr_err("oppo_display_set_closebl_flag = %d\n",lcd_closebl_flag);
	return count;
}

extern const char *cmd_set_prop_map[];
static ssize_t oppo_display_get_dsi_command(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i, cnt;

	cnt = snprintf(buf, PAGE_SIZE,
		"read current dsi_cmd:\n"
		"    echo dump > dsi_cmd  - then you can find dsi cmd on kmsg\n"
		"set sence dsi cmd:\n"
		"  example hbm on:\n"
		"    echo qcom,mdss-dsi-hbm-on-command > dsi_cmd\n"
		"    echo [dsi cmd0] > dsi_cmd\n"
		"    echo [dsi cmd1] > dsi_cmd\n"
		"    echo [dsi cmdX] > dsi_cmd\n"
		"    echo flush > dsi_cmd\n"
		"available dsi_cmd sences:\n");
	for (i = 0; i < DSI_CMD_SET_MAX; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"    %s\n", cmd_set_prop_map[i]);

	return cnt;
}
static int oppo_display_dump_dsi_command(struct dsi_display *display)
{
	struct dsi_display_mode *mode;
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_panel_cmd_set *cmd_sets;
	enum dsi_cmd_set_state state;
	struct dsi_cmd_desc *cmds;
	const char *cmd_name;
	int i, j, k, cnt = 0;
	const u8 *tx_buf;
	char bufs[SZ_256];

	if (!display || !display->panel || !display->panel->cur_mode) {
		pr_err("failed to get main dsi display\n");
		return -EFAULT;
	}

	mode = display->panel->cur_mode;
	if (!mode || !mode->priv_info) {
		pr_err("failed to get dsi display mode\n");
		return -EFAULT;
	}

	priv_info = mode->priv_info;
	cmd_sets = priv_info->cmd_sets;

	for (i = 0; i < DSI_CMD_SET_MAX; i++) {
		cmd_name = cmd_set_prop_map[i];
		if (!cmd_name)
			continue;
		state = cmd_sets[i].state;
		pr_err("%s: %s", cmd_name, state == DSI_CMD_SET_STATE_LP ?
				"dsi_lp_mode" : "dsi_hs_mode");

		for (j = 0; j < cmd_sets[i].count; j++) {
			cmds = &cmd_sets[i].cmds[j];
			tx_buf = cmds->msg.tx_buf;
			cnt = snprintf(bufs, SZ_256,
				" %02x %02x %02x %02x %02x %02x %02x",
				cmds->msg.type, cmds->last_command,
				cmds->msg.channel,
				cmds->msg.flags == MIPI_DSI_MSG_REQ_ACK,
				cmds->post_wait_ms,
				(int)(cmds->msg.tx_len >> 8),
				(int)(cmds->msg.tx_len & 0xff));
			for (k = 0; k < cmds->msg.tx_len; k++)
				cnt += snprintf(bufs + cnt,
						SZ_256 > cnt ? SZ_256 - cnt : 0,
						" %02x", tx_buf[k]);
			pr_err("%s", bufs);
		}
	}

	return 0;
}

static int oppo_dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt)
{
	const u32 cmd_set_min_size = 7;
	u32 count = 0;
	u32 packet_length;
	u32 tmp;

	while (length >= cmd_set_min_size) {
		packet_length = cmd_set_min_size;
		tmp = ((data[5] << 8) | (data[6]));
		packet_length += tmp;
		if (packet_length > length) {
			pr_err("format error packet_length[%d] length[%d] count[%d]\n",
				packet_length, length, count);
			return -EINVAL;
		}
		length -= packet_length;
		data += packet_length;
		count++;
	};

	*cnt = count;
	return 0;
}

static int oppo_dsi_panel_create_cmd_packets(const char *data,
					     u32 length,
					     u32 count,
					     struct dsi_cmd_desc *cmd)
{
	int rc = 0;
	int i, j;
	u8 *payload;

	for (i = 0; i < count; i++) {
		u32 size;

		cmd[i].msg.type = data[0];
		cmd[i].last_command = (data[1] == 1 ? true : false);
		cmd[i].msg.channel = data[2];
		cmd[i].msg.flags |= (data[3] == 1 ? MIPI_DSI_MSG_REQ_ACK : 0);
		cmd[i].msg.ctrl = 0;
		cmd[i].post_wait_ms = data[4];
		cmd[i].msg.tx_len = ((data[5] << 8) | (data[6]));

		size = cmd[i].msg.tx_len * sizeof(u8);

		payload = kzalloc(size, GFP_KERNEL);
		if (!payload) {
			rc = -ENOMEM;
			goto error_free_payloads;
		}

		for (j = 0; j < cmd[i].msg.tx_len; j++)
			payload[j] = data[7 + j];

		cmd[i].msg.tx_buf = payload;
		data += (7 + cmd[i].msg.tx_len);
	}

	return rc;
error_free_payloads:
	for (i = i - 1; i >= 0; i--) {
		cmd--;
		kfree(cmd->msg.tx_buf);
	}

	return rc;
}

static ssize_t oppo_display_set_dsi_command(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	struct dsi_display_mode *mode;
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_panel_cmd_set *cmd_sets;
	char *bufp = (char *)buf;
	struct dsi_cmd_desc *cmds;
	struct dsi_panel_cmd_set *cmd;
	static char *cmd_bufs;
	static int cmd_counts;
	static u32 oppo_dsi_command = DSI_CMD_SET_MAX;
	static int oppo_dsi_state = DSI_CMD_SET_STATE_HS;
	u32 old_dsi_command = oppo_dsi_command;
	u32 packet_count = 0, size;
	int rc = count, i;
	char data[SZ_256];
	bool flush = false;

	if (!cmd_bufs) {
		cmd_bufs = kmalloc(SZ_4K, GFP_KERNEL);
		if (!cmd_bufs)
			return -ENOMEM;
	}
	sscanf(buf, "%s", data);
	if (!strcmp("dump", data)) {
		rc = oppo_display_dump_dsi_command(display);
		if (rc < 0)
			return rc;
		return count;
	} else if (!strcmp("flush", data)) {
		flush = true;
	} else if (!strcmp("dsi_hs_mode", data)) {
		oppo_dsi_state = DSI_CMD_SET_STATE_HS;
	} else if (!strcmp("dsi_lp_mode", data)) {
		oppo_dsi_state = DSI_CMD_SET_STATE_LP;
	} else {
		for (i = 0; i < DSI_CMD_SET_MAX; i++) {
			if (!strcmp(cmd_set_prop_map[i], data)) {
				oppo_dsi_command = i;
				flush = true;
				break;
			}
		}
	}

	if (!flush) {
		u32 value = 0, step = 0;

		while (sscanf(bufp, "%x%n", &value, &step) > 0) {
			if (value > 0xff) {
				pr_err("input reg don't large than 0xff\n");
				return -EINVAL;
			}
			cmd_bufs[cmd_counts++] = value;
			if (cmd_counts >= SZ_4K) {
				pr_err("wrong input reg len\n");
				cmd_counts = 0;
				return -EFAULT;
			}
			bufp += step;
		}
		return count;
	}

	if (!cmd_counts)
		return rc;
	if (old_dsi_command >= DSI_CMD_SET_MAX) {
		pr_err("UnSupport dsi command set\n");
		goto error;
	}

	if (!display || !display->panel || !display->panel->cur_mode) {
		pr_err("failed to get main dsi display\n");
		rc = -EFAULT;
		goto error;
	}

	mode = display->panel->cur_mode;
	if (!mode || !mode->priv_info) {
		pr_err("failed to get dsi display mode\n");
		rc = -EFAULT;
		goto error;
	}

	priv_info = mode->priv_info;
	cmd_sets = priv_info->cmd_sets;

	cmd = &cmd_sets[old_dsi_command];

	rc = oppo_dsi_panel_get_cmd_pkt_count(cmd_bufs, cmd_counts,
			&packet_count);
	if (rc) {
		pr_err("commands failed, rc=%d\n", rc);
		goto error;
	}

	size = packet_count * sizeof(*cmd->cmds);

	cmds = kzalloc(size, GFP_KERNEL);
	if (!cmds) {
		rc = -ENOMEM;
		goto error;
	}

	rc = oppo_dsi_panel_create_cmd_packets(cmd_bufs, cmd_counts,
			packet_count, cmds);
	if (rc) {
		pr_err("failed to create cmd packets, rc=%d\n", rc);
		goto error_free_cmds;
	}

	mutex_lock(&display->panel->panel_lock);

	kfree(cmd->cmds);
	cmd->cmds = cmds;
	cmd->count = packet_count;
	if (oppo_dsi_state == DSI_CMD_SET_STATE_LP)
		cmd->state = DSI_CMD_SET_STATE_LP;
	else if (oppo_dsi_state == DSI_CMD_SET_STATE_LP)
		cmd->state = DSI_CMD_SET_STATE_HS;

	mutex_unlock(&display->panel->panel_lock);

	cmd_counts = 0;
	oppo_dsi_state = DSI_CMD_SET_STATE_HS;

	return count;

error_free_cmds:
	kfree(cmds);
error:
	cmd_counts = 0;
	oppo_dsi_state = DSI_CMD_SET_STATE_HS;

	return rc;
}

int oppo_panel_alpha = 0;
struct oppo_brightness_alpha *oppo_brightness_alpha_lut = NULL;
struct oppo_brightness_alpha brightness_alpha_lut[] = {
	{0, 0xff},
	{1, 0xee},
	{2, 0xe8},
	{3, 0xe6},
	{4, 0xe5},
	{6, 0xe4},
	{10, 0xe0},
	{20, 0xd5},
	{30, 0xce},
	{45, 0xc6},
	{70, 0xb7},
	{100, 0xad},
	{150, 0xa0},
	{227, 0x8a},
	{300, 0x80},
	{400, 0x6e},
	{500, 0x5b},
	{600, 0x50},
	{800, 0x38},
	{1023, 0x18},
};

struct oppo_brightness_alpha brightness_seed_alpha_lut_dc[] = {
	{0, 0xff},
	{1, 0xfc},
	{2, 0xfb},
	{3, 0xfa},
	{4, 0xf9},
	{5, 0xf8},
	{6, 0xf7},
	{8, 0xf6},
	{10, 0xf4},
	{15, 0xf0},
	{20, 0xea},
	{30, 0xe0},
	{45, 0xd0},
	{70, 0xbc},
	{100, 0x98},
	{120, 0x80},
	{140, 0x70},
	{160, 0x58},
	{180, 0x48},
	{200, 0x30},
	{220, 0x20},
	{240, 0x10},
	{260, 0x00},
};

struct oppo_brightness_alpha brightness_alpha_lut_dc[] = {
	{0, 0xff},
	{1, 0xE0},
	{2, 0xd6},
	{3, 0xd5},
	{4, 0xcf},
	{5, 0xcb},
	{6, 0xc9},
	{8, 0xc5},
	{10, 0xc1},
	{15, 0xb6},
	{20, 0xac},
	{30, 0x9d},
	{45, 0x82},
	{70, 0x6c},
	{100, 0x56},
	{120, 0x47},
	{140, 0x3a},
	{160, 0x2f},
	{180, 0x22},
	{200, 0x16},
	{220, 0xe},
	{240, 0x6},
	{260, 0x00},
};

static int interpolate(int x, int xa, int xb, int ya, int yb)
{
	int bf, factor, plus;
	int sub = 0;

	bf = 2 * (yb - ya) * (x - xa) / (xb - xa);
	factor = bf / 2;
	plus = bf % 2;
	if ((xa - xb) && (yb - ya))
		sub = 2 * (x - xa) * (x - xb) / (yb - ya) / (xa - xb);

	return ya + factor + plus + sub;
}

int oppo_seed_bright_to_alpha(int brightness)
{
	int level = ARRAY_SIZE(brightness_seed_alpha_lut_dc);
	int i = 0;
	int alpha;

	if (oppo_panel_alpha)
		return oppo_panel_alpha;
	for (i = 0; i < ARRAY_SIZE(brightness_seed_alpha_lut_dc); i++){
		if (brightness_seed_alpha_lut_dc[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		alpha = brightness_seed_alpha_lut_dc[0].alpha;
	else if (i == level)
		alpha = brightness_seed_alpha_lut_dc[level - 1].alpha;
	else
		alpha = interpolate(brightness,
			brightness_seed_alpha_lut_dc[i-1].brightness,
			brightness_seed_alpha_lut_dc[i].brightness,
			brightness_seed_alpha_lut_dc[i-1].alpha,
			brightness_seed_alpha_lut_dc[i].alpha);

	return alpha;
}

int bl_to_alpha(int brightness)
{
	struct dsi_display *display = get_main_display();
	struct oppo_brightness_alpha *lut = NULL;
	int count = 0;
	int i = 0;
	int alpha;

	if (!display)
		return 0;

	if (display->panel->ba_seq && display->panel->ba_count) {
		count = display->panel->ba_count;
		lut = display->panel->ba_seq;
	} else {
		count = ARRAY_SIZE(brightness_alpha_lut);
		lut = brightness_alpha_lut;
	}

	for (i = 0; i < count; i++){
		if (lut[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		alpha = lut[0].alpha;
	else if (i == count)
		alpha = lut[count - 1].alpha;
	else
		alpha = interpolate(brightness, lut[i-1].brightness,
				    lut[i].brightness, lut[i-1].alpha,
				    lut[i].alpha);

	return alpha;
}

int bl_to_alpha_dc(int brightness)
{
	int level = ARRAY_SIZE(brightness_alpha_lut_dc);
	int i = 0;
	int alpha;

	for (i = 0; i < ARRAY_SIZE(brightness_alpha_lut_dc); i++){
		if (brightness_alpha_lut_dc[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		alpha = brightness_alpha_lut_dc[0].alpha;
	else if (i == level)
		alpha = brightness_alpha_lut_dc[level - 1].alpha;
	else
		alpha = interpolate(brightness,
			brightness_alpha_lut_dc[i-1].brightness,
			brightness_alpha_lut_dc[i].brightness,
			brightness_alpha_lut_dc[i-1].alpha,
			brightness_alpha_lut_dc[i].alpha);
	return alpha;
}

int oppo_dimlayer_hbm = 0;
extern int oppo_last_backlight;
int brightness_to_alpha(int brightness)
{
	int alpha;

	if (brightness == 0 || brightness == 1)
		brightness = oppo_last_backlight;

	if (oppo_dimlayer_hbm)
		alpha = bl_to_alpha(brightness);
	else
		alpha = bl_to_alpha_dc(brightness);

	return alpha;
}

int oppo_get_panel_brightness(void)
{
	struct dsi_display *display = get_main_display();

	if (!display)
		return 0;

	return display->panel->bl_config.bl_level;
}

extern bool oppo_ffl_trigger_finish;
int oppo_get_panel_brightness_to_alpha(void)
{
	struct dsi_display *display = get_main_display();

	if (!display)
		return 0;
	if (oppo_panel_alpha)
		return oppo_panel_alpha;

	if (hbm_mode)
		return 0;

	if (!oppo_ffl_trigger_finish)
		return brightness_to_alpha(FFL_FP_LEVEL);

	return brightness_to_alpha(display->panel->bl_config.bl_level);
}

static ssize_t oppo_display_get_dim_alpha(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();

	if (!display->panel->is_hbm_enabled ||
	    (get_oppo_display_power_status() != OPPO_DISPLAY_POWER_ON))
		return sprintf(buf, "%d\n", 0);

	return sprintf(buf, "%d\n", oppo_underbrightness_alpha);
}

static ssize_t oppo_display_set_dim_alpha(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oppo_panel_alpha);

	return count;
}

static ssize_t oppo_display_get_dc_dim_alpha(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct dsi_display *display = get_main_display();

	if (display->panel->is_hbm_enabled ||
	    get_oppo_display_power_status() != OPPO_DISPLAY_POWER_ON)
		ret = 0;
	if(oppo_dc2_alpha != 0) {
		ret = oppo_dc2_alpha;
	} else if(oppo_underbrightness_alpha != 0){
		ret = oppo_underbrightness_alpha;
	} else if (oppo_dimlayer_bl_enable_v3_real) {
		ret = 1;
	}

	return sprintf(buf, "%d\n", ret);
}

static ssize_t oppo_display_get_dimlayer_backlight(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d %d %d %d %d %d\n", oppo_dimlayer_bl_alpha,
			oppo_dimlayer_bl_alpha_value, oppo_dimlayer_dither_threshold,
			oppo_dimlayer_dither_bitdepth, oppo_dimlayer_bl_delay, oppo_dimlayer_bl_delay_after);
}

static ssize_t oppo_display_set_dimlayer_backlight(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%d %d %d %d %d %d", &oppo_dimlayer_bl_alpha,
		&oppo_dimlayer_bl_alpha_value, &oppo_dimlayer_dither_threshold,
		&oppo_dimlayer_dither_bitdepth, &oppo_dimlayer_bl_delay,
		&oppo_dimlayer_bl_delay_after);

	return count;
}

int oppo_fod_on_vblank = -1;
int oppo_fod_off_vblank = -1;
static int oppo_datadimming_v3_debug_value = -1;
static int oppo_datadimming_v3_debug_delay = 16000;
static ssize_t oppo_display_get_debug(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d %d %d %d\n", oppo_fod_on_vblank, oppo_fod_off_vblank, oppo_datadimming_v3_debug_value, oppo_datadimming_v3_debug_delay);
}

static ssize_t oppo_display_set_debug(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%d %d %d %d", &oppo_fod_on_vblank, &oppo_fod_off_vblank, &oppo_datadimming_v3_debug_value, &oppo_datadimming_v3_debug_delay);

	return count;
}


static ssize_t oppo_display_get_dimlayer_enable(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oppo_dimlayer_bl_enable_v2);
}


int oppo_datadimming_vblank_count = 0;
atomic_t oppo_datadimming_vblank_ref = ATOMIC_INIT(0);
static int oppo_boe_data_dimming_process_unlock(int brightness, int enable)
{
	struct dsi_display *display = get_main_display();
	struct drm_connector *dsi_connector = display->drm_conn;
	struct dsi_panel *panel = display->panel;
	struct mipi_dsi_device *mipi_device;
	int rc = 0;

	if (!panel) {
		pr_err("failed to find display panel\n");
		return -EINVAL;
	}

	if (!dsi_panel_initialized(panel)) {
		pr_err("dsi_panel_aod_low_light_mode is not init\n");
		return -EINVAL;
	}

	mipi_device = &panel->mipi_device;

	if (!panel->is_hbm_enabled && oppo_datadimming_vblank_count != 0) {
		drm_crtc_wait_one_vblank(dsi_connector->state->crtc);
		rc = mipi_dsi_dcs_set_display_brightness(mipi_device, brightness);
		drm_crtc_wait_one_vblank(dsi_connector->state->crtc);
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_ALL_CLKS, DSI_CLK_ON);
	}

	if (enable) {
		rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_DATA_DIMMING_ON);
	} else {
		rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_DATA_DIMMING_OFF);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_ALL_CLKS, DSI_CLK_OFF);
	}

	return rc;
}

struct oppo_brightness_alpha brightness_remapping[] = {
	{0, 0},
	{1, 1},
	{2, 155},
	{3, 160},
	{5, 165},
	{6, 170},
	{8, 175},
	{10, 180},
	{20, 190},
	{40, 230},
	{80, 270},
	{100, 300},
	{150, 400},
	{200, 600},
	{300, 800},
	{400, 1000},
	{600, 1250},
	{800, 1400},
	{1000, 1500},
	{1200, 1650},
	{1400, 1800},
	{1600, 1900},
	{1780, 2000},
	{2047, 2047},
};

int oppo_backlight_remapping(int brightness)
{
	struct oppo_brightness_alpha *lut = brightness_remapping;
	int count = ARRAY_SIZE(brightness_remapping);
	int i = 0;
	int bl_lvl = brightness;

	if (oppo_datadimming_v3_debug_value >=0)
		return oppo_datadimming_v3_debug_value;

	for (i = 0; i < count; i++){
		if (lut[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		bl_lvl = lut[0].alpha;
	else if (i == count)
		bl_lvl = lut[count - 1].alpha;
	else
		bl_lvl = interpolate(brightness, lut[i-1].brightness,
				    lut[i].brightness, lut[i-1].alpha,
				    lut[i].alpha);

	return bl_lvl;
}

int oppo_panel_process_dimming_v3(struct dsi_panel *panel, int brightness)
{
	static bool dimming_enable = false;
	bool enable = oppo_dimlayer_bl_enable_v3_real;
	int ret = 0;
	int bl_lvl = brightness;

	if (brightness <= 1)
		enable = false;

	if (enable)
		bl_lvl = oppo_backlight_remapping(brightness);

	if (enable != dimming_enable) {
		if (enable) {
			ret = oppo_boe_data_dimming_process_unlock(bl_lvl, true);
			if (ret) {
				pr_err("failed to enable data dimming\n");
				goto error;
			}
			dimming_enable = true;
			pr_err("Enter DC backlight v3\n");
		} else {
			ret = oppo_boe_data_dimming_process_unlock(bl_lvl, false);
			if (ret) {
				pr_err("failed to enable data dimming\n");
				goto error;
			}
			dimming_enable = false;
			pr_err("Exit DC backlight v3\n");
		}
	}

error:
	if (dimming_enable)
		return bl_lvl;

	return brightness;
}

extern int oppo_seed_backlight;
extern int oppo_dimlayer_bl_enable_v2_real;
extern bool oppo_skip_datadimming_sync;
static bool oppo_datadimming_v2_need_flush = false;
static bool oppo_datadimming_v2_need_sync = false;
void oppo_panel_process_dimming_v2_post(struct dsi_panel *panel, bool force_disable)
{
	struct dsi_display *display = get_main_display();
	struct drm_connector *dsi_connector = display->drm_conn;

	if (oppo_datadimming_v2_need_flush) {
		if (oppo_datadimming_v2_need_sync &&
		    (!strcmp(panel->oppo_priv.vendor_name,"AMB655UV01") ||
			 !strcmp(panel->oppo_priv.vendor_name,"AMS643XF01")) &&
		    dsi_connector && dsi_connector->state && dsi_connector->state->crtc) {
			struct drm_crtc *crtc = dsi_connector->state->crtc;
			int frame_time_us, ret = 0;
			u32 current_vblank;

			frame_time_us = mult_frac(1000, 1000, panel->cur_mode->timing.refresh_rate);

			current_vblank = drm_crtc_vblank_count(crtc);
			ret = wait_event_timeout(*drm_crtc_vblank_waitqueue(crtc),
					current_vblank != drm_crtc_vblank_count(crtc),
					usecs_to_jiffies(frame_time_us + 1000));
			if (!ret)
				pr_err("%s: crtc wait_event_timeout \n", __func__);

		}

		if (display->config.panel_mode == DSI_OP_VIDEO_MODE)
			panel->oppo_priv.skip_mipi_last_cmd = true;
		dsi_panel_seed_mode_unlock(panel, seed_mode);
		if (display->config.panel_mode == DSI_OP_VIDEO_MODE)
			panel->oppo_priv.skip_mipi_last_cmd = false;
		oppo_datadimming_v2_need_flush = false;
	}
}

int oppo_panel_process_dimming_v2(struct dsi_panel *panel, int bl_lvl, bool force_disable)
{
	struct dsi_display *display = get_main_display();
	struct drm_connector *dsi_connector = display->drm_conn;

	oppo_datadimming_v2_need_flush = false;
	oppo_datadimming_v2_need_sync = false;
	if (!force_disable && oppo_dimlayer_bl_enable_v2_real &&
	    bl_lvl > 1 && bl_lvl < 260) {
		if (!oppo_seed_backlight) {
			pr_err("Enter DC backlight v2\n");
			oppo_dc_v2_on = true;
			if (!oppo_skip_datadimming_sync &&
			    oppo_last_backlight != 0 &&
			    oppo_last_backlight != 1)
				oppo_datadimming_v2_need_sync = true;
		}
		oppo_seed_backlight = bl_lvl;
		bl_lvl = oppo_dimlayer_bl_alpha;
		oppo_datadimming_v2_need_flush = true;
	} else if (oppo_seed_backlight) {
		pr_err("Exit DC backlight v2\n");
		oppo_dc_v2_on = false;
		oppo_seed_backlight = 0;
		oppo_dc2_alpha = 0;
		oppo_datadimming_v2_need_flush = true;
		oppo_datadimming_v2_need_sync = true;
	}

	if (oppo_datadimming_v2_need_flush) {
		if (oppo_datadimming_v2_need_sync &&
		    (!strcmp(panel->oppo_priv.vendor_name,"AMB655UV01") ||
			 !strcmp(panel->oppo_priv.vendor_name,"AMS643XF01")) &&
		    dsi_connector && dsi_connector->state && dsi_connector->state->crtc) {
			struct drm_crtc *crtc = dsi_connector->state->crtc;
			int frame_time_us, ret = 0;
			u32 current_vblank;

			frame_time_us = mult_frac(1000, 1000, panel->cur_mode->timing.refresh_rate);

			current_vblank = drm_crtc_vblank_count(crtc);
			ret = wait_event_timeout(*drm_crtc_vblank_waitqueue(crtc),
					current_vblank != drm_crtc_vblank_count(crtc),
					usecs_to_jiffies(frame_time_us + 1000));
			if (!ret)
				pr_err("%s: crtc wait_event_timeout \n", __func__);

		}
	}
	if (oppo_datadimming_v2_need_flush && strcmp(panel->oppo_priv.vendor_name,"AMB655UV01"))
		oppo_panel_process_dimming_v2_post(panel, force_disable);

	return bl_lvl;
}

int oppo_panel_update_backlight_unlock(struct dsi_panel *panel)
{
	return dsi_panel_set_backlight(panel, panel->bl_config.bl_level);
}

static ssize_t oppo_display_set_dimlayer_enable(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	struct drm_connector *dsi_connector = display->drm_conn;

	if (display && display->name) {
		int enable = 0;
		int err = 0;

		sscanf(buf, "%d", &enable);
		mutex_lock(&display->display_lock);
		if (!dsi_connector || !dsi_connector->state || !dsi_connector->state->crtc) {
			pr_err("[%s]: display not ready\n", __func__);
		}else {
			err = drm_crtc_vblank_get(dsi_connector->state->crtc);
			if (err) {
				pr_err("failed to get crtc vblank, error=%d\n", err);
			} else {
				/* do vblank put after 7 frames */
				oppo_datadimming_vblank_count= 7;
				atomic_inc(&oppo_datadimming_vblank_ref);
			}
		}

		usleep_range(17000, 17100);
		if (!strcmp(display->name,"qcom,mdss_dsi_oppo19101boe_nt37800_1080_2400_cmd"))
			oppo_dimlayer_bl_enable_v3 = enable;
		else
			oppo_dimlayer_bl_enable_v2 = enable;

		mutex_unlock(&display->display_lock);
	}

	return count;
}

static ssize_t oppo_display_get_dimlayer_hbm(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oppo_dimlayer_hbm);
}

int oppo_dimlayer_hbm_vblank_count = 0;
atomic_t oppo_dimlayer_hbm_vblank_ref = ATOMIC_INIT(0);
static ssize_t oppo_display_set_dimlayer_hbm(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	struct drm_connector *dsi_connector = display->drm_conn;
	int err = 0;
	int value = 0;

	sscanf(buf, "%d", &value);
	value = !!value;
	if (oppo_dimlayer_hbm == value)
		return count;
	if (!dsi_connector || !dsi_connector->state || !dsi_connector->state->crtc) {
		pr_err("[%s]: display not ready\n", __func__);
	} else {
		err = drm_crtc_vblank_get(dsi_connector->state->crtc);
		if (err) {
			pr_err("failed to get crtc vblank, error=%d\n", err);
		} else {
			/* do vblank put after 5 frames */
			oppo_dimlayer_hbm_vblank_count = 5;
			atomic_inc(&oppo_dimlayer_hbm_vblank_ref);
		}
	}
	oppo_dimlayer_hbm = value;

#ifdef VENDOR_EDIT
/* Hu Jie@PSW.MM.Display.Lcd.Stability, 2019-09-27, add log at display key evevnt */
	pr_err("debug for oppo_display_set_dimlayer_hbm set oppo_dimlayer_hbm = %d\n",oppo_dimlayer_hbm);
#endif

	return count;
}

int oppo_force_screenfp = 0;
static ssize_t oppo_display_get_forcescreenfp(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oppo_force_screenfp);
}

static ssize_t oppo_display_set_forcescreenfp(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oppo_force_screenfp);

	return count;
}

static ssize_t oppo_display_get_esd_status(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	int rc = 0;

	return rc;

	if (!display)
		return -ENODEV;
	mutex_lock(&display->display_lock);

	if (!display->panel) {
		rc = -EINVAL;
		goto error;
	}

	rc = sprintf(buf, "%d\n", display->panel->esd_config.esd_enabled);

error:
	mutex_unlock(&display->display_lock);
	return rc;
}

static ssize_t oppo_display_set_esd_status(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	int enable = 0;

	return count;

	sscanf(buf, "%du", &enable);

#ifdef VENDOR_EDIT
/* Hu Jie@PSW.MM.Display.Lcd.Stability, 2019-09-27, add log at display key evevnt */
	pr_err("debug for oppo_display_set_esd_status, the enable value = %d\n", enable);
#endif

	if (!display)
		return -ENODEV;

	if (!display->panel || !display->drm_conn) {
		return -EINVAL;
	}

	if (!enable) {
		if (display->panel->esd_config.esd_enabled)
		{
			sde_connector_schedule_status_work(display->drm_conn, false);
			display->panel->esd_config.esd_enabled = false;
			pr_err("disable esd work");
		}
	}

	return count;
}

static ssize_t oppo_display_notify_panel_blank(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {

	struct msm_drm_notifier notifier_data;
	int blank;
	int temp_save = 0;

	sscanf(buf, "%du", &temp_save);
	printk(KERN_INFO "%s oppo_display_notify_panel_blank = %d\n", __func__, temp_save);

	if(temp_save == 1) {
		blank = MSM_DRM_BLANK_UNBLANK;
		notifier_data.data = &blank;
		notifier_data.id = 0;
		msm_drm_notifier_call_chain(MSM_DRM_EARLY_EVENT_BLANK,
						   &notifier_data);
		msm_drm_notifier_call_chain(MSM_DRM_EVENT_BLANK,
						   &notifier_data);
	} else if (temp_save == 0) {
		blank = MSM_DRM_BLANK_POWERDOWN;
		notifier_data.data = &blank;
		notifier_data.id = 0;
		msm_drm_notifier_call_chain(MSM_DRM_EARLY_EVENT_BLANK,
						   &notifier_data);
	}
	return count;
}

#define FFL_LEVEL_START 2
#define FFL_LEVEL_END  236
#define FFLUPRARE  1
#define BACKUPRATE 6
#define FFL_PENDING_END 600
#define FFL_EXIT_CONTROL 0
#define FFL_TRIGGLE_CONTROL 1
#define FFL_EXIT_FULLY_CONTROL 2

bool ffl_work_running = false;
bool oppo_ffl_trigger_finish = true;
static int is_ffl_enable = FFL_EXIT_CONTROL;
struct task_struct *oppo_ffl_thread;
struct kthread_worker oppo_ffl_worker;
struct kthread_work oppo_ffl_work;
static DEFINE_MUTEX(oppo_ffl_lock);

static ssize_t oppo_get_ffl_setting(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_ffl_enable);
}

static ssize_t oppo_set_ffl_setting(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int enable = 0;
	unsigned char payload[150] = "";

	sscanf(buf, "%du", &enable);
	printk(KERN_INFO "%s oppo_set_ffl_setting = %d\n", __func__, enable);

	mutex_lock(&oppo_ffl_lock);

	if(enable != is_ffl_enable) {
		pr_debug("set_ffl_setting need change is_ffl_enable\n");
		is_ffl_enable = enable;
		if ((is_ffl_enable ==FFL_TRIGGLE_CONTROL) && ffl_work_running){
			oppo_ffl_trigger_finish = false;
			kthread_queue_work(&oppo_ffl_worker, &oppo_ffl_work);
		}
	}

	mutex_unlock(&oppo_ffl_lock);

	if ((is_ffl_enable ==FFL_TRIGGLE_CONTROL) && ffl_work_running) {
		scnprintf(payload, sizeof(payload), "NULL$$EventID@@%d$$fflset@@%d",OPPO_MM_DIRVER_FB_EVENT_ID_FFLSET,enable);
		upload_mm_kevent_fb_data(OPPO_MM_DIRVER_FB_EVENT_MODULE_DISPLAY,payload);
	}

	return count;
}

static void oppo_ffl_setting_thread(struct kthread_work *work)
{
	struct dsi_display *display = get_main_display();
	int index =0;
	int pending =0;
	int system_backlight_target;
	int rc;

	if (get_oppo_display_power_status() == OPPO_DISPLAY_POWER_OFF)
		return;

	if (is_ffl_enable != FFL_TRIGGLE_CONTROL)
		return;

	if (!display || !display->panel) {
		pr_err("failed to find display panel \n");
		return;
	}

	if (!ffl_work_running)
		return;

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
				display->name, rc);
		return;
	}

	for(index = FFL_LEVEL_START;index < FFL_LEVEL_END;index = index + FFLUPRARE ) {
		if((is_ffl_enable ==FFL_EXIT_CONTROL) ||
		   (is_ffl_enable ==FFL_EXIT_FULLY_CONTROL) ||
		   !ffl_work_running)
			break;
		/*
		 * On Onscreenfingerprint mode, max backlight level should be FFL_FP_LEVEL
		 */
		if (display->panel->is_hbm_enabled && index > FFL_FP_LEVEL)
			break;

		mutex_lock(&display->panel->panel_lock);
		dsi_panel_set_backlight(display->panel, index);
		mutex_unlock(&display->panel->panel_lock);
		usleep_range(1000, 1100);
	}

	for(pending =0; pending <= FFL_PENDING_END; pending++)
	{
		if((is_ffl_enable ==FFL_EXIT_CONTROL) ||
		   (is_ffl_enable ==FFL_EXIT_FULLY_CONTROL) ||
		   !ffl_work_running)
			break;
		usleep_range(8000, 8100);
	}

	system_backlight_target = display->panel->bl_config.bl_level;

	if(index < system_backlight_target) {
		for(index; index < system_backlight_target; index =index + BACKUPRATE) {
			if((is_ffl_enable ==FFL_EXIT_FULLY_CONTROL) ||
			   !ffl_work_running)
				break;
			mutex_lock(&display->panel->panel_lock);
			dsi_panel_set_backlight(display->panel, index);
			mutex_unlock(&display->panel->panel_lock);
			usleep_range(6000, 6100);
		}
	} else if (index > system_backlight_target) {
		for(index; index > system_backlight_target; index =index - BACKUPRATE) {
			if((is_ffl_enable ==FFL_EXIT_FULLY_CONTROL) ||
			   !ffl_work_running)
				break;
			mutex_lock(&display->panel->panel_lock);
			dsi_panel_set_backlight(display->panel, index);
			mutex_unlock(&display->panel->panel_lock);
			usleep_range(6000, 6100);
		}
	}

	mutex_lock(&display->panel->panel_lock);
	system_backlight_target = display->panel->bl_config.bl_level;
	dsi_panel_set_backlight(display->panel, system_backlight_target);
	oppo_ffl_trigger_finish = true;
	mutex_unlock(&display->panel->panel_lock);

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s] failed to disable DSI core clocks, rc=%d\n",
				display->name, rc);
	}
}

int oppo_start_ffl_thread(void)
{
	mutex_lock(&oppo_ffl_lock);

	ffl_work_running = true;
	if (is_ffl_enable == FFL_TRIGGLE_CONTROL) {
		oppo_ffl_trigger_finish = false;
		kthread_queue_work(&oppo_ffl_worker, &oppo_ffl_work);
	}

	mutex_unlock(&oppo_ffl_lock);
	return 0;
}

void oppo_stop_ffl_thread(void)
{
	mutex_lock(&oppo_ffl_lock);

	oppo_ffl_trigger_finish = true;
	ffl_work_running = false;
	kthread_flush_worker(&oppo_ffl_worker);

	mutex_unlock(&oppo_ffl_lock);
}

int oppo_display_atomic_check(struct drm_crtc *crtc, struct drm_crtc_state *state)
{
	struct dsi_display *display = get_main_display();

	if (!is_dsi_panel(crtc))
		return 0;

	if (display && display->panel &&
	    display->panel->oppo_priv.is_aod_ramless &&
	    is_oppo_display_aod_mode() &&
	    (crtc->state->mode.flags | DRM_MODE_FLAG_CMD_MODE_PANEL)) {
		wait_event_timeout(oppo_aod_wait, !is_oppo_display_aod_mode(),
				   msecs_to_jiffies(100));
	}

	return 0;
}

static ssize_t oppo_display_notify_fp_press(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	struct drm_device *drm_dev = display->drm_dev;
	struct drm_connector *dsi_connector = display->drm_conn;
	struct drm_mode_config *mode_config = &drm_dev->mode_config;
	struct msm_drm_private *priv = drm_dev->dev_private;
	struct drm_display_mode *cmd_mode = NULL;
	struct drm_display_mode *vid_mode = NULL;
	struct drm_display_mode *cur_mode = NULL;
	struct drm_display_mode *mode = NULL;
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	static ktime_t on_time;
	bool mode_changed = false;
	int onscreenfp_status = 0;
	int vblank_get = -EINVAL;
	int err = 0;
	int i;

	if (!dsi_connector || !dsi_connector->state || !dsi_connector->state->crtc) {
		pr_err("[%s]: display not ready\n", __func__);
		return count;
	}

	sscanf(buf, "%du", &onscreenfp_status);
	onscreenfp_status = !!onscreenfp_status;
	if (onscreenfp_status == oppo_onscreenfp_status)
		return count;

	pr_err("notify fingerpress %s\n", onscreenfp_status ? "on" : "off");
	if (OPPO_DISPLAY_AOD_SCENE == get_oppo_display_scene()) {
		if (onscreenfp_status) {
			on_time = ktime_get();
		} else {
			ktime_t now = ktime_get();
			ktime_t delta = ktime_sub(now, on_time);

			if (ktime_to_ns(delta) < 300000000)
				msleep(300 - (ktime_to_ns(delta) / 1000000));
		}
	}

	vblank_get = drm_crtc_vblank_get(dsi_connector->state->crtc);
	if (vblank_get) {
		pr_err("failed to get crtc vblank\n", vblank_get);
	}
	oppo_onscreenfp_status = onscreenfp_status;

	if (onscreenfp_status &&
	    OPPO_DISPLAY_AOD_SCENE == get_oppo_display_scene() && !display->panel->oppo_priv.is_aod_ramless) {
		/* enable the clk vote for CMD mode panels */
		if (display->config.panel_mode == DSI_OP_CMD_MODE) {
			dsi_display_clk_ctrl(display->dsi_clk_handle,
					DSI_ALL_CLKS, DSI_CLK_ON);
		}

		mutex_lock(&display->panel->panel_lock);

		if (display->panel->panel_initialized)
			err = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_AOD_HBM_ON);

		mutex_unlock(&display->panel->panel_lock);
		if (err)
			pr_err("failed to setting aod hbm on mode %d\n", err);

		if (display->config.panel_mode == DSI_OP_CMD_MODE) {
			dsi_display_clk_ctrl(display->dsi_clk_handle,
					DSI_ALL_CLKS, DSI_CLK_OFF);
		}
	}

	if (!display->panel->oppo_priv.is_aod_ramless) {
		oppo_onscreenfp_vblank_count = drm_crtc_vblank_count(dsi_connector->state->crtc);
		oppo_onscreenfp_pressed_time = ktime_get();
	}

	drm_modeset_lock_all(drm_dev);

	state = drm_atomic_state_alloc(drm_dev);
	if (!state)
		goto error;
	state->acquire_ctx = mode_config->acquire_ctx;
	crtc = dsi_connector->state->crtc;
	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	cur_mode = &crtc->state->mode;
	for (i = 0; i < priv->num_crtcs; i++) {
		if (priv->disp_thread[i].crtc_id == crtc->base.id) {
			if (priv->disp_thread[i].thread)
				kthread_flush_worker(&priv->disp_thread[i].worker);
		}
	}

	if (display->panel->oppo_priv.is_aod_ramless) {
		struct drm_display_mode *set_mode = NULL;

		if (oppo_display_mode == 2)
			goto error;

		list_for_each_entry(mode, &dsi_connector->modes, head) {
			if (drm_mode_vrefresh(mode) == 0)
				continue;
			if (mode->flags & DRM_MODE_FLAG_VID_MODE_PANEL)
				vid_mode = mode;
			if (mode->flags & DRM_MODE_FLAG_CMD_MODE_PANEL)
				cmd_mode = mode;
		}

		set_mode = oppo_display_mode ? vid_mode : cmd_mode;
		set_mode = onscreenfp_status ? vid_mode : set_mode;
		if (!crtc_state->active || !crtc_state->enable)
			goto error;

		if (set_mode && drm_mode_vrefresh(set_mode) != drm_mode_vrefresh(&crtc_state->mode)) {
			mode_changed = true;
		} else {
			mode_changed = false;
		}

		if (mode_changed) {
			display->panel->dyn_clk_caps.dyn_clk_support = false;
			drm_atomic_set_mode_for_crtc(crtc_state, set_mode);
		}

		wake_up(&oppo_aod_wait);
	}

	err = drm_atomic_commit(state);
	drm_atomic_state_put(state);

	if (display->panel->oppo_priv.is_aod_ramless && mode_changed) {
		for (i = 0; i < priv->num_crtcs; i++) {
			if (priv->disp_thread[i].crtc_id == crtc->base.id) {
				if (priv->disp_thread[i].thread)
					kthread_flush_worker(&priv->disp_thread[i].worker);
			}
		}

		if (oppo_display_mode == 1)
			display->panel->dyn_clk_caps.dyn_clk_support = true;
	}

error:
	drm_modeset_unlock_all(drm_dev);
	if (!vblank_get)
		drm_crtc_vblank_put(dsi_connector->state->crtc);
	return count;
}

static ssize_t oppo_display_get_roundcorner(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	bool roundcorner = true;

	if (display && display->name &&
	    !strcmp(display->name,"qcom,mdss_dsi_oppo19101boe_nt37800_1080_2400_cmd"))
		roundcorner = false;

	return sprintf(buf, "%d\n", roundcorner);
}

static int dynamic_osc_clock = 90250;
static ssize_t oppo_display_get_dynamic_osc_clock(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", dynamic_osc_clock);
}

int oppo_dsi_update_dynamic_osc_clock(void){
	struct dsi_display *display = get_main_display();
	int rc = 0;
	int osc_clock = dynamic_osc_clock;

	if (!display||!display->panel) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!display->panel->oppo_priv.is_osc_support){
		pr_err("not support osc\n");
		return 0;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	if(osc_clock == 90250){
		rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_OSC_CLK_MODEO0);
	} else if(osc_clock == 88900) {
		rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_OSC_CLK_MODEO1);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

unlock:
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return rc;
}

static ssize_t oppo_display_set_dynamic_osc_clock(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	int osc_clk = 0;

	if (!display) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}
	sscanf(buf, "%du", &osc_clk);
	dynamic_osc_clock = osc_clk;

	if (display->panel->panel_mode != DSI_OP_CMD_MODE) {
		pr_err("only supported for command mode\n");
		return -EFAULT;
	}

	pr_info("%s: osc clk param value: '%d'\n", __func__, osc_clk);

	oppo_dsi_update_dynamic_osc_clock();
	return count;
}

int oppo_debug_max_brightness = 0;
static ssize_t oppo_display_get_max_brightness(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oppo_debug_max_brightness);
}

static ssize_t oppo_display_set_max_brightness(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &oppo_debug_max_brightness);

	return count;
}

static ssize_t oppo_display_get_ccd_check(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	struct mipi_dsi_device *mipi_device;
	int rc = 0;
	int ccd_check = 0;

	if (!display || !display->panel) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if(get_oppo_display_power_status() != OPPO_DISPLAY_POWER_ON) {
		printk(KERN_ERR"%s display panel in off status\n", __func__);
		return -EFAULT;
	}

	if (display->panel->panel_mode != DSI_OP_CMD_MODE) {
		pr_err("only supported for command mode\n");
		return -EFAULT;
	}

	if (!(display && display->panel->oppo_priv.vendor_name &&
	      !strcmp(display->panel->oppo_priv.vendor_name,"AMB655UV01"))) {
		ccd_check = 0;
		goto end;
	}

	mipi_device = &display->panel->mipi_device;

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("%s, cmd engine enable failed\n", __func__);
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	{
		char value[] = { 0x5A, 0x5A };
		rc = mipi_dsi_dcs_write(mipi_device, 0xF0, value, sizeof(value));
	}
	{
		char value[] = { 0x44, 0x50 };
		rc = mipi_dsi_dcs_write(mipi_device, 0xE7, value, sizeof(value));
	}
	usleep_range(1000, 1100);
	{
		char value[] = { 0x03 };
		rc = mipi_dsi_dcs_write(mipi_device, 0xB0, value, sizeof(value));
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}
	dsi_display_cmd_engine_disable(display);

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);
	{
		unsigned char read[10];

		rc = dsi_display_read_panel_reg(display, 0xE1, read, 1);

		pr_err("read ccd_check value = 0x%x rc=%d\n", read[0], rc);
		ccd_check = read[0];
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("%s, cmd engine enable failed\n", __func__);
		goto unlock;
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	{
		char value[] = { 0xA5, 0xA5 };
		rc = mipi_dsi_dcs_write(mipi_device, 0xF0, value, sizeof(value));
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

	dsi_display_cmd_engine_disable(display);
unlock:

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);
end:
	pr_err("[%s] ccd_check = %d\n",  display->panel->oppo_priv.vendor_name, ccd_check);
	return sprintf(buf, "%d\n", ccd_check);
}

struct aod_area {
	bool enable;
	int x;
	int y;
	int w;
	int h;
	int color;
	int bitdepth;
	int mono;
	int gray;
};

#define RAMLESS_AOD_AREA_NUM		6
#define RAMLESS_AOD_PAYLOAD_SIZE	100
static struct aod_area oppo_aod_area[RAMLESS_AOD_AREA_NUM];

int oppo_display_update_aod_area_unlock(void)
{
	struct dsi_display *display = get_main_display();
	struct mipi_dsi_device *mipi_device;
	char payload[RAMLESS_AOD_PAYLOAD_SIZE];
	int rc = 0;
	int i;

	if (!display || !display->panel || !display->panel->oppo_priv.is_aod_ramless)
		return 0;

	if (!dsi_panel_initialized(display->panel))
		return -EINVAL;

	mipi_device = &display->panel->mipi_device;

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	memset(payload, 0, RAMLESS_AOD_PAYLOAD_SIZE);

	for (i = 0; i < RAMLESS_AOD_AREA_NUM; i++) {
		struct aod_area *area = &oppo_aod_area[i];

		payload[0] |= (!!area->enable) << (RAMLESS_AOD_AREA_NUM - i - 1);
		if (area->enable) {
			int h_start = area->x;
			int h_block = area->w / 100;
			int v_start = area->y;
			int v_end = area->y + area->h;
			int off = i * 5;

			/* Rect Setting */
			payload[1 + off] = h_start >> 4;
			payload[2 + off] = ((h_start & 0xf) << 4) | (h_block & 0xf);
			payload[3 + off] = v_start >> 4;
			payload[4 + off] = ((v_start & 0xf) << 4) | ((v_end >> 8) & 0xf);
			payload[5 + off] = v_end & 0xff;

			/* Mono Setting */
			#define SET_MONO_SEL(index, shift) \
				if (i == index) \
					payload[31] |= area->mono << shift;

			SET_MONO_SEL(0, 6);
			SET_MONO_SEL(1, 5);
			SET_MONO_SEL(2, 4);
			SET_MONO_SEL(3, 2);
			SET_MONO_SEL(4, 1);
			SET_MONO_SEL(5, 0);
			#undef SET_MONO_SEL

			/* Depth Setting */
			if (i < 4)
				payload[32] |= (area->bitdepth & 0x3) << ((3 - i) * 2);
			else if (i == 4)
				payload[33] |= (area->bitdepth & 0x3) << 6;
			else if (i == 5)
				payload[33] |= (area->bitdepth & 0x3) << 4;
			/* Color Setting */
			#define SET_COLOR_SEL(index, reg, shift) \
				if (i == index) \
					payload[reg] |= (area->color & 0x7) << shift;
			SET_COLOR_SEL(0, 34, 4);
			SET_COLOR_SEL(1, 34, 0);
			SET_COLOR_SEL(2, 35, 4);
			SET_COLOR_SEL(3, 35, 0);
			SET_COLOR_SEL(4, 36, 4);
			SET_COLOR_SEL(5, 36, 0);
			#undef SET_COLOR_SEL
			/* Area Gray Setting */
			payload[37 + i] = area->gray & 0xff;
		}
	}
	payload[43] = 0x00;

	rc = mipi_dsi_dcs_write(mipi_device, 0x81, payload, 43);
	pr_err("dsi_cmd aod_area[%x] updated \n", payload[0]);

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

	return 0;
}

static ssize_t oppo_display_get_aod_area(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	int i, cnt = 0;

	if (!display || !display->panel || !display->panel->oppo_priv.is_aod_ramless)
		return -EINVAL;

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	cnt = snprintf(buf, PAGE_SIZE, "aod_area info:\n");
	for (i = 0; i < RAMLESS_AOD_AREA_NUM; i++) {
		struct aod_area *area = &oppo_aod_area[i];

		if (area->enable) {
			cnt += snprintf(buf + cnt, PAGE_SIZE,
				"    area[%d]: [%dx%d]-[%dx%d]-%d-%d-%d-%d\n",
				cnt, area->x, area->y, area->w, area->h,
				area->color, area->bitdepth, area->mono, area->gray);
		}
	}

	cnt += snprintf(buf + cnt, PAGE_SIZE, "aod_area raw:\n");
	for (i = 0; i < RAMLESS_AOD_AREA_NUM; i++) {
		struct aod_area *area = &oppo_aod_area[i];

		if (area->enable) {
			cnt += snprintf(buf + cnt, PAGE_SIZE,
				"%d %d %d %d %d %d %d %d",
				area->x, area->y, area->w, area->h,
				area->color, area->bitdepth, area->mono, area->gray);
		}
		cnt += snprintf(buf + cnt, PAGE_SIZE, ":");
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE, "aod_area end\n");

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return cnt;
}

static ssize_t oppo_display_set_aod_area(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	struct dsi_display *display = get_main_display();
	char *bufp = (char *)buf;
	char *token;
	int cnt = 0;

	if (!display || !display->panel) {
		pr_err("failed to find dsi display\n");
		return false;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	memset(oppo_aod_area, 0, sizeof(struct aod_area) * RAMLESS_AOD_AREA_NUM);

	while ((token = strsep(&bufp, ":")) != NULL) {
		struct aod_area *area = &oppo_aod_area[cnt];
		if (!*token)
			continue;

		sscanf(token, "%d %d %d %d %d %d %d %d",
			&area->x, &area->y, &area->w, &area->h,
			&area->color, &area->bitdepth, &area->mono, &area->gray);
		pr_err("aod_area[%d]: [%dx%d]-[%dx%d]-%d-%d-%d-%d\n",
			cnt, area->x, area->y, area->w, area->h,
			area->color, area->bitdepth, area->mono, area->gray);
		area->enable = true;
		cnt++;
	}
	oppo_display_update_aod_area_unlock();
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return count;
}

static ssize_t oppo_display_get_video(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();
	bool is_aod_ramless = false;

	if (display && display->panel && display->panel->oppo_priv.is_aod_ramless)
		is_aod_ramless = true;

	return sprintf(buf, "%d\n", is_aod_ramless);
}

static ssize_t oppo_display_set_video(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count) {
	struct dsi_display *display = get_main_display();
	struct drm_device *drm_dev = display->drm_dev;
	struct drm_connector *dsi_connector = display->drm_conn;
	struct drm_mode_config *mode_config = &drm_dev->mode_config;
	struct msm_drm_private *priv = drm_dev->dev_private;
	struct drm_display_mode *mode;
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	bool mode_changed = false;
	int mode_id = 0;
	int vblank_get = -EINVAL;
	int err = 0;
	int i;

	if (!display || !display->panel) {
		pr_err("failed to find dsi display\n");
		return -EINVAL;
	}

	if (!display->panel->oppo_priv.is_aod_ramless)
		return count;

	if (!dsi_connector || !dsi_connector->state || !dsi_connector->state->crtc) {
		pr_err("[%s]: display not ready\n", __func__);
		return count;
	}

	sscanf(buf, "%du", &mode_id);
	pr_err("setting display mode %d\n", mode_id);

	vblank_get = drm_crtc_vblank_get(dsi_connector->state->crtc);
	if (vblank_get) {
		pr_err("failed to get crtc vblank\n", vblank_get);
	}

	drm_modeset_lock_all(drm_dev);

	if (oppo_display_mode != 1)
		display->panel->dyn_clk_caps.dyn_clk_support = false;

	state = drm_atomic_state_alloc(drm_dev);
	if (!state)
		goto error;

	oppo_display_mode = mode_id;
	state->acquire_ctx = mode_config->acquire_ctx;
	crtc = dsi_connector->state->crtc;
	crtc_state = drm_atomic_get_crtc_state(state, crtc);

	{
		struct drm_display_mode *set_mode = NULL;
		struct drm_display_mode *cmd_mode = NULL;
		struct drm_display_mode *vid_mode = NULL;

		list_for_each_entry(mode, &dsi_connector->modes, head) {
			if (drm_mode_vrefresh(mode) == 0)
				continue;
			if (mode->flags & DRM_MODE_FLAG_VID_MODE_PANEL)
				vid_mode = mode;
			if (mode->flags & DRM_MODE_FLAG_CMD_MODE_PANEL)
				cmd_mode = mode;
		}

		set_mode = oppo_display_mode ? vid_mode : cmd_mode;
		set_mode = oppo_onscreenfp_status ? vid_mode : set_mode;

		if (set_mode && drm_mode_vrefresh(set_mode) != drm_mode_vrefresh(&crtc_state->mode)) {
			mode_changed = true;
		} else {
			mode_changed = false;
		}

		if (mode_changed) {
			for (i = 0; i < priv->num_crtcs; i++) {
				if (priv->disp_thread[i].crtc_id == crtc->base.id) {
					if (priv->disp_thread[i].thread)
						kthread_flush_worker(&priv->disp_thread[i].worker);
				}
			}

			display->panel->dyn_clk_caps.dyn_clk_support = false;
			drm_atomic_set_mode_for_crtc(crtc_state, set_mode);
		}
		wake_up(&oppo_aod_wait);
	}
	err = drm_atomic_commit(state);
	drm_atomic_state_put(state);

	if (mode_changed) {
		for (i = 0; i < priv->num_crtcs; i++) {
			if (priv->disp_thread[i].crtc_id == crtc->base.id) {
				if (priv->disp_thread[i].thread)
					kthread_flush_worker(&priv->disp_thread[i].worker);
			}
		}
	}

	if (oppo_display_mode == 1)
		display->panel->dyn_clk_caps.dyn_clk_support = true;

error:
	drm_modeset_unlock_all(drm_dev);
	if (!vblank_get)
		drm_crtc_vblank_put(dsi_connector->state->crtc);

	return count;
}

int dsi_display_read_panel_reg_unlock(struct dsi_display *display,u8 cmd, void *data, size_t len)
{
	struct dsi_display_ctrl *ctrl;
	struct dsi_cmd_desc cmdsreq;
	int rc = 0;
	u32 flags = 0;

	if (!display || !display->panel || data == NULL) {
		pr_err("%s, Invalid params\n", __func__);
		return -EINVAL;
	}

	ctrl = &display->ctrl[display->cmd_master_idx];

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc) {
			pr_err("%s, failed to allocate cmd tx buffer memory\n", __func__);
			goto done;
		}
	}

	rc = dsi_display_cmd_engine_enable(display);
	if (rc) {
		pr_err("%s, cmd engine enable failed\n", __func__);
		goto done;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	}

	memset(&cmdsreq, 0x0, sizeof(cmdsreq));
	cmdsreq.msg.type = 0x06;
	cmdsreq.msg.tx_buf = &cmd;
	cmdsreq.msg.tx_len = 1;
	cmdsreq.msg.rx_buf = data;
	cmdsreq.msg.rx_len = len;
	cmdsreq.msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
	flags |= (DSI_CTRL_CMD_FETCH_MEMORY | DSI_CTRL_CMD_READ |
		DSI_CTRL_CMD_CUSTOM_DMA_SCHED |
		DSI_CTRL_CMD_LAST_COMMAND);

	rc = dsi_ctrl_cmd_transfer(ctrl->ctrl, &cmdsreq.msg, flags);
	if (rc <= 0) {
		pr_err("%s, dsi_display_read_panel_reg rx cmd transfer failed rc=%d\n",
			__func__,
			rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
					  DSI_ALL_CLKS, DSI_CLK_OFF);
	}

	dsi_display_cmd_engine_disable(display);
done:
	return rc;
}

int oppo_check_aod_gamma_data_status(struct dsi_panel *panel)
{
	struct dsi_display *display = get_main_display();
	unsigned char read[30];
	int ret = -EINVAL;

	if (oppo_aod_gamma_check_data >= 0)
		return oppo_aod_gamma_check_data;

	{
		char value[] = { 0x5A, 0x5A };
		ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xF0, value, sizeof(value));
	}
	{
		char value[] = { 0x69 };
		ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xB0, value, sizeof(value));
	}
	ret = dsi_display_read_panel_reg_unlock(display, 0xBA, read, 1);
	if (ret < 0)
		pr_err("failed to read panel_reg ret=%d\n", ret);
	else
		oppo_aod_gamma_check_data = read[0];

	pr_err("%s: read aod gamma check data: %02x\n", __func__, oppo_aod_gamma_check_data);
	{
		char value[] = { 0xA5, 0xA5 };
		ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xF0, value, sizeof(value));
	}

	return oppo_aod_gamma_check_data;
}

int oppo_set_aod_gamma_data_status(struct dsi_panel *panel)
{
	struct dsi_display *display = get_main_display();
	int check_data = 0;
	int ret = 0;

	if (strcmp(panel->oppo_priv.vendor_name,"AMB655UV01"))
		return 0;

	check_data = oppo_check_aod_gamma_data_status(panel);
	if (check_data < 0 || check_data >= 10)
		return 0;

	{
		char value[] = { 0x5A, 0x5A };
		ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xF0, value, sizeof(value));
	}
	{
		char value[] = { 0x4D };
		ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xB0, value, sizeof(value));
	}
	{
		char value[] = { 0x07, 0x71, 0xf8, 0xA3, 0x09, 0xA2, 0x70,
			0xBC, 0x0C, 0xD3, 0x04, 0xF3, 0x0E, 0xD3, 0x59, 0x17,
			0x11, 0x13, 0xC9, 0x41, 0x14, 0x24, 0x89, 0x79, 0x18,
			0x05, 0x65, 0xBE, 0x1A, 0x05, 0xE1, 0xE2};
		ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xBA, value, sizeof(value));
	}
	{
		char value[] = { 0x03 };
		ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xF7, value, sizeof(value));
	}
	{
		char value[] = { 0xA5, 0xA5 };
		ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xF0, value, sizeof(value));
	}
	pr_err("resetting aod gamma table\n");

	return 0;
}

static int oppo_display_find_vreg_by_name(const char *name)
{
	int count = 0, i =0;
	struct dsi_vreg *vreg = NULL;
	struct dsi_regulator_info *dsi_reg = NULL;
	struct dsi_display *display = get_main_display();

	if (!display)
		return -ENODEV;

	if (!display->panel )
		return -EINVAL;

	dsi_reg = &display->panel->power_info;
        count = dsi_reg->count;
	for( i =0; i < count; i++ ){
		vreg = &dsi_reg->vregs[i];
		pr_err("%s : find  %s",__func__, vreg->vreg_name);
		if (!strcmp(vreg->vreg_name, name)) {
			pr_err("%s : find the vreg %s",__func__, name);
			return i;
		}else {
			continue;
		}
	}
	pr_err("%s : dose not find the vreg [%s]",__func__, name);

	return -EINVAL;
}

int dsi_panel_parse_panel_power_cfg(struct dsi_panel *panel)
{
        int rc = 0, i = 0;
	const char *name_vddi = NULL;
	const char *name_vddr = NULL;
	u32 *panel_vol = NULL;
	struct dsi_parser_utils *utils = &panel->utils;

	pr_err("[%s] \n", __func__);

	if (!strcmp(panel->type, "primary")) {
		panel_vol = &panel_vol_bak[PANEL_VOLTAGE_ID_VDDI].voltage_id;
		rc = utils->read_u32_array(utils->data, "qcom,panel_voltage_vddi",
					panel_vol, PANEL_VOLTAGE_VALUE_COUNT);
		if (rc) {
			pr_err("[%s] failed to parse panel_voltage vddi\n", panel->name);
			goto error;
		}

		rc = utils->read_string(utils->data, "qcom,panel_voltage_vddi_name",
					&name_vddi);
		if (rc) {
			pr_err("[%s] failed to parse vddi name\n", panel->name);
			goto error;
		}else {
			pr_err("[%s] surccess to parse vddi name %s\n", panel->name, name_vddi);
			strcpy(panel_vol_bak[PANEL_VOLTAGE_ID_VDDI].pwr_name,name_vddi);
		}

		panel_vol = &panel_vol_bak[PANEL_VOLTAGE_ID_VDDR].voltage_id;
		rc = utils->read_u32_array(utils->data, "qcom,panel_voltage_vddr",
					panel_vol, PANEL_VOLTAGE_VALUE_COUNT);
		if (rc) {
			pr_err("[%s] failed to parse panel_voltage vddr\n", panel->name);
			goto error;
		}

		rc = utils->read_string(utils->data, "qcom,panel_voltage_vddr_name",
					&name_vddr);
		if (rc) {
			pr_err("[%s] failed to parse vddr name\n", panel->name);
			goto error;
		}else {
			pr_err("[%s] surccess to parse vddr name %s\n", panel->name, name_vddr);
			strcpy(panel_vol_bak[PANEL_VOLTAGE_ID_VDDR].pwr_name,name_vddr);
		}

		//add for debug
		for(i; i < PANEL_VOLTAGE_ID_MAX; i++)
		{
			pr_err("[%s] panel_voltage[%d] = %d,%d,%d,%d,%s\n", __func__, i, panel_vol_bak[i].voltage_id,
				panel_vol_bak[i].voltage_min,panel_vol_bak[i].voltage_current,
				panel_vol_bak[i].voltage_max,panel_vol_bak[i].pwr_name);
		}
	}

	error:
		return rc;
}

static u32 update_current_voltage(u32 id)
{
	int vol_current = 0, pwr_id = 0;
	struct dsi_vreg *dsi_reg = NULL;
	struct dsi_regulator_info *dsi_reg_info = NULL;
	struct dsi_display *display = get_main_display();

	if (!display)
		return -ENODEV;
	if (!display->panel || !display->drm_conn)
		return -EINVAL;

	dsi_reg_info = &display->panel->power_info;
	pwr_id = oppo_display_find_vreg_by_name(panel_vol_bak[id].pwr_name);
	if (pwr_id < 0)
	{
		pr_err("%s: can't find the pwr_id, please check the vreg name\n",__func__);
		return pwr_id;
	}

	dsi_reg = &dsi_reg_info->vregs[pwr_id];
	if (!dsi_reg)
		return -EINVAL;

	vol_current = regulator_get_voltage(dsi_reg->vreg);

	return vol_current;
}

static ssize_t oppo_display_get_panel_pwr(struct device *dev,
	struct device_attribute *attr, char *buf) {

	u32 ret = 0;
	u32 i = 0;

	for (i = 0; i < (PANEL_VOLTAGE_ID_MAX-1); i++)
	{
		ret = update_current_voltage(panel_vol_bak[i].voltage_id);

		if (ret < 0)
			pr_err("%s : update_current_voltage error = %d\n", __func__, ret);
		else
			panel_vol_bak[i].voltage_current = ret;
	}
	panel_vol_bak[PANEL_VOLTAGE_ID_VG_BASE].voltage_current = panel_pwr_vg_base;

	return sprintf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d\n",
		panel_vol_bak[0].voltage_id,panel_vol_bak[0].voltage_min,
		panel_vol_bak[0].voltage_current,panel_vol_bak[0].voltage_max,
		panel_vol_bak[1].voltage_id,panel_vol_bak[1].voltage_min,
		panel_vol_bak[1].voltage_current,panel_vol_bak[1].voltage_max,
		panel_vol_bak[2].voltage_id,panel_vol_bak[2].voltage_min,
		panel_vol_bak[2].voltage_current,panel_vol_bak[2].voltage_max);
}

__attribute__((weak)) void oppo_notify_panel_gatevol_change(struct sde_connector *conn)
{
	return;
}

int dsi_panel_update_gatevol(struct dsi_panel *panel)
{
	int rc = 0;
	int mode = panel_pwr_vg_base;

	switch (mode) {
	case 1:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_GATE_VOL_2V);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_GATE_VOL_2V cmds, rc=%d\n",
					panel->name, rc);
		}
		break;
	case 2:
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_GATE_VOL_3V);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_GATE_VOL_3V cmds, rc=%d\n",
					panel->name, rc);
		}
		break;
	default:
		pr_err("[%s] default\n", panel->name);
	}

	return rc;
}

int dsi_display_update_panel_gatevol(struct dsi_display *display, int mode)
{
	int rc = 0;
	struct sde_connector *c_conn;

	if (!display || !display->drm_conn) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (mode == panel_pwr_vg_base){
		pr_err("%s ignore the same mode\n",__func__);
		return rc;
	}

	panel_pwr_vg_base = mode;
	c_conn = to_sde_connector(display->drm_conn);
	oppo_notify_panel_gatevol_change(c_conn);

	return rc;
}

static ssize_t oppo_display_set_panel_pwr(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	u32 panel_vol_value = 0, rc = 0, panel_vol_id = 0, pwr_id = 0;
	struct dsi_vreg *dsi_reg = NULL;
	struct dsi_regulator_info *dsi_reg_info = NULL;
	struct dsi_display *display = get_main_display();

	sscanf(buf, "%d %d", &panel_vol_id, &panel_vol_value);
	panel_vol_id = panel_vol_id & 0x0F;

	pr_err("debug for %s, buf = [%s], id = %d value = %d, count = %d\n",
		__func__, buf, panel_vol_id, panel_vol_value,count);

	if (panel_vol_id < 0 || panel_vol_id > PANEL_VOLTAGE_ID_MAX)
		return -EINVAL;

	if (panel_vol_value < panel_vol_bak[panel_vol_id].voltage_min ||
		panel_vol_id > panel_vol_bak[panel_vol_id].voltage_max)
		return -EINVAL;

	if (!display)
		return -ENODEV;
	if (!display->panel || !display->drm_conn) {
		return -EINVAL;
	}

	if (panel_vol_id == PANEL_VOLTAGE_ID_VG_BASE)
	{
		pr_err("%s: set the VGH_L pwr = %d \n",__func__,panel_vol_value );
		rc = dsi_display_update_panel_gatevol(display, panel_vol_value);
		if (rc < 0)
			return rc;

		return count;
	}

	dsi_reg_info = &display->panel->power_info;

	pwr_id = oppo_display_find_vreg_by_name(panel_vol_bak[panel_vol_id].pwr_name);
	if (pwr_id < 0)
	{
		pr_err("%s: can't find the vreg name, please re-check vreg name: %s \n",__func__,
                       panel_vol_bak[panel_vol_id].pwr_name);
		return pwr_id;
	}
	dsi_reg = &dsi_reg_info->vregs[pwr_id];

	rc = regulator_set_voltage(dsi_reg->vreg, panel_vol_value, panel_vol_value);
	if (rc) {
		pr_err("Set voltage(%s) fail, rc=%d\n",
			 dsi_reg->vreg_name, rc);
		return -EINVAL;
	}

	return count;
}

static struct kobject *oppo_display_kobj;

static DEVICE_ATTR(aod, S_IRUGO|S_IWUSR, NULL, oppo_display_set_aod);
static DEVICE_ATTR(hbm, S_IRUGO|S_IWUSR, oppo_display_get_hbm, oppo_display_set_hbm);
static DEVICE_ATTR(audio_ready, S_IRUGO|S_IWUSR, NULL, oppo_display_set_audio_ready);
static DEVICE_ATTR(seed, S_IRUGO|S_IWUSR, oppo_display_get_seed, oppo_display_set_seed);
static DEVICE_ATTR(panel_serial_number, S_IRUGO|S_IWUSR, oppo_display_get_panel_serial_number, NULL);
static DEVICE_ATTR(dump_info, S_IRUGO|S_IWUSR, oppo_display_dump_info, NULL);
static DEVICE_ATTR(panel_dsc, S_IRUGO|S_IWUSR, oppo_display_get_panel_dsc, NULL);
static DEVICE_ATTR(power_status, S_IRUGO|S_IWUSR, oppo_display_get_power_status, oppo_display_set_power_status);
static DEVICE_ATTR(display_regulator_control, S_IRUGO|S_IWUSR, NULL, oppo_display_regulator_control);
static DEVICE_ATTR(panel_id, S_IRUGO|S_IWUSR, oppo_display_get_panel_id, NULL);
static DEVICE_ATTR(sau_closebl_node, S_IRUGO|S_IWUSR, oppo_display_get_closebl_flag, oppo_display_set_closebl_flag);
static DEVICE_ATTR(write_panel_reg, S_IRUGO|S_IWUSR, oppo_display_get_panel_reg, oppo_display_set_panel_reg);
static DEVICE_ATTR(dsi_cmd, S_IRUGO|S_IWUSR, oppo_display_get_dsi_command, oppo_display_set_dsi_command);
static DEVICE_ATTR(dim_alpha, S_IRUGO|S_IWUSR, oppo_display_get_dim_alpha, oppo_display_set_dim_alpha);
static DEVICE_ATTR(dim_dc_alpha, S_IRUGO|S_IWUSR, oppo_display_get_dc_dim_alpha, oppo_display_set_dim_alpha);
static DEVICE_ATTR(dimlayer_hbm, S_IRUGO|S_IWUSR, oppo_display_get_dimlayer_hbm, oppo_display_set_dimlayer_hbm);
static DEVICE_ATTR(dimlayer_bl_en, S_IRUGO|S_IWUSR, oppo_display_get_dimlayer_enable, oppo_display_set_dimlayer_enable);
static DEVICE_ATTR(dimlayer_set_bl, S_IRUGO|S_IWUSR, oppo_display_get_dimlayer_backlight, oppo_display_set_dimlayer_backlight);
static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR, oppo_display_get_debug, oppo_display_set_debug);
static DEVICE_ATTR(force_screenfp, S_IRUGO|S_IWUSR, oppo_display_get_forcescreenfp, oppo_display_set_forcescreenfp);
static DEVICE_ATTR(esd_status, S_IRUGO|S_IWUSR, oppo_display_get_esd_status, oppo_display_set_esd_status);
static DEVICE_ATTR(notify_panel_blank, S_IRUGO|S_IWUSR, NULL, oppo_display_notify_panel_blank);
static DEVICE_ATTR(ffl_set, S_IRUGO|S_IWUSR, oppo_get_ffl_setting, oppo_set_ffl_setting);
static DEVICE_ATTR(notify_fppress, S_IRUGO|S_IWUSR, NULL, oppo_display_notify_fp_press);
static DEVICE_ATTR(aod_light_mode_set, S_IRUGO|S_IWUSR, oppo_get_aod_light_mode, oppo_set_aod_light_mode);
static DEVICE_ATTR(serial_number, S_IRUGO|S_IWUSR, oppo_display_get_serial_number, NULL);
static DEVICE_ATTR(spr, S_IRUGO|S_IWUSR, oppo_display_get_spr, oppo_display_set_spr);
static DEVICE_ATTR(roundcorner, S_IRUGO|S_IRUSR, oppo_display_get_roundcorner, NULL);
static DEVICE_ATTR(dynamic_osc_clock, S_IRUGO|S_IWUSR, oppo_display_get_dynamic_osc_clock, oppo_display_set_dynamic_osc_clock);
static DEVICE_ATTR(max_brightness, S_IRUGO|S_IWUSR, oppo_display_get_max_brightness, oppo_display_set_max_brightness);
static DEVICE_ATTR(ccd_check, S_IRUGO|S_IRUSR, oppo_display_get_ccd_check, NULL);
static DEVICE_ATTR(aod_area, S_IRUGO|S_IWUSR, oppo_display_get_aod_area, oppo_display_set_aod_area);
static DEVICE_ATTR(video, S_IRUGO|S_IWUSR, oppo_display_get_video, oppo_display_set_video);
static DEVICE_ATTR(failsafe, S_IRUGO|S_IWUSR, oppo_display_get_failsafe, oppo_display_set_failsafe);
static DEVICE_ATTR(mipi_clk_rate_hz, S_IRUGO|S_IWUSR, oppo_display_get_mipi_clk_rate_hz, NULL);
static DEVICE_ATTR(panel_pwr, S_IRUGO|S_IWUSR, oppo_display_get_panel_pwr, oppo_display_set_panel_pwr);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *oppo_display_attrs[] = {
	&dev_attr_aod.attr,
	&dev_attr_hbm.attr,
	&dev_attr_audio_ready.attr,
	&dev_attr_seed.attr,
	&dev_attr_panel_serial_number.attr,
	&dev_attr_dump_info.attr,
	&dev_attr_panel_dsc.attr,
	&dev_attr_power_status.attr,
	&dev_attr_display_regulator_control.attr,
	&dev_attr_panel_id.attr,
	&dev_attr_sau_closebl_node.attr,
	&dev_attr_write_panel_reg.attr,
	&dev_attr_dsi_cmd.attr,
	&dev_attr_dim_alpha.attr,
	&dev_attr_dim_dc_alpha.attr,
	&dev_attr_dimlayer_hbm.attr,
	&dev_attr_dimlayer_set_bl.attr,
	&dev_attr_dimlayer_bl_en.attr,
	&dev_attr_debug.attr,
	&dev_attr_force_screenfp.attr,
	&dev_attr_esd_status.attr,
	&dev_attr_notify_panel_blank.attr,
	&dev_attr_ffl_set.attr,
	&dev_attr_notify_fppress.attr,
	&dev_attr_aod_light_mode_set.attr,
	&dev_attr_serial_number.attr,
	&dev_attr_spr.attr,
	&dev_attr_roundcorner.attr,
	&dev_attr_dynamic_osc_clock.attr,
	&dev_attr_max_brightness.attr,
	&dev_attr_ccd_check.attr,
	&dev_attr_aod_area.attr,
	&dev_attr_video.attr,
	&dev_attr_failsafe.attr,
	&dev_attr_mipi_clk_rate_hz.attr,
	&dev_attr_panel_pwr.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group oppo_display_attr_group = {
	.attrs = oppo_display_attrs,
};

/*
 * Create a new API to get display resolution
 */
int oppo_display_get_resolution(unsigned int *xres, unsigned int *yres)
{
	*xres = *yres = 0;
	if (get_main_display() && get_main_display()->modes) {
		*xres = get_main_display()->modes->timing.v_active;
		*yres = get_main_display()->modes->timing.h_active;
	}
	return 0;
}
EXPORT_SYMBOL(oppo_display_get_resolution);

static int __init oppo_display_private_api_init(void)
{
	struct dsi_display *display = get_main_display();
	int retval;

	if (!display)
		return -EPROBE_DEFER;

	oppo_display_kobj = kobject_create_and_add("oppo_display", kernel_kobj);
	if (!oppo_display_kobj)
		return -ENOMEM;

	mm_kevent_init();
	/* Create the files associated with this kobject */
	retval = sysfs_create_group(oppo_display_kobj, &oppo_display_attr_group);
	if (retval)
		goto error_remove_kobj;

	retval = sysfs_create_link(oppo_display_kobj,
				   &display->pdev->dev.kobj, "panel");
	if (retval)
		goto error_remove_sysfs_group;

	kthread_init_worker(&oppo_ffl_worker);
	kthread_init_work(&oppo_ffl_work, &oppo_ffl_setting_thread);
	oppo_ffl_thread = kthread_run(kthread_worker_fn,
				      &oppo_ffl_worker, "oppo_ffl");

	return 0;

error_remove_sysfs_group:
	sysfs_remove_group(oppo_display_kobj, &oppo_display_attr_group);
error_remove_kobj:
	kobject_put(oppo_display_kobj);
	oppo_display_kobj = NULL;
	mm_kevent_deinit();

	return retval;
}

static void __exit oppo_display_private_api_exit(void)
{
	if (oppo_ffl_thread) {
		is_ffl_enable = FFL_EXIT_FULLY_CONTROL;
		kthread_flush_worker(&oppo_ffl_worker);
		kthread_stop(oppo_ffl_thread);
		oppo_ffl_thread = NULL;
	}

	sysfs_remove_link(oppo_display_kobj, "panel");
	sysfs_remove_group(oppo_display_kobj, &oppo_display_attr_group);
	kobject_put(oppo_display_kobj);
	mm_kevent_deinit();
}

module_init(oppo_display_private_api_init);
module_exit(oppo_display_private_api_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hujie <hujie@oppo.com>");
