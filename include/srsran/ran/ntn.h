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
#include <cstdint>
#include <string>

namespace srsran {

struct position_velocity_t {
  int32_t position_x;
  int32_t position_y;
  int32_t position_z;
  int32_t velocity_vx;
  int32_t velocity_vy;
  int32_t velocity_vz;
};

struct ta_common_t {
  uint32_t ta_common;
  int32_t  ta_common_drift;
  uint16_t ta_common_drift_variant;
};

struct epoch_time_t {
  uint64_t sfn;
  uint32_t subframe_number;
};

struct ntn_config {
  // sib 19 values
  std::string reference_location;
  uint32_t    distance_threshold;
  // ntn-config values
  epoch_time_t        epoch_time;
  int32_t             cell_specific_koffset;
  uint32_t            k_mac;
  position_velocity_t ephemeris_info;
  ta_common_t         ta_info;
};
} // namespace srsran
