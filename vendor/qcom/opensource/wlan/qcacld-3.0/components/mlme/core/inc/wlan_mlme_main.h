/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * DOC: declare internal APIs related to the mlme component
 */

#ifndef _WLAN_MLME_MAIN_H_
#define _WLAN_MLME_MAIN_H_

#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wmi_unified_param.h>

#define mlme_fatal(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_MLME, params)
#define mlme_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_MLME, params)
#define mlme_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_MLME, params)
#define mlme_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_MLME, params)
#define mlme_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_MLME, params)

/**
 * struct wlan_ies - Generic WLAN Information Element(s) format
 * @len: Total length of the IEs
 * @data: IE data
 */
struct wlan_ies {
	uint16_t len;
	uint8_t *data;
};

/**
 * struct peer_mlme_priv_obj - peer MLME component object
 * @ucast_key_cipher: unicast crypto type.
 * @is_pmf_enabled: True if PMF is enabled
 * @last_assoc_received_time: last assoc received time
 * @last_disassoc_deauth_received_time: last disassoc/deauth received time
 */
struct peer_mlme_priv_obj {
	uint32_t ucast_key_cipher;
	bool is_pmf_enabled;
	qdf_time_t last_assoc_received_time;
	qdf_time_t last_disassoc_deauth_received_time;
};

/**
 * struct vdev_mlme_obj - VDEV MLME component object
 * @dynamic_cfg: current configuration of nss, chains for vdev.
 * @ini_cfg: Max configuration of nss, chains supported for vdev.
 * @sta_dynamic_oce_value: Dyanmic oce flags value for sta
 * @follow_ap_edca: if true, it is forced to follow the AP's edca
 * @self_disconnect_ies: Disconnect IEs to be sent in deauth/disassoc frames
 *			 originated from driver
 * @peer_disconnect_ies: Disconnect IEs received in deauth/disassoc frames
 *			 from peer
 * @reconn_after_assoc_timeout: reconnect to the same AP if association timeout
 */
struct vdev_mlme_priv_obj {
	struct mlme_nss_chains dynamic_cfg;
	struct mlme_nss_chains ini_cfg;
	uint8_t sta_dynamic_oce_value;
	bool follow_ap_edca;
	struct wlan_ies self_disconnect_ies;
	struct wlan_ies peer_disconnect_ies;
	bool reconn_after_assoc_timeout;
};


/**
 * wlan_vdev_mlme_get_priv_obj() - Update the oce flags to FW
 * @vdev: pointer to vdev object
 *
 * Return: vdev_mlme_priv_obj- Mlme private object
 */
struct vdev_mlme_priv_obj *
wlan_vdev_mlme_get_priv_obj(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlme_update_oce_flags() - Update the oce flags to FW
 * @pdev: pointer to pdev object
 * @cfg_value: INI value
 *
 * Return: void
 */
void wlan_mlme_update_oce_flags(struct wlan_objmgr_pdev *pdev,
				uint8_t cfg_value);

/**
 * mlme_get_dynamic_vdev_config() - get the vdev dynamic config params
 * @vdev: vdev pointer
 *
 * Return: pointer to the dynamic vdev config structure
 */
struct mlme_nss_chains *mlme_get_dynamic_vdev_config(
					struct wlan_objmgr_vdev *vdev);

/**
 * mlme_get_ini_vdev_config() - get the vdev ini config params
 * @vdev: vdev pointer
 *
 * Return: pointer to the ini vdev config structure
 */
struct mlme_nss_chains *mlme_get_ini_vdev_config(
					struct wlan_objmgr_vdev *vdev);

/**
 * mlme_vdev_object_created_notification(): mlme vdev create handler
 * @vdev: vdev which is going to created by objmgr
 * @arg: argument for vdev create handler
 *
 * Register this api with objmgr to detect vdev is created
 *
 * Return: QDF_STATUS status in case of success else return error
 */

QDF_STATUS
mlme_vdev_object_created_notification(struct wlan_objmgr_vdev *vdev,
				      void *arg);

/**
 * mlme_vdev_object_destroyed_notification(): mlme vdev delete handler
 * @psoc: vdev which is going to delete by objmgr
 * @arg: argument for vdev delete handler
 *
 * Register this api with objmgr to detect vdev is deleted
 *
 * Return: QDF_STATUS status in case of success else return error
 */
QDF_STATUS
mlme_vdev_object_destroyed_notification(struct wlan_objmgr_vdev *vdev,
					void *arg);

/**
 * wlan_peer_set_unicast_cipher() - set unicast cipher
 * @peer: PEER object
 * @value: value to be set
 *
 * Return: void
 */
static inline
void wlan_peer_set_unicast_cipher(struct wlan_objmgr_peer *peer, uint32_t value)
{
	struct peer_mlme_priv_obj *peer_priv;

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		mlme_err(" peer mlme component object is NULL");
		return;
	}
	peer_priv->ucast_key_cipher  = value;
}

/**
 * wlan_peer_get_unicast_cipher() - get unicast cipher
 * @peer: PEER object
 *
 * Return: ucast_key_cipher value
 */
static inline
uint32_t wlan_peer_get_unicast_cipher(struct wlan_objmgr_peer *peer)
{
	struct peer_mlme_priv_obj *peer_priv;

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		mlme_err("peer mlme component object is NULL");
		return 0;
	}

	return peer_priv->ucast_key_cipher;
}

/**
 * wma_get_peer_mic_len() - get mic hdr len and mic length for peer
 * @psoc: psoc
 * @pdev_id: pdev id for the peer
 * @peer_mac: peer mac
 * @mic_len: mic length for peer
 * @mic_hdr_len: mic header length for peer
 *
 * Return: Success or Failure status
 */
QDF_STATUS mlme_get_peer_mic_len(struct wlan_objmgr_psoc *psoc, uint8_t pdev_id,
				 uint8_t *peer_mac, uint8_t *mic_len,
				 uint8_t *mic_hdr_len);

/**
 * mlme_peer_object_created_notification(): mlme peer create handler
 * @peer: peer which is going to created by objmgr
 * @arg: argument for vdev create handler
 *
 * Register this api with objmgr to detect peer is created
 *
 * Return: QDF_STATUS status in case of success else return error
 */

QDF_STATUS
mlme_peer_object_created_notification(struct wlan_objmgr_peer *peer,
				      void *arg);

/**
 * mlme_peer_object_destroyed_notification(): mlme peer delete handler
 * @peer: peer which is going to delete by objmgr
 * @arg: argument for vdev delete handler
 *
 * Register this api with objmgr to detect peer is deleted
 *
 * Return: QDF_STATUS status in case of success else return error
 */
QDF_STATUS
mlme_peer_object_destroyed_notification(struct wlan_objmgr_peer *peer,
					void *arg);

/**
 * mlme_set_peer_pmf_status() - set pmf status of peer
 * @peer: PEER object
 * @is_pmf_enabled: Carries if PMF is enabled or not
 *
 * is_pmf_enabled will be set to true if PMF is enabled by peer
 *
 * Return: void
 */
void mlme_set_peer_pmf_status(struct wlan_objmgr_peer *peer,
			      bool is_pmf_enabled);
/**
 * mlme_get_peer_pmf_status() - get if peer is of pmf capable
 * @peer: PEER object
 *
 * Return: Value of is_pmf_enabled; True if PMF is enabled by peer
 */
bool mlme_get_peer_pmf_status(struct wlan_objmgr_peer *peer);

/**
 * mlme_set_self_disconnect_ies() - Set diconnect IEs configured from userspace
 * @vdev: vdev pointer
 * @ie: pointer for disconnect IEs
 *
 * Return: None
 */
void mlme_set_self_disconnect_ies(struct wlan_objmgr_vdev *vdev,
				  struct wlan_ies *ie);

/**
 * mlme_free_self_disconnect_ies() - Free the self diconnect IEs
 * @vdev: vdev pointer
 *
 * Return: None
 */
void mlme_free_self_disconnect_ies(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_get_self_disconnect_ies() - Get diconnect IEs from vdev object
 * @vdev: vdev pointer
 *
 * Return: Returns a pointer to the self disconnect IEs present in vdev object
 */
struct wlan_ies *mlme_get_self_disconnect_ies(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_peer_disconnect_ies() - Cache disconnect IEs received from peer
 * @vdev: vdev pointer
 * @ie: pointer for disconnect IEs
 *
 * Return: None
 */
void mlme_set_peer_disconnect_ies(struct wlan_objmgr_vdev *vdev,
				  struct wlan_ies *ie);

/**
 * mlme_free_peer_disconnect_ies() - Free the peer diconnect IEs
 * @vdev: vdev pointer
 *
 * Return: None
 */
void mlme_free_peer_disconnect_ies(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_reconn_after_assoc_timeout_flag() - Set reconn after assoc timeout
 * flag
 * @psoc: soc object
 * @vdev_id: vdev id
 * @flag: enable or disable reconnect
 *
 * Return: void
 */
void mlme_set_reconn_after_assoc_timeout_flag(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id, bool flag);

/**
 * mlme_get_reconn_after_assoc_timeout_flag() - Get reconn after assoc timeout
 * flag
 * @psoc: soc object
 * @vdev_id: vdev id
 *
 * Return: true for enabling reconnect, otherwise false
 */
bool mlme_get_reconn_after_assoc_timeout_flag(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id);

/**
 * mlme_get_peer_disconnect_ies() - Get diconnect IEs from vdev object
 * @vdev: vdev pointer
 *
 * Return: Returns a pointer to the peer disconnect IEs present in vdev object
 */
struct wlan_ies *mlme_get_peer_disconnect_ies(struct wlan_objmgr_vdev *vdev);
#endif
