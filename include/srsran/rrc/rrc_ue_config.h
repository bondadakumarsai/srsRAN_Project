/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "srsran/asn1/rrc_nr/rrc_nr.h"
#include "srsran/cu_cp/up_resource_manager.h"
#include "srsran/srslog/srslog.h"

namespace srsran {

namespace srs_cu_cp {
struct rrc_ue_cfg_t {
  srslog::basic_logger&    logger = srslog::fetch_basic_logger("RRC");
  asn1::rrc_nr::pdcp_cfg_s srb1_pdcp_cfg; ///< PDCP configuration for SRB1.
  srsran::srs_cu_cp::up_resource_manager_cfg
                                  up_cfg; ///< DRB manager configuration holds the bearer configs for all FiveQIs.
  asn1::rrc_nr::meas_timing_cfg_s meas_timing_cfg;
};

} // namespace srs_cu_cp

} // namespace srsran
