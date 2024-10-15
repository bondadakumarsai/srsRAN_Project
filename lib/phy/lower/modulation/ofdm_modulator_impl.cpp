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

#include "ofdm_modulator_impl.h"
#include "srsran/phy/constants.h"
#include "srsran/phy/support/resource_grid_reader.h"
#include "srsran/ran/subcarrier_spacing.h"
#include "srsran/srsvec/copy.h"
#include "srsran/srsvec/sc_prod.h"
#include "srsran/srsvec/zero.h"
#include "srsran/phy/upper/channel_processors/pdcch_processor.h"
#include "srsran/phy/generic_functions/global.h"


#define ROWS 624
#define COLS 1

using namespace srsran;

int flag = 0;
int flag_initializer = 0;
bool DCIMask_Command = false; // Input should be coming from the keyboard

using namespace srsran;

ofdm_symbol_modulator_impl::ofdm_symbol_modulator_impl(ofdm_modulator_common_configuration& common_config,
                                                       const ofdm_modulator_configuration&  ofdm_config) :
  dft_size(ofdm_config.dft_size),
  rg_size(ofdm_config.bw_rb * NRE),
  cp(ofdm_config.cp),
  scs(to_subcarrier_spacing(ofdm_config.numerology)),
  sampling_rate_Hz(to_sampling_rate_Hz(scs, dft_size)),
  scale(ofdm_config.scale),
  dft(std::move(common_config.dft)),
  phase_compensation_table(to_subcarrier_spacing(ofdm_config.numerology),
                           ofdm_config.cp,
                           ofdm_config.dft_size,
                           ofdm_config.center_freq_hz,
                           true)
{
  srsran_assert(std::isnormal(scale), "Invalid scaling factor {}", scale);
  srsran_assert(
      dft_size > rg_size, "The DFT size ({}) must be greater than the resource grid size ({})", dft_size, rg_size);

  // Fill DFT input with zeros.
  srsvec::zero(dft->get_input());
}

void ofdm_symbol_modulator_impl::modulate(span<cf_t>                  output,
                                          const resource_grid_reader& grid,
                                          unsigned                    port_index,
                                          unsigned                    symbol_index,
					  unsigned a,
					  unsigned b)
{
  // Calculate number of symbols per slot.
  unsigned nsymb = get_nsymb_per_slot(cp);

  // Calculate cyclic prefix length.
  unsigned cp_len = cp.get_length(symbol_index, scs).to_samples(sampling_rate_Hz);

  // Make sure output buffer matches the symbol size.
  srsran_assert(output.size() == (cp_len + dft_size),
                "The output buffer size ({}) does not match the symbol index {} size ({}+{}={}). SCS={}kHz.",
                output.size(),
                symbol_index,
                cp_len,
                dft_size,
                cp_len + dft_size,
                scs_to_khz(scs));

  // Skip modulator if the grid is empty for the given port.
  if (grid.is_empty(port_index)) {
    srsvec::zero(output);
    return;
  }

    unsigned start = 699;
  // unsigned end   = 750;
  if (a > start)
  {
    flag = 1;
    // printf("Underlay Transmission starts \n");
  }

  // if (flag)
  // {

    if (b == 2)
    {
      char fullfilename[200]; //= "/home/vm1/Desktop/txFolderBin/underlay_grid700.bin";
      //sprintf(fullfilename, "/home/vm1/Desktop/txFolderBin/underlay_grid700_%d.bin", symbol_index);

      if (next_XOR_payload[0] == 0) { // Assuming if first bit is 0, the payload is all 0s
            sprintf(fullfilename, "/home/kumar/Desktop/txFolderBin/underlay_grid0_%d.bin", symbol_index);
            //printf("Hello, next_XOR_payload[0]=%d\n",next_XOR_payload[0]);
            }
      else { // Check if payload matches default_XOR
            sprintf(fullfilename, "/home/kumar/Desktop/txFolderBin/underlay_grid700_%d.bin", symbol_index);
      }


      // Set the path appropriately
      FILE *fp = fopen(fullfilename, "rb");
      float *real_part = (float *)malloc(ROWS * COLS * sizeof(float));
      float *imag_part = (float *)malloc(ROWS * COLS * sizeof(float));
      int aa = fread(real_part, sizeof(float), ROWS * COLS, fp);
      int bb = fread(imag_part, sizeof(float), ROWS * COLS, fp);
      fclose(fp);
      //printf("a = %d, b = %d\n",aa,bb);

      // Prepare lower bound frequency domain data.
      grid.get(dft->get_input().last(rg_size / 2), port_index, symbol_index % nsymb, 0);
      auto x = dft->get_input().last(rg_size / 2);
      for (int i = 0; i < (aa / 2); i++)
      {
        x[i] = x[i] + std::complex<float>(real_part[i], imag_part[i]);
      }

      // Prepare upper bound frequency domain data.
      grid.get(dft->get_input().first(rg_size / 2), port_index, symbol_index % nsymb, rg_size / 2);
      auto y = dft->get_input().first(rg_size / 2);
      for (int i = 0; i < (bb / 2); i++)
      {
        // printf("z = %.4f + %.4fi, %d, %d \t",(output[108 + i].real()),(output[108 + i].imag()),a,b);
        y[i] = y[i] + std::complex<float>(real_part[i + (rg_size / 2)], imag_part[i + (rg_size / 2)]);
      }

      free(real_part);
      free(imag_part);
    }
    else
    {

      // Prepare lower bound frequency domain data.
      grid.get(dft->get_input().last(rg_size / 2), port_index, symbol_index % nsymb, 0);

      // Prepare upper bound frequency domain data.
      grid.get(dft->get_input().first(rg_size / 2), port_index, symbol_index % nsymb, rg_size / 2);
    }
  //}



  if ((a % 64 == 0) && (b == 0))
  {
    //printf("global_flag=%d\n",global_flag);
    for (int i = 0; i < PAYLOAD_SIZE_MASK; i++)
    {
      current_XOR_payload[i] = next_XOR_payload[i];
    }

    if (global_flag)
    {
      for (int i = 0; i < PAYLOAD_SIZE_MASK; i++)
      {
        next_XOR_payload[i] = default_XOR[i];
      }
    }
    else
    {
      for (int i = 0; i < PAYLOAD_SIZE_MASK; i++)
      {
        next_XOR_payload[i] = 0;
      }
    }
  }


  // Prepare lower bound frequency domain data.
  // grid.get(dft->get_input().last(rg_size / 2), port_index, symbol_index % nsymb, 0);

  // Prepare upper bound frequency domain data.
  // grid.get(dft->get_input().first(rg_size / 2), port_index, symbol_index % nsymb, rg_size / 2);

  // Execute DFT.
  span<const cf_t> dft_output = dft->run();

  // Get phase correction (TS 138.211, Section 5.4)
  cf_t phase_compensation = phase_compensation_table.get_coefficient(symbol_index);

  // Apply scaling and phase compensation.
  srsvec::sc_prod(dft_output, phase_compensation * scale, output.last(dft_size));

  // Copy cyclic prefix.
  srsvec::copy(output.first(cp_len), output.last(cp_len));
}

unsigned ofdm_slot_modulator_impl::get_slot_size(unsigned slot_index) const
{
  unsigned nsymb = get_nsymb_per_slot(cp);
  unsigned count = 0;

  // Iterate all symbols of the slot and accumulate
  for (unsigned symbol_idx = 0; symbol_idx != nsymb; ++symbol_idx) {
    count += symbol_modulator.get_symbol_size(nsymb * slot_index + symbol_idx);
  }

  return count;
}

void ofdm_slot_modulator_impl::modulate(span<cf_t>                  output,
                                        const resource_grid_reader& grid,
                                        unsigned                    port_index,
                                        unsigned                    slot_index)
{
  unsigned nsymb = get_nsymb_per_slot(cp);

  unsigned nslots_per_subframe = get_nof_slots_per_subframe(to_subcarrier_spacing(numerology));
  srsran_assert(slot_index < nslots_per_subframe,
                "Slot index within the subframe {} exceeds the number of slots per subframe {}.",
                slot_index,
                nslots_per_subframe);

  // For each symbol in the slot.
  for (unsigned symbol_idx = 0; symbol_idx != nsymb; ++symbol_idx) {
    // Get the current symbol size.
    unsigned symbol_sz = symbol_modulator.get_symbol_size(nsymb * slot_index + symbol_idx);

    // Modulate symbol.
    symbol_modulator.modulate(output.first(symbol_sz), grid, port_index, nsymb * slot_index + symbol_idx, 0, 0);

    // Advance output buffer.
    output = output.last(output.size() - symbol_sz);
  }
}
