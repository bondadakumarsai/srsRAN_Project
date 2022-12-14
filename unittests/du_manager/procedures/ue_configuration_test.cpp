/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "du_manager_procedure_test_helpers.h"
#include "lib/du_manager/procedures/ue_configuration_procedure.h"
#include "srsgnb/asn1/rrc_nr/rrc_nr.h"
#include "srsgnb/du/du_cell_config_helpers.h"
#include "srsgnb/support/test_utils.h"
#include <gtest/gtest.h>

using namespace srsgnb;
using namespace srs_du;

class ue_config_tester : public du_manager_proc_tester, public ::testing::Test
{
protected:
  ue_config_tester() :
    du_manager_proc_tester(std::vector<du_cell_config>{config_helpers::make_default_du_cell_config()})
  {
    test_ue = &create_ue(to_du_ue_index(test_rgen::uniform_int<unsigned>(0, MAX_DU_UE_INDEX)));
  }

  void start_procedure(const f1ap_ue_context_update_request& req)
  {
    proc = launch_async<ue_configuration_procedure>(req, ue_mng, params.services, mac, params.rlc, params.f1ap);
    proc_launcher.emplace(proc);
  }

  void mac_finishes_ue_config(du_ue_index_t ue_index, bool result)
  {
    this->mac.wait_ue_reconf.result.result   = result;
    this->mac.wait_ue_reconf.result.ue_index = this->ue_mng.ues.begin()->get()->ue_index;
    this->mac.wait_ue_reconf.ready_ev.set();
  }

  void
  check_du_to_cu_rrc_container(const f1ap_ue_context_update_request& req, const byte_buffer& container, bool verbose)
  {
    ASSERT_FALSE(container.empty());
    asn1::rrc_nr::cell_group_cfg_s cell_group;
    {
      asn1::cbit_ref bref(container);
      ASSERT_TRUE(cell_group.unpack(bref) == asn1::SRSASN_SUCCESS);
      if (verbose) {
        asn1::json_writer js;
        cell_group.to_json(js);
        fmt::print("UE Configuration produced CellGroup: {}\n", js.to_string());
      }
    }

    ASSERT_EQ(cell_group.rlc_bearer_to_add_mod_list.size(), req.srbs_to_setup.size() + req.drbs_to_setup.size());
    for (srb_id_t srb_id : req.srbs_to_setup) {
      auto srb_it =
          std::find_if(cell_group.rlc_bearer_to_add_mod_list.begin(),
                       cell_group.rlc_bearer_to_add_mod_list.end(),
                       [srb_id](const auto& b) { return b.served_radio_bearer.srb_id() == srb_id_to_uint(srb_id); });
      ASSERT_NE(srb_it, cell_group.rlc_bearer_to_add_mod_list.end());
      ASSERT_EQ(srb_it->lc_ch_id, srb_id_to_lcid(srb_id));
      ASSERT_TRUE(srb_it->mac_lc_ch_cfg_present);
      ASSERT_TRUE(srb_it->rlc_cfg_present);
    }
    for (const drb_to_setup& drb : req.drbs_to_setup) {
      auto drb_it =
          std::find_if(cell_group.rlc_bearer_to_add_mod_list.begin(),
                       cell_group.rlc_bearer_to_add_mod_list.end(),
                       [&drb](const auto& b) { return b.served_radio_bearer.srb_id() == drb_id_to_uint(drb.drb_id); });
      ASSERT_NE(drb_it, cell_group.rlc_bearer_to_add_mod_list.end());
      ASSERT_FALSE(is_srb(uint_to_lcid(drb_it->lc_ch_id)));
      ASSERT_TRUE(drb_it->mac_lc_ch_cfg_present);
      ASSERT_TRUE(drb_it->rlc_cfg_present);
    }
  }

  du_ue*                                                        test_ue = nullptr;
  async_task<f1ap_ue_context_update_response>                   proc;
  optional<lazy_task_launcher<f1ap_ue_context_update_response>> proc_launcher;
};

TEST_F(ue_config_tester, when_du_manager_receives_ue_config_request_then_mac_and_f1ap_get_request_to_reconfigure_ue)
{
  // Start Procedure.
  start_procedure(create_f1ap_ue_context_update_request(test_ue->ue_index, {srb_id_t::srb2}, {drb_id_t::drb1}));

  // Check MAC received request to update UE configuration with valid params.
  ASSERT_TRUE(this->mac.last_ue_reconf_msg.has_value());
  ASSERT_EQ(this->mac.last_ue_reconf_msg->ue_index, test_ue->ue_index);
  ASSERT_NE(this->ue_mng.find_ue(this->mac.last_ue_reconf_msg->ue_index), nullptr);
  ASSERT_EQ(this->mac.last_ue_reconf_msg->bearers_to_addmod.size(), 2);
  ASSERT_EQ(this->mac.last_ue_reconf_msg->bearers_to_addmod[0].lcid, LCID_SRB2);
  ASSERT_NE(this->mac.last_ue_reconf_msg->bearers_to_addmod[0].dl_bearer, nullptr);
  ASSERT_NE(this->mac.last_ue_reconf_msg->bearers_to_addmod[0].ul_bearer, nullptr);
  ASSERT_FALSE(is_srb(this->mac.last_ue_reconf_msg->bearers_to_addmod[1].lcid));
  ASSERT_NE(this->mac.last_ue_reconf_msg->bearers_to_addmod[1].dl_bearer, nullptr);
  ASSERT_NE(this->mac.last_ue_reconf_msg->bearers_to_addmod[1].ul_bearer, nullptr);

  // Check F1AP received request to update UE configuration with valid params.
  ASSERT_TRUE(this->f1ap.last_ue_config.has_value());
  ASSERT_EQ(this->f1ap.last_ue_config->ue_index, test_ue->ue_index);
  ASSERT_EQ(this->f1ap.last_ue_config->f1c_bearers_to_add.size(), 1);
  ASSERT_EQ(this->f1ap.last_ue_config->f1c_bearers_to_add[0].srb_id, srb_id_t::srb2);
  ASSERT_NE(this->f1ap.last_ue_config->f1c_bearers_to_add[0].rx_sdu_notifier, nullptr);
  ASSERT_EQ(this->f1ap.last_ue_config->f1u_bearers_to_add.size(), 1);
  ASSERT_EQ(this->f1ap.last_ue_config->f1u_bearers_to_add[0].drb_id, drb_id_t::drb1);
  ASSERT_NE(this->f1ap.last_ue_config->f1u_bearers_to_add[0].rx_sdu_notifier, nullptr);
}

TEST_F(ue_config_tester, when_du_manager_completes_ue_configuration_procedure_then_response_has_rrc_container)
{
  // Start Procedure.
  f1ap_ue_context_update_request req =
      create_f1ap_ue_context_update_request(test_ue->ue_index, {srb_id_t::srb2}, {drb_id_t::drb1});
  start_procedure(req);

  // MAC finishes config.
  ASSERT_FALSE(proc.ready());
  mac_finishes_ue_config(test_ue->ue_index, true);

  // Check DU manager response to UE Config Request from F1AP.
  ASSERT_TRUE(proc.ready());
  f1ap_ue_context_update_response resp = proc.get();
  ASSERT_TRUE(resp.result);
  ASSERT_NO_FATAL_FAILURE(check_du_to_cu_rrc_container(req, resp.du_to_cu_rrc_container, true));
}

TEST_F(ue_config_tester, when_du_manager_finishes_processing_ue_config_request_then_mac_rlc_f1c_bearers_are_connected)
{
  // Run UE Configuration Procedure to completion.
  configure_ue(create_f1ap_ue_context_update_request(test_ue->ue_index, {srb_id_t::srb2}, {}));

  // Forward MAC Rx SDU through SRB2.
  // > Add dummy RLC data header.
  byte_buffer mac_sdu = {0x80, 0x0};
  // > Append data buffer.
  byte_buffer payload{test_rgen::random_vector<uint8_t>(test_rgen::uniform_int<unsigned>(1, 100))};
  mac_sdu.append(payload.copy());
  mac.last_ue_reconf_msg->bearers_to_addmod[0].ul_bearer->on_new_sdu({mac_sdu.copy()});

  // Check existence of F1-C Tx SDU.
  ASSERT_EQ(payload, f1ap.f1_ues[test_ue->ue_index].f1c_bearers[srb_id_t::srb2].last_tx_sdu);
}

TEST_F(ue_config_tester, when_du_manager_finishes_processing_ue_config_request_then_mac_rlc_f1u_bearers_are_connected)
{
  // Run UE Configuration Procedure to completion.
  configure_ue(create_f1ap_ue_context_update_request(test_ue->ue_index, {}, {drb_id_t::drb1}));

  // Forward MAC Rx SDU through DRB1.
  // > Add dummy RLC data header.
  byte_buffer mac_sdu = {0x80, 0x0};
  // > Append data buffer.
  byte_buffer payload{test_rgen::random_vector<uint8_t>(test_rgen::uniform_int<unsigned>(1, 100))};
  mac_sdu.append(payload.copy());
  mac.last_ue_reconf_msg->bearers_to_addmod[0].ul_bearer->on_new_sdu({mac_sdu.copy()});

  // Check existence of F1-U Tx SDU.
  ASSERT_EQ(payload, f1ap.f1_ues[test_ue->ue_index].f1u_bearers[drb_id_t::drb1].last_sdu);
}
