/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "pdcch_processor_impl.h"
#include "srsran/ran/pdcch/cce_to_prb_mapping.h"
#include "srsran/support/math_utils.h"
#include <array> // for std::array
#include "srsran/phy/generic_functions/global.h" 
#define PAYLOAD_SIZE_MASK 22 // Assuming payload size is 128 bits (array of 0s and 1s)
#define SFN_MODULO 64

using namespace srsran;
using namespace pdcch_constants;

bounded_bitset<MAX_RB> pdcch_processor_impl::compute_rb_mask(const coreset_description& coreset,
                                                             const dci_description&     dci)
{
  prb_index_list prb_indexes;
  switch (coreset.cce_to_reg_mapping) {
    case cce_to_reg_mapping_type::CORESET0:
      prb_indexes = cce_to_prb_mapping_coreset0(coreset.bwp_start_rb,
                                                coreset.bwp_size_rb,
                                                coreset.duration,
                                                coreset.shift_index,
                                                dci.aggregation_level,
                                                dci.cce_index);
      break;
    case cce_to_reg_mapping_type::NON_INTERLEAVED:
      prb_indexes = cce_to_prb_mapping_non_interleaved(
          coreset.bwp_start_rb, coreset.frequency_resources, coreset.duration, dci.aggregation_level, dci.cce_index);
      break;
    case cce_to_reg_mapping_type::INTERLEAVED:
      prb_indexes = cce_to_prb_mapping_interleaved(coreset.bwp_start_rb,
                                                   coreset.frequency_resources,
                                                   coreset.duration,
                                                   coreset.reg_bundle_size,
                                                   coreset.interleaver_size,
                                                   coreset.shift_index,
                                                   dci.aggregation_level,
                                                   dci.cce_index);
      break;
  }

  bounded_bitset<MAX_RB> result(coreset.bwp_start_rb + coreset.bwp_size_rb);
  for (uint16_t prb_index : prb_indexes) {
    result.set(prb_index, true);
  }
  return result;
}


void pdcch_processor_impl::xor_payload(dci_description &dci,
                                       const std::array<uint8_t, pdcch_constants::MAX_DCI_PAYLOAD_SIZE> &xor_array)
{
  // dci.payload.size()
  for (size_t i = 0; i < 10; ++i)
  {
    dci.payload[i] ^= xor_array[i];
  }
}






void pdcch_processor_impl::process(resource_grid_mapper& mapper, const pdcch_processor::pdu_t& pdu)
{
  const coreset_description& coreset = pdu.coreset;
  const dci_description&     dci     = pdu.dci;

  // Verify CORESET.
  srsran_assert(coreset.duration > 0 && coreset.duration <= MAX_CORESET_DURATION,
                "Invalid CORESET duration ({})",
                coreset.duration);

  // Generate RB mask.
  bounded_bitset<MAX_RB> rb_mask = compute_rb_mask(coreset, dci);

  // Populate PDCCH encoder configuration.
  pdcch_encoder::config_t encoder_config;
  encoder_config.E    = dci.aggregation_level * NOF_REG_PER_CCE * NOF_RE_PDCCH_PER_RB * 2;
  encoder_config.rnti = dci.rnti;

    auto modified_dci = pdu.dci; // dci with xor payload

  // Create a random number generator
  // std::mt19937 rng(std::random_device{}());
  //std::mt19937 rng(123); // seed of 123
  // Define a uniform distribution of uint8_t values
//  std::uniform_int_distribution<uint8_t> dist(0, 255);
  // Generate xor_array with the same size as the payload
  // auto xor_array = pdu.dci.payload;
  std::array<uint8_t, pdcch_constants::MAX_DCI_PAYLOAD_SIZE> xor_array;

  // unsigned a = pdu.slot.sfn();

  // if ((a % SFN_MODULO == 0) && (pdu.slot.slot_index() == 0))
  // {
  //   for (int i = 0; i < PAYLOAD_SIZE_MASK; i++)
  //   {
  //     current_XOR_payload[i] = next_XOR_payload[i];
  //   }

  //   if (DCIMask_Command)
  //   {
  //     for (int i = 0; i < PAYLOAD_SIZE_MASK; i++)
  //     {
  //       next_XOR_payload[i] = default_XOR[i];
  //     }
  //   }
  //   else
  //   {
  //     for (int i = 0; i < PAYLOAD_SIZE_MASK; i++)
  //     {
  //       next_XOR_payload[i] = 0;
  //     }
  //   }
  // }

  // Apply XOR if current_XOR_payload is not all 0s
  int non_zero = 0;
  for (int i = 0; i < PAYLOAD_SIZE_MASK; i++)
  {
    if (current_XOR_payload[i] != 0)
    {
      non_zero = 1;
      break;
    }
  }

  if (non_zero)
  {
    // XOR logic with actual data should be implemented here
    for (size_t i = 0; i < 10; ++i)
    {
      // xor_array[i] = dist(rng);
      xor_array[i] = current_XOR_payload[i];
    }
  }

  for (size_t i = 0; i < 10; ++i)
    {
      // xor_array[i] = dist(rng);
      xor_array[i] = current_XOR_payload[i];
      
    }

  // if (a > start)
  // {
  //   flag1 = 1;
  //   // printf("Underlay Transmission starts \n");
  // }
  // if (flag1)
  // {
  //   for (size_t i = 0; i < 10; ++i)
  //   {
  //     // xor_array[i] = dist(rng);
  //     xor_array[i] = 1;
  //   }
  // }
  // else
  // {
  //   for (size_t i = 0; i < modified_dci.payload.size(); ++i)
  //   {
  //     // xor_array[i] = dist(rng);
  //     xor_array[i] = 0;
  //   }
  // }

  // if(a  > start && a < end)
  // {
  //   for (size_t i = 0; i < modified_dci.payload.size(); ++i) {
  //   // xor_array[i] = dist(rng);
  //   xor_array[i] = 1;
  //   }
  // }
  // else{
  //   for (size_t i = 0; i < modified_dci.payload.size(); ++i) {
  //   // xor_array[i] = dist(rng);
  //   xor_array[i] = 0;
  //   }
  // }

  // for (size_t i = 0; i < modified_dci.payload.size(); ++i) {
  //   // xor_array[i] = dist(rng);
  //   xor_array[i] = 1;
  // }

  // srsran_vec_fprint_hex(stdout, modified_dci.payload, modified_dci.payload.size());
  // Print DCI before XOR
  // std::cout << "DCI before XOR: \n";

  for (size_t i = 0; i < modified_dci.payload.size(); ++i)
  {
    // std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(modified_dci.payload[i]) << " ";
  }
  // std::cout << std::dec << "\n"; // Switch back to decimal for any other output

  // Apply XOR operation to the payload of modified_dci.
  // if (twenty_seconds_passed()) {
  //   xor_payload(modified_dci, xor_array);
  // }
  xor_payload(modified_dci, xor_array);
  // print after xor
  // std::cout << "DCI after XOR: \n";
  for (size_t i = 0; i < modified_dci.payload.size(); ++i)
  {
    // std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(modified_dci.payload[i]) << " ";
  }
  // std::cout << std::dec << "\n"; // Switch back to decimal for any other output

  // Encode.
  span<uint8_t> encoded = span<uint8_t>(temp_encoded).first(nof_encoded_bits(dci.aggregation_level));
  encoder->encode(encoded, modified_dci.payload, encoder_config);

  // Populate PDCCH modulator configuration.
  pdcch_modulator::config_t modulator_config;
  modulator_config.rb_mask            = rb_mask;
  modulator_config.start_symbol_index = coreset.start_symbol_index;
  modulator_config.duration           = coreset.duration;
  modulator_config.n_id               = dci.n_id_pdcch_data;
  modulator_config.n_rnti             = dci.n_rnti;
  modulator_config.scaling            = convert_dB_to_amplitude(dci.data_power_offset_dB);
  modulator_config.precoding          = dci.precoding;

  // Modulate.
  modulator->modulate(mapper, encoded, modulator_config);

  unsigned reference_point_k_rb =
      coreset.cce_to_reg_mapping == cce_to_reg_mapping_type::CORESET0 ? coreset.bwp_start_rb : 0;

  // Populate DMRS for PDCCH configuration.
  dmrs_pdcch_processor::config_t dmrs_pdcch_config;
  dmrs_pdcch_config.slot                 = pdu.slot;
  dmrs_pdcch_config.cp                   = pdu.cp;
  dmrs_pdcch_config.reference_point_k_rb = reference_point_k_rb;
  dmrs_pdcch_config.rb_mask              = rb_mask;
  dmrs_pdcch_config.start_symbol_index   = coreset.start_symbol_index;
  dmrs_pdcch_config.duration             = coreset.duration;
  dmrs_pdcch_config.n_id                 = dci.n_id_pdcch_dmrs;
  dmrs_pdcch_config.amplitude            = convert_dB_to_amplitude(dci.dmrs_power_offset_dB);
  dmrs_pdcch_config.precoding            = dci.precoding;

  // Generate DMRS.
  dmrs->map(mapper, dmrs_pdcch_config);
}
