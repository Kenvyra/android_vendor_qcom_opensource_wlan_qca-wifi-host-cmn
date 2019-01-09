/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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
 * DOC: vdev_mgr_ops.c
 *
 * This header file provides API definitions for filling data structures
 * and sending vdev mgmt commands to target_if/mlme
 */
#include "vdev_mgr_ops.h"
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_vdev_mlme_api.h>
#include <wlan_mlme_dbg.h>
#include <wlan_vdev_mgr_tgt_if_tx_api.h>
#include <target_if.h>
#include <init_deinit_lmac.h>
#include <wlan_lmac_if_api.h>
#include <wlan_reg_services_api.h>
#include <wlan_dfs_tgt_api.h>
#include "core/src/dfs.h"
#include <wlan_vdev_mgr_ucfg_api.h>

static QDF_STATUS vdev_mgr_create_param_update(
					struct vdev_mlme_obj *mlme_obj,
					struct vdev_create_params *param)
{
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_mlme_mbss_11ax *mbss;

	vdev = mlme_obj->vdev;
	if (!vdev) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	mbss = &mlme_obj->mgmt.mbss_11ax;
	param->pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	param->vdev_id = wlan_vdev_get_id(vdev);
	param->nss_2g = mlme_obj->mgmt.generic.nss_2g;
	param->nss_5g = mlme_obj->mgmt.generic.nss_5g;
	param->type = mlme_obj->mgmt.generic.type;
	param->subtype = mlme_obj->mgmt.generic.subtype;
	param->mbssid_flags = mbss->mbssid_flags;
	param->vdevid_trans = mbss->vdevid_trans;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS vdev_mgr_create_send(struct vdev_mlme_obj *mlme_obj)
{
	QDF_STATUS status;
	struct vdev_create_params param = {0};

	if (!mlme_obj) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	status = vdev_mgr_create_param_update(mlme_obj, &param);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Param Update Error: %d", status);
		return status;
	}

	status = tgt_vdev_mgr_create_send(mlme_obj, &param);

	return status;
}

static QDF_STATUS vdev_mgr_start_param_update(
					struct vdev_mlme_obj *mlme_obj,
					struct vdev_start_params *param)
{
	struct wlan_channel *bss_chan;
	uint32_t dfs_reg;
	uint64_t chan_flags;
	uint16_t chan_flags_ext;
	bool set_agile = false, dfs_set_cfreq2 = false;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;

	vdev = mlme_obj->vdev;
	if (!vdev) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_MLME_SB_ID) !=
							QDF_STATUS_SUCCESS) {
		mlme_err("Failed to get pdev reference");
		return QDF_STATUS_E_FAILURE;
	}

	bss_chan = wlan_vdev_mlme_get_bss_chan(vdev);
	param->vdev_id = wlan_vdev_get_id(vdev);
	chan_flags = mlme_obj->mgmt.generic.chan_flags;
	chan_flags_ext = mlme_obj->mgmt.generic.chan_flags_ext;

	tgt_dfs_set_current_channel(pdev, bss_chan->ch_freq,
				    chan_flags, chan_flags_ext,
				    bss_chan->ch_ieee,
				    bss_chan->ch_freq_seg1,
				    bss_chan->ch_freq_seg2);

	param->beacon_interval = mlme_obj->proto.generic.beacon_interval;
	param->dtim_period = mlme_obj->proto.generic.dtim_period;
	param->disable_hw_ack = mlme_obj->mgmt.generic.disable_hw_ack;
	param->preferred_rx_streams =
		mlme_obj->mgmt.chainmask_info.num_rx_chain;
	param->preferred_tx_streams =
		mlme_obj->mgmt.chainmask_info.num_tx_chain;

	wlan_reg_get_dfs_region(pdev, &dfs_reg);
	param->regdomain = dfs_reg;
	param->he_ops = mlme_obj->proto.he_ops_info.he_ops;

	param->channel.chan_id = bss_chan->ch_ieee;
	param->channel.pwr = mlme_obj->mgmt.generic.tx_power;
	param->channel.mhz = bss_chan->ch_freq;
	param->channel.half_rate = mlme_obj->mgmt.rate_info.half_rate;
	param->channel.quarter_rate = mlme_obj->mgmt.rate_info.quarter_rate;
	param->channel.dfs_set = mlme_obj->mgmt.generic.dfs_set;
	param->channel.dfs_set_cfreq2 = mlme_obj->mgmt.generic.dfs_set_cfreq2;
	param->channel.is_chan_passive =
		mlme_obj->mgmt.generic.is_chan_passive;
	param->channel.allow_ht = mlme_obj->proto.ht_info.allow_ht;
	param->channel.allow_vht = mlme_obj->proto.vht_info.allow_vht;
	param->channel.phy_mode = bss_chan->ch_phymode;
	param->channel.cfreq1 = mlme_obj->mgmt.generic.cfreq1;
	param->channel.cfreq2 = mlme_obj->mgmt.generic.cfreq2;
	param->channel.maxpower = mlme_obj->mgmt.generic.maxpower;
	param->channel.minpower = mlme_obj->mgmt.generic.minpower;
	param->channel.maxregpower = mlme_obj->mgmt.generic.maxregpower;
	param->channel.antennamax = mlme_obj->mgmt.generic.antennamax;
	param->channel.reg_class_id = mlme_obj->mgmt.generic.reg_class_id;
	param->bcn_tx_rate_code = mlme_obj->mgmt.rate_info.bcn_tx_rate;
	param->ldpc_rx_enabled = mlme_obj->proto.generic.ldpc;
	wlan_vdev_mlme_get_ssid(vdev, param->ssid.mac_ssid,
				&param->ssid.length);

	if (bss_chan->ch_phymode == WLAN_PHYMODE_11AXA_HE80_80) {
		tgt_dfs_find_vht80_chan_for_precac(pdev,
						   param->channel.phy_mode,
						   bss_chan->ch_freq_seg1,
						   &param->channel.cfreq1,
						   &param->channel.cfreq2,
						   &param->channel.phy_mode,
						   &dfs_set_cfreq2,
						   &set_agile);

		param->channel.dfs_set_cfreq2 = dfs_set_cfreq2;
		param->channel.set_agile = set_agile;
	}

	wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS vdev_mgr_start_send(
			struct vdev_mlme_obj *mlme_obj,
			bool restart)
{
	QDF_STATUS status;
	struct vdev_start_params param = {0};

	if (!mlme_obj) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	status = vdev_mgr_start_param_update(mlme_obj, &param);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Param Update Error: %d", status);
		return status;
	}

	param.is_restart = restart;
	status = tgt_vdev_mgr_start_send(mlme_obj, &param);

	return status;
}

static QDF_STATUS vdev_mgr_delete_param_update(
					struct vdev_mlme_obj *mlme_obj,
					struct vdev_delete_params *param)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = mlme_obj->vdev;
	if (!vdev) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	param->vdev_id = wlan_vdev_get_id(vdev);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS vdev_mgr_delete_send(struct vdev_mlme_obj *mlme_obj)
{
	QDF_STATUS status;
	struct vdev_delete_params param;

	if (!mlme_obj) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	status = vdev_mgr_delete_param_update(mlme_obj, &param);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Param Update Error: %d", status);
		return status;
	}

	status = tgt_vdev_mgr_delete_send(mlme_obj, &param);

	return status;
}

static QDF_STATUS vdev_mgr_stop_param_update(
				struct vdev_mlme_obj *mlme_obj,
				struct vdev_stop_params *param)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = mlme_obj->vdev;
	if (!vdev) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	param->vdev_id = wlan_vdev_get_id(vdev);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS vdev_mgr_stop_send(struct vdev_mlme_obj *mlme_obj)
{
	QDF_STATUS status;
	struct vdev_stop_params param = {0};

	if (!mlme_obj) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	status = vdev_mgr_stop_param_update(mlme_obj, &param);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Param Update Error: %d", status);
		return status;
	}

	status = tgt_vdev_mgr_stop_send(mlme_obj, &param);

	return status;
}

static QDF_STATUS vdev_mgr_config_ratemask_update(
				struct vdev_mlme_obj *mlme_obj,
				struct config_ratemask_params *param)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = mlme_obj->vdev;
	param->vdev_id = wlan_vdev_get_id(vdev);
	param->type = mlme_obj->mgmt.rate_info.type;
	param->lower32 = mlme_obj->mgmt.rate_info.lower32;
	param->higher32 = mlme_obj->mgmt.rate_info.higher32;
	param->lower32_2 = mlme_obj->mgmt.rate_info.lower32_2;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS vdev_mgr_bcn_tmpl_param_update(
				struct vdev_mlme_obj *mlme_obj,
				struct beacon_tmpl_params *param)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS vdev_mgr_sta_ps_param_update(
				struct vdev_mlme_obj *mlme_obj,
				struct sta_ps_params *param)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = mlme_obj->vdev;
	param->vdev_id = wlan_vdev_get_id(vdev);
	param->param = WLAN_MLME_CFG_UAPSD;
	param->value = mlme_obj->proto.sta.uapsd_cfg;
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS vdev_mgr_up_param_update(
				struct vdev_mlme_obj *mlme_obj,
				struct vdev_up_params *param)
{
	struct vdev_mlme_mbss_11ax *mbss;
	struct wlan_objmgr_vdev *vdev;

	vdev = mlme_obj->vdev;
	param->vdev_id = wlan_vdev_get_id(vdev);
	param->assoc_id = mlme_obj->proto.sta.assoc_id;
	mbss = &mlme_obj->mgmt.mbss_11ax;
	if (mbss->profile_idx) {
		param->profile_idx = mbss->profile_idx;
		param->profile_num = mbss->profile_num;
		qdf_mem_copy(param->trans_bssid, mbss->trans_bssid,
			     QDF_MAC_ADDR_SIZE);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS vdev_mgr_up_send(struct vdev_mlme_obj *mlme_obj)
{
	QDF_STATUS status;
	struct vdev_up_params param = {0};
	struct config_ratemask_params rm_param = {0};
	struct sta_ps_params ps_param = {0};
	struct beacon_tmpl_params bcn_tmpl_param = {0};
	enum QDF_OPMODE opmode;
	struct wlan_objmgr_vdev *vdev;

	if (!mlme_obj) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	vdev = mlme_obj->vdev;
	if (!vdev) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	vdev_mgr_up_param_update(mlme_obj, &param);
	vdev_mgr_bcn_tmpl_param_update(mlme_obj, &bcn_tmpl_param);

	opmode = wlan_vdev_mlme_get_opmode(vdev);
	if (opmode == QDF_STA_MODE) {
		vdev_mgr_sta_ps_param_update(mlme_obj, &ps_param);
		status = tgt_vdev_mgr_sta_ps_param_send(mlme_obj, &ps_param);

	} else if (opmode == QDF_SAP_MODE) {
		vdev_mgr_config_ratemask_update(mlme_obj, &rm_param);
		status = tgt_vdev_mgr_config_ratemask_cmd_send(mlme_obj,
							       &rm_param);
	}

	status = tgt_vdev_mgr_beacon_tmpl_send(mlme_obj, &bcn_tmpl_param);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = tgt_vdev_mgr_up_send(mlme_obj, &param);

	return status;
}

static QDF_STATUS vdev_mgr_down_param_update(
					struct vdev_mlme_obj *mlme_obj,
					struct vdev_down_params *param)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = mlme_obj->vdev;
	if (!vdev) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	param->vdev_id = wlan_vdev_get_id(vdev);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS vdev_mgr_down_send(struct vdev_mlme_obj *mlme_obj)
{
	QDF_STATUS status;
	struct vdev_down_params param = {0};

	if (!mlme_obj) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	status = vdev_mgr_down_param_update(mlme_obj, &param);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Param Update Error: %d", status);
		return status;
	}

	status = tgt_vdev_mgr_down_send(mlme_obj, &param);

	return status;
}

static QDF_STATUS vdev_mgr_peer_flush_tids_param_update(
					struct vdev_mlme_obj *mlme_obj,
					struct peer_flush_params *param,
					uint8_t *mac,
					uint32_t peer_tid_bitmap)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = mlme_obj->vdev;
	if (!vdev) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	param->vdev_id = wlan_vdev_get_id(vdev);
	param->peer_tid_bitmap = peer_tid_bitmap;
	qdf_mem_copy(param->peer_mac, mac, sizeof(*mac));
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS vdev_mgr_peer_flush_tids_send(struct vdev_mlme_obj *mlme_obj,
					 uint8_t *mac,
					 uint32_t peer_tid_bitmap)
{
	QDF_STATUS status;
	struct peer_flush_params param = {0};

	if (!mlme_obj || !mac) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	status = vdev_mgr_peer_flush_tids_param_update(mlme_obj, &param,
						       mac, peer_tid_bitmap);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Param Update Error: %d", status);
		return status;
	}

	status = tgt_vdev_mgr_peer_flush_tids_send(mlme_obj, &param);

	return status;
}

static QDF_STATUS vdev_mgr_multiple_restart_param_update(
				struct wlan_objmgr_pdev *pdev,
				struct mlme_channel_param *chan,
				uint32_t disable_hw_ack,
				uint32_t *vdev_ids,
				uint32_t num_vdevs,
				struct multiple_vdev_restart_params *param)
{
	param->pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	param->requestor_id = MULTIPLE_VDEV_RESTART_REQ_ID;
	param->disable_hw_ack = disable_hw_ack;
	param->cac_duration_ms = WLAN_DFS_WAIT_MS;
	param->num_vdevs = num_vdevs;

	qdf_mem_copy(param->vdev_ids, vdev_ids,
		     sizeof(uint32_t) * (param->num_vdevs));
	qdf_mem_copy(&param->ch_param, chan,
		     sizeof(struct mlme_channel_param));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS vdev_mgr_multiple_restart_send(struct wlan_objmgr_pdev *pdev,
					  struct mlme_channel_param *chan,
					  uint32_t disable_hw_ack,
					  uint32_t *vdev_ids,
					  uint32_t num_vdevs)
{
	struct multiple_vdev_restart_params param = {0};

	vdev_mgr_multiple_restart_param_update(pdev, chan,
					       disable_hw_ack,
					       vdev_ids, num_vdevs,
					       &param);

	return tgt_vdev_mgr_multiple_vdev_restart_send(pdev, &param);
}