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

#include "pucch_resource_generator.h"
#include "srsran/ran/pucch/pucch_info.h"

using namespace srsran;
using namespace srs_du;

namespace {

struct pucch_grant {
  pucch_format                format;
  ofdm_symbol_range           symbols;
  prb_interval                prbs;
  std::optional<prb_interval> freq_hop_grant;
  std::optional<unsigned>     occ_cs_idx;
};

} // namespace

/// Returns the number of possible spreading factors which is a function of the number of symbols.
static unsigned format1_symb_to_spreading_factor(bounded_integer<unsigned, 4, 14> f1_symbols)
{
  // As per Table 6.3.2.4.1-1, TS 38.211.
  return f1_symbols.to_uint() / 2;
};

/// Given the OCC-CS index (implementation-defined), maps and returns the \c initialCyclicShift, defined as per
/// PUCCH-format1, in PUCCH-Config, TS 38.331.
static unsigned occ_cs_index_to_cyclic_shift(unsigned occ_cs_idx, unsigned nof_css)
{
  // NOTE: the OCC-CS index -> (CS, OCC) mapping is defined as follows.
  // i)   Define CS_step = 12 / nof_css. NOTE that 12 is divisible by nof_css.
  // ii)  occ_cs_idx = 0, 1, 2, ...  => (CS = 0, OCC=0), (CS = CS_step, OCC=0), (CS = 2*CS_step, OCC=0), ...
  // iii) occ_cs_idx = nof_css, nof_css+1, ...  => (CS = 0, OCC=1), (CS = CS_step, OCC=1), ...
  // iv)  occ_cs_idx = 2*nof_css, (2*nof_css)+1, ...  => (CS = 0, OCC=2), (CS = CS_step, OCC=2), ...
  const unsigned max_nof_css = format1_cp_step_to_uint(nof_cyclic_shifts::twelve);
  const unsigned cs_step     = max_nof_css / nof_css;
  return (occ_cs_idx * cs_step) % max_nof_css;
}

/// Given the OCC-CS index (implementation-defined), maps and returns the \c timeDomainOCC, defined as per
/// PUCCH-format1, in PUCCH-Config, TS 38.331.
static unsigned occ_cs_index_to_occ(unsigned occ_cs_idx, unsigned nof_css)
{
  // NOTE: the OCC-CS index -> (CS, OCC) mapping is defined as follows.
  // i)   Define CS_step = 12 / nof_css. NOTE that 12 is divisible by nof_css.
  // ii)  occ_cs_idx = 0, 1, 2, ...  => (CS = 0, OCC=0), (CS = CS_step, OCC=0), (CS = 2*CS_step, OCC=0), ...
  // iii) occ_cs_idx = nof_css, nof_css+1, ...  => (CS = 0, OCC=1), (CS = CS_step, OCC=1), ...
  // iv)  occ_cs_idx = 2*nof_css, (2*nof_css)+1, ...  => (CS = 0, OCC=2), (CS = CS_step, OCC=2), ...
  return occ_cs_idx / nof_css;
}

static std::vector<pucch_grant> compute_f0_res(unsigned                         nof_res_f0,
                                               pucch_f0_params                  params,
                                               unsigned                         bwp_size_rbs,
                                               bounded_integer<unsigned, 1, 14> max_nof_symbols)
{
  // Compute the number of symbols and RBs for F0.
  std::vector<pucch_grant> res_list;
  const unsigned           nof_f0_symbols = params.nof_symbols.to_uint();

  // With intraslot freq. hopping.
  if (params.intraslot_freq_hopping) {
    for (unsigned rb_idx = 0; rb_idx < bwp_size_rbs / 2 - 1U; ++rb_idx) {
      // Generate resource for increasing RB index, until the num. of required resources is reached.
      const prb_interval prbs{rb_idx, rb_idx + 1U};
      // Pre-compute the second RBs for the frequency hop specularly to the first RBs.
      const prb_interval freq_hop_prbs{bwp_size_rbs - 1U - rb_idx, bwp_size_rbs - rb_idx};

      // Generate resource for increasing Symbol index, until the num. of required resources is reached.
      for (unsigned sym_idx = 0; sym_idx + nof_f0_symbols <= max_nof_symbols.to_uint(); sym_idx += nof_f0_symbols) {
        const ofdm_symbol_range symbols{sym_idx, sym_idx + nof_f0_symbols};

        // Allocate resources for first hop.
        res_list.emplace_back(pucch_grant{.format         = srsran::pucch_format::FORMAT_0,
                                          .symbols        = symbols,
                                          .prbs           = prbs,
                                          .freq_hop_grant = freq_hop_prbs});
        if (res_list.size() == nof_res_f0) {
          break;
        }

        // Allocate resources for second hop.
        res_list.emplace_back(pucch_grant{.format         = srsran::pucch_format::FORMAT_2,
                                          .symbols        = symbols,
                                          .prbs           = freq_hop_prbs,
                                          .freq_hop_grant = prbs});
        if (res_list.size() == nof_res_f0) {
          break;
        }
      }
      if (res_list.size() == nof_res_f0) {
        break;
      }
    }
  }
  // With intraslot freq. hopping.
  else {
    for (unsigned rb_idx = 0; rb_idx < bwp_size_rbs / 2 - 1U; ++rb_idx) {
      const prb_interval prbs_low_spectrum{rb_idx, rb_idx + 1U};
      // Pre-compute the RBs for the resources to be allocated on the upper part of the spectrum. This is to achieve
      // balancing of the PUCCH resources on both sides of the BWP.
      const prb_interval prbs_hi_spectrum{bwp_size_rbs - 1U - rb_idx, bwp_size_rbs - rb_idx};

      // Generate resource for increasing Symbol index, until the num. of required resources is reached.
      for (unsigned sym_idx = 0; sym_idx + nof_f0_symbols <= max_nof_symbols.to_uint(); sym_idx += nof_f0_symbols) {
        const ofdm_symbol_range symbols{sym_idx, sym_idx + nof_f0_symbols};
        res_list.emplace_back(
            pucch_grant{.format = srsran::pucch_format::FORMAT_0, .symbols = symbols, .prbs = prbs_low_spectrum});
        if (res_list.size() == nof_res_f0) {
          break;
        }
      }
      if (res_list.size() == nof_res_f0) {
        break;
      }

      // Repeat the resource allocation on the upper part of the spectrum, to spread the PUCCH resource on both sides of
      // the BWP.
      for (unsigned sym_idx = 0; sym_idx + nof_f0_symbols <= 14; sym_idx += nof_f0_symbols) {
        const ofdm_symbol_range symbols{sym_idx, sym_idx + nof_f0_symbols};
        res_list.emplace_back(
            pucch_grant{.format = srsran::pucch_format::FORMAT_0, .symbols = symbols, .prbs = prbs_hi_spectrum});
        if (res_list.size() == nof_res_f0) {
          break;
        }
      }

      if (res_list.size() == nof_res_f0) {
        break;
      }
    }
  }

  return res_list;
}

static std::vector<pucch_grant> compute_f1_res(unsigned                         nof_res_f1,
                                               pucch_f1_params                  params,
                                               unsigned                         bwp_size_rbs,
                                               unsigned                         nof_occ_css,
                                               bounded_integer<unsigned, 1, 14> max_nof_symbols)
{
  std::vector<pucch_grant> res_list;

  // With intraslot_freq_hopping.
  if (params.intraslot_freq_hopping) {
    // Generate resource for increasing RB index, until the num. of required resources is reached.
    for (unsigned rb_idx = 0; rb_idx < bwp_size_rbs / 2 - 1; ++rb_idx) {
      const prb_interval prbs{rb_idx, rb_idx + 1};
      // Pre-compute the second RBs for the frequency hop specularly to the first RBs.
      const prb_interval freq_hop_prbs{bwp_size_rbs - 1 - rb_idx, bwp_size_rbs - rb_idx};

      // Generate resource for increasing Symbol index, until the num. of required resources is reached.
      for (unsigned sym_idx = 0; sym_idx + params.nof_symbols.to_uint() <= max_nof_symbols.to_uint();
           sym_idx += params.nof_symbols.to_uint()) {
        const ofdm_symbol_range symbols{sym_idx, sym_idx + params.nof_symbols.to_uint()};

        // Allocate all OCC and CSS resources for first hop, until the num. of required resources is reached.
        // NOTE: occ_cs_idx is an index that will be mapped into Initial Cyclic Shift and Orthogonal Cover Code (OCC).
        // Two PUCCH F1 resources over the same symbols and RBs are orthogonal if they have different CS or different
        // OCC; this leads to a tot. nof OCCs x tot. nof CSs possible orthogonal F1 resources over the same REs.
        for (unsigned occ_cs_idx = 0; occ_cs_idx < nof_occ_css; ++occ_cs_idx) {
          res_list.emplace_back(pucch_grant{.format         = srsran::pucch_format::FORMAT_1,
                                            .symbols        = symbols,
                                            .prbs           = prbs,
                                            .freq_hop_grant = freq_hop_prbs,
                                            .occ_cs_idx     = occ_cs_idx});
          if (res_list.size() == nof_res_f1) {
            break;
          }
        }
        if (res_list.size() == nof_res_f1) {
          break;
        }

        // Allocate all OCC and CSS resources for second hop, until the num. of required resources is reached.
        for (unsigned occ_cs_idx = 0; occ_cs_idx < nof_occ_css; ++occ_cs_idx) {
          res_list.emplace_back(pucch_grant{.format         = srsran::pucch_format::FORMAT_1,
                                            .symbols        = symbols,
                                            .prbs           = freq_hop_prbs,
                                            .freq_hop_grant = prbs,
                                            .occ_cs_idx     = occ_cs_idx});
          if (res_list.size() == nof_res_f1) {
            break;
          }
        }
        if (res_list.size() == nof_res_f1) {
          break;
        }
      }
      if (res_list.size() == nof_res_f1) {
        break;
      }
    }
  }
  // Without intraslot freq. hopping.
  else {
    // Generate resource for increasing RB index, until the num. of required resources is reached.
    for (unsigned rb_idx = 0; rb_idx < bwp_size_rbs / 2 - 1; ++rb_idx) {
      const prb_interval prbs_low_spectrum{rb_idx, rb_idx + 1};
      // Pre-compute the RBs for the resources to be allocated on the upper part of the spectrum. This is to achieve
      // balancing of the PUCCH resources on both sides of the BWP.
      const prb_interval prbs_hi_spectrum{bwp_size_rbs - 1 - rb_idx, bwp_size_rbs - rb_idx};

      // Generate resource for increasing Symbol index, until the num. of required resources is reached.
      for (unsigned sym_idx = 0; sym_idx + params.nof_symbols.to_uint() <= max_nof_symbols.to_uint();
           sym_idx += params.nof_symbols.to_uint()) {
        const ofdm_symbol_range symbols{sym_idx, sym_idx + params.nof_symbols.to_uint()};

        // Allocate all OCC and CS resources, until the num. of required resources is reached.
        for (unsigned occ_cs_idx = 0; occ_cs_idx < nof_occ_css; ++occ_cs_idx) {
          res_list.emplace_back(pucch_grant{.format     = srsran::pucch_format::FORMAT_1,
                                            .symbols    = symbols,
                                            .prbs       = prbs_low_spectrum,
                                            .occ_cs_idx = occ_cs_idx});
          if (res_list.size() == nof_res_f1) {
            break;
          }
        }
        if (res_list.size() == nof_res_f1) {
          break;
        }
      }
      if (res_list.size() == nof_res_f1) {
        break;
      }

      // Repeat the resource allocation on the upper part of the spectrum, to spread the PUCCH resource on both sides of
      // the BWP.
      for (unsigned sym_idx = 0; sym_idx + params.nof_symbols.to_uint() <= max_nof_symbols.to_uint();
           sym_idx += params.nof_symbols.to_uint()) {
        const ofdm_symbol_range symbols{sym_idx, sym_idx + params.nof_symbols.to_uint()};

        for (unsigned occ_cs_idx = 0; occ_cs_idx < nof_occ_css; ++occ_cs_idx) {
          res_list.emplace_back(pucch_grant{.format     = srsran::pucch_format::FORMAT_1,
                                            .symbols    = symbols,
                                            .prbs       = prbs_hi_spectrum,
                                            .occ_cs_idx = occ_cs_idx});
          if (res_list.size() == nof_res_f1) {
            break;
          }
        }
        if (res_list.size() == nof_res_f1) {
          break;
        }
      }

      if (res_list.size() == nof_res_f1) {
        break;
      }
    }
  }

  return res_list;
}

static std::vector<pucch_grant> compute_f2_res(unsigned                         nof_res_f2,
                                               pucch_f2_params                  params,
                                               unsigned                         bwp_size_rbs,
                                               bounded_integer<unsigned, 1, 14> max_nof_symbols)
{
  // Compute the number of symbols and RBs for F2.
  std::vector<pucch_grant> res_list;
  const unsigned           nof_f2_symbols = params.nof_symbols.to_uint();
  const unsigned           f2_max_rbs     = params.max_payload_bits.has_value()
                                                ? get_pucch_format2_max_nof_prbs(params.max_payload_bits.value(),
                                                                   nof_f2_symbols,
                                                                   to_max_code_rate_float(params.max_code_rate))
                                                : params.max_nof_rbs;

  if (f2_max_rbs > pucch_constants::FORMAT2_MAX_NPRB) {
    return {};
  }

  // With intraslot freq. hopping.
  if (params.intraslot_freq_hopping) {
    for (unsigned rb_idx = 0; rb_idx < bwp_size_rbs / 2 - f2_max_rbs; rb_idx += f2_max_rbs) {
      // Generate resource for increasing RB index, until the num. of required resources is reached.
      const prb_interval prbs{rb_idx, rb_idx + f2_max_rbs};
      // Pre-compute the second RBs for the frequency hop specularly to the first RBs.
      const prb_interval freq_hop_prbs{bwp_size_rbs - f2_max_rbs - rb_idx, bwp_size_rbs - rb_idx};

      // Generate resource for increasing Symbol index, until the num. of required resources is reached.
      for (unsigned sym_idx = 0; sym_idx + nof_f2_symbols <= max_nof_symbols.to_uint(); sym_idx += nof_f2_symbols) {
        const ofdm_symbol_range symbols{sym_idx, sym_idx + nof_f2_symbols};

        // Allocate resources for first hop.
        res_list.emplace_back(pucch_grant{.format         = srsran::pucch_format::FORMAT_2,
                                          .symbols        = symbols,
                                          .prbs           = prbs,
                                          .freq_hop_grant = freq_hop_prbs});
        if (res_list.size() == nof_res_f2) {
          break;
        }

        // Allocate resources for second hop.
        res_list.emplace_back(pucch_grant{.format         = srsran::pucch_format::FORMAT_2,
                                          .symbols        = symbols,
                                          .prbs           = freq_hop_prbs,
                                          .freq_hop_grant = prbs});
        if (res_list.size() == nof_res_f2) {
          break;
        }
      }
      if (res_list.size() == nof_res_f2) {
        break;
      }
    }
  }
  // With intraslot freq. hopping.
  else {
    for (unsigned rb_idx = 0; rb_idx < bwp_size_rbs / 2 - f2_max_rbs; rb_idx += f2_max_rbs) {
      const prb_interval prbs_low_spectrum{rb_idx, rb_idx + f2_max_rbs};
      // Pre-compute the RBs for the resources to be allocated on the upper part of the spectrum. This is to achieve
      // balancing of the PUCCH resources on both sides of the BWP.
      const prb_interval prbs_hi_spectrum{bwp_size_rbs - f2_max_rbs - rb_idx, bwp_size_rbs - rb_idx};

      // Generate resource for increasing Symbol index, until the num. of required resources is reached.
      for (unsigned sym_idx = 0; sym_idx + nof_f2_symbols <= max_nof_symbols.to_uint(); sym_idx += nof_f2_symbols) {
        const ofdm_symbol_range symbols{sym_idx, sym_idx + nof_f2_symbols};
        res_list.emplace_back(
            pucch_grant{.format = srsran::pucch_format::FORMAT_2, .symbols = symbols, .prbs = prbs_low_spectrum});
        if (res_list.size() == nof_res_f2) {
          break;
        }
      }
      if (res_list.size() == nof_res_f2) {
        break;
      }

      // Repeat the resource allocation on the upper part of the spectrum, to spread the PUCCH resource on both sides of
      // the BWP.
      for (unsigned sym_idx = 0; sym_idx + nof_f2_symbols <= 14; sym_idx += nof_f2_symbols) {
        const ofdm_symbol_range symbols{sym_idx, sym_idx + nof_f2_symbols};
        res_list.emplace_back(
            pucch_grant{.format = srsran::pucch_format::FORMAT_2, .symbols = symbols, .prbs = prbs_hi_spectrum});
        if (res_list.size() == nof_res_f2) {
          break;
        }
      }

      if (res_list.size() == nof_res_f2) {
        break;
      }
    }
  }

  return res_list;
}

error_type<std::string>
srsran::srs_du::pucch_parameters_validator(unsigned                                       nof_res_f0_f1,
                                           unsigned                                       nof_res_f2,
                                           std::variant<pucch_f1_params, pucch_f0_params> f0_f1_params,
                                           pucch_f2_params                                f2_params,
                                           unsigned                                       bwp_size_rbs,
                                           bounded_integer<unsigned, 1, 14>               max_nof_symbols)
{
  const bool has_f0        = std::holds_alternative<pucch_f0_params>(f0_f1_params);
  unsigned   nof_f0_f1_rbs = 0;
  srsran_assert(max_nof_symbols.to_uint() <= NOF_OFDM_SYM_PER_SLOT_NORMAL_CP, "Invalid number of symbols");

  if (has_f0) {
    const auto& f0_params = std::get<pucch_f0_params>(f0_f1_params);
    // > If intraslot_freq_hopping is enabled, check if PUCCH Format 0 has more than symbol.
    if (has_f0 and f0_params.intraslot_freq_hopping and f0_params.nof_symbols == 1) {
      return make_unexpected("Intra-slot frequency hopping for PUCCH Format 0 requires 2 symbols");
    }

    // We define a block as a set of Resources (either F0/F1 or F2) aligned over the same starting PRB.
    const unsigned nof_f0_per_block = max_nof_symbols.to_uint() / f0_params.nof_symbols.to_uint();
    nof_f0_f1_rbs =
        static_cast<unsigned>(std::ceil(static_cast<float>(nof_res_f0_f1) / static_cast<float>(nof_f0_per_block)));
  } else {
    const auto& f1_params = std::get<pucch_f1_params>(f0_f1_params);
    // > Compute the number of RBs required for the PUCCH Format 1 resources.
    const unsigned nof_occ_codes =
        f1_params.occ_supported ? format1_symb_to_spreading_factor(f1_params.nof_symbols) : 1;

    // We define a block as a set of Resources (either F0/F1 or F2) aligned over the same starting PRB.
    const unsigned nof_f1_per_block = nof_occ_codes * format1_cp_step_to_uint(f1_params.nof_cyc_shifts) *
                                      (max_nof_symbols.to_uint() / f1_params.nof_symbols.to_uint());
    nof_f0_f1_rbs =
        static_cast<unsigned>(std::ceil(static_cast<float>(nof_res_f0_f1) / static_cast<float>(nof_f1_per_block)));
    // With intraslot_freq_hopping, the nof of RBs is an even number.
    if (f1_params.intraslot_freq_hopping) {
      nof_f0_f1_rbs = static_cast<unsigned>(std::ceil(static_cast<float>(nof_f0_f1_rbs) / 2.0F)) * 2;
    }
  }

  // > If intraslot_freq_hopping is enabled, check if PUCCH Format 2 has more than symbol.
  if (f2_params.intraslot_freq_hopping and f2_params.nof_symbols == 1) {
    return make_unexpected("Intra-slot frequency hopping for PUCCH Format 2 requires 2 symbols");
  }

  const unsigned f2_max_rbs = f2_params.max_payload_bits.has_value()
                                  ? get_pucch_format2_max_nof_prbs(f2_params.max_payload_bits.value(),
                                                                   f2_params.nof_symbols.to_uint(),
                                                                   to_max_code_rate_float(f2_params.max_code_rate))
                                  : f2_params.max_nof_rbs;

  if (f2_max_rbs > pucch_constants::FORMAT2_MAX_NPRB) {
    return make_unexpected("The number of PRBs for PUCCH Format 2 exceeds the limit of 16");
  }

  const unsigned nof_f2_blocks = max_nof_symbols.to_uint() / f2_params.nof_symbols.to_uint();
  unsigned       nof_f2_rbs =
      static_cast<unsigned>(std::ceil(static_cast<float>(nof_res_f2) / static_cast<float>(nof_f2_blocks))) * f2_max_rbs;
  // With intraslot_freq_hopping, the nof of RBs is an even number of the PUCCH resource size in RB.
  if (f2_params.intraslot_freq_hopping) {
    nof_f2_rbs = static_cast<unsigned>(std::ceil(static_cast<float>(nof_f2_rbs) / 2.0F)) * 2;
  }

  // Verify the number of RBs for the PUCCH resources does not exceed the BWP size.
  // [Implementation-defined] We do not allow the PUCCH resources to occupy more than 60% of the BWP.
  const float max_allowed_prbs_usage = 0.6F;
  if (static_cast<float>(nof_f0_f1_rbs + nof_f2_rbs) / static_cast<float>(bwp_size_rbs) >= max_allowed_prbs_usage) {
    return make_unexpected("With the given parameters, the number of PRBs for PUCCH exceeds the 60% of the BWP PRBs");
  }

  return {};
}

static std::vector<pucch_resource>
merge_f0_f1_f2_resource_lists(const std::vector<pucch_grant>& pucch_f0_f1_resource_list,
                              const std::vector<pucch_grant>& pucch_f2_resource_list,
                              std::optional<unsigned>         nof_cs,
                              unsigned                        bwp_size_rbs)
{
  // This function merges the lists of PUCCH F0/F1 and F2 resource. It first allocates the F0/F1 resources on the sides
  // of the BWP; second, it allocates the F2 resources beside F0/F1 ones.
  std::vector<pucch_resource> resource_list;
  const bool                  has_f0 = not nof_cs.has_value();

  // NOTE: PUCCH F0/F1 resource are located at the sides of the BWP. PUCCH F2 are located beside the F0/F1 resources,
  // specifically on F0/F1's right (on the frequency axis) for frequencies < BWP/2, and F0/F1's left (on the frequency
  // axis) for frequencies > BWP/2 and < BWP.
  unsigned f0_f1_rbs_occupancy_low_freq = 0;
  unsigned f0_f1_rbs_occupancy_hi_freq  = 0;

  if (has_f0) {
    for (const auto& res_f0 : pucch_f0_f1_resource_list) {
      auto res_id = static_cast<unsigned>(resource_list.size());
      // No need to set res_id.second, which is the PUCCH resource ID for the ASN1 message. This will be set by the DU
      // before assigning the resources to the UE.
      pucch_resource res{.res_id = {res_id, 0}, .starting_prb = res_f0.prbs.start()};
      if (res_f0.freq_hop_grant.has_value()) {
        res.second_hop_prb.emplace(res_f0.freq_hop_grant.value().start());
      }
      pucch_format_0_cfg format0{.initial_cyclic_shift = 0U, .starting_sym_idx = res_f0.symbols.start()};

      // Update the frequency shift for PUCCH F2.
      if (res_f0.prbs.start() < bwp_size_rbs / 2 - 1U) {
        // f0_f1_rbs_occupancy_low_freq accounts for the PUCCH F0/F1 resource occupancy on the first half of the BWP;
        // PUCCH F0/F1 resources are located on the lowest RBs indices.
        f0_f1_rbs_occupancy_low_freq = std::max(f0_f1_rbs_occupancy_low_freq, res_f0.prbs.start() + 1);
        if (res_f0.freq_hop_grant.has_value()) {
          f0_f1_rbs_occupancy_hi_freq =
              std::max(f0_f1_rbs_occupancy_hi_freq, bwp_size_rbs - res_f0.freq_hop_grant.value().start());
        }
      } else if (res_f0.prbs.start() > bwp_size_rbs / 2) {
        // f0_f1_rbs_occupancy_hi_freq accounts for the PUCCH F0/F1 resource occupancy on the second half of the BWP;
        // PUCCH F0/F1 resources are located on the highest RBs indices.
        f0_f1_rbs_occupancy_hi_freq = std::max(f0_f1_rbs_occupancy_hi_freq, bwp_size_rbs - res_f0.prbs.start());
        if (res_f0.freq_hop_grant.has_value()) {
          f0_f1_rbs_occupancy_low_freq = std::max(f0_f1_rbs_occupancy_low_freq, res_f0.freq_hop_grant.value().start());
        }
      } else {
        srsran_assert(false, "PUCCH resources are not expected to be allocated at the centre of the BWP");
        return {};
      }

      format0.nof_symbols = res_f0.symbols.length();
      res.format_params.emplace<pucch_format_0_cfg>(format0);
      res.format = pucch_format::FORMAT_0;
      resource_list.emplace_back(res);
    }
  } else {
    for (const auto& res_f1 : pucch_f0_f1_resource_list) {
      auto res_id = static_cast<unsigned>(resource_list.size());
      // No need to set res_id.second, which is the PUCCH resource ID for the ASN1 message. This will be set by the DU
      // before assigning the resources to the UE.
      pucch_resource res{.res_id = {res_id, 0}, .starting_prb = res_f1.prbs.start()};
      if (res_f1.freq_hop_grant.has_value()) {
        res.second_hop_prb.emplace(res_f1.freq_hop_grant.value().start());
      }
      pucch_format_1_cfg format1{.starting_sym_idx = res_f1.symbols.start()};

      // Update the frequency shift for PUCCH F2.
      if (res_f1.prbs.start() < bwp_size_rbs / 2 - 1) {
        // f0_f1_rbs_occupancy_low_freq accounts for the PUCCH F0/F1 resource occupancy on the first half of the BWP;
        // PUCCH F0/F1 resources are located on the lowest RBs indices.
        f0_f1_rbs_occupancy_low_freq = std::max(f0_f1_rbs_occupancy_low_freq, res_f1.prbs.start() + 1);
        if (res_f1.freq_hop_grant.has_value()) {
          f0_f1_rbs_occupancy_hi_freq =
              std::max(f0_f1_rbs_occupancy_hi_freq, bwp_size_rbs - res_f1.freq_hop_grant.value().start());
        }
      } else if (res_f1.prbs.start() > bwp_size_rbs / 2) {
        // f0_f1_rbs_occupancy_hi_freq accounts for the PUCCH F0/F1 resource occupancy on the second half of the BWP;
        // PUCCH F0/F1 resources are located on the highest RBs indices.
        f0_f1_rbs_occupancy_hi_freq = std::max(f0_f1_rbs_occupancy_hi_freq, bwp_size_rbs - res_f1.prbs.start());
        if (res_f1.freq_hop_grant.has_value()) {
          f0_f1_rbs_occupancy_low_freq = std::max(f0_f1_rbs_occupancy_low_freq, res_f1.freq_hop_grant.value().start());
        }
      } else {
        srsran_assert(false, "PUCCH resources are not expected to be allocated at the centre of the BWP");
        return {};
      }

      format1.nof_symbols = res_f1.symbols.length();
      srsran_assert(res_f1.occ_cs_idx.has_value(),
                    "The index needed to compute OCC code and cyclic shift have not been found");
      format1.initial_cyclic_shift = occ_cs_index_to_cyclic_shift(res_f1.occ_cs_idx.value(), nof_cs.value());
      format1.time_domain_occ      = occ_cs_index_to_occ(res_f1.occ_cs_idx.value(), nof_cs.value());
      res.format_params.emplace<pucch_format_1_cfg>(format1);
      res.format = pucch_format::FORMAT_1;
      resource_list.emplace_back(res);
    }
  }

  for (const auto& res_f2 : pucch_f2_resource_list) {
    auto res_id = static_cast<unsigned>(resource_list.size());
    // No need to set res_id.second, which is the PUCCH resource ID for the ASN1 message. This will be set by the DU
    // before assigning the resources to the UE.
    pucch_resource res{.res_id = {res_id, 0}};
    // Shift F2 RBs depending on previously allocated F0/F1 resources.
    if (res_f2.prbs.start() < bwp_size_rbs / 2 - res_f2.prbs.length()) {
      res.starting_prb = res_f2.prbs.start() + f0_f1_rbs_occupancy_low_freq;
      if (res_f2.freq_hop_grant.has_value()) {
        res.second_hop_prb.emplace(res_f2.freq_hop_grant.value().start() - f0_f1_rbs_occupancy_hi_freq);
      }
    } else if (res_f2.prbs.start() > bwp_size_rbs / 2) {
      res.starting_prb = res_f2.prbs.start() - f0_f1_rbs_occupancy_hi_freq;
      if (res_f2.freq_hop_grant.has_value()) {
        res.second_hop_prb.emplace(res_f2.freq_hop_grant.value().start() + f0_f1_rbs_occupancy_low_freq);
      }
    } else {
      srsran_assert(false, "PUCCH resources are not expected to be allocated at the centre of the BWP");
      return {};
    }

    pucch_format_2_3_cfg format2{.starting_sym_idx = res_f2.symbols.start()};
    format2.nof_symbols = res_f2.symbols.length();
    format2.nof_prbs    = res_f2.prbs.length();
    res.format_params.emplace<pucch_format_2_3_cfg>(format2);
    res.format = pucch_format::FORMAT_2;
    resource_list.emplace_back(res);
  }
  return resource_list;
}

std::vector<pucch_resource>
srsran::srs_du::generate_cell_pucch_res_list(unsigned                                       nof_res_f0_f1,
                                             unsigned                                       nof_res_f2,
                                             std::variant<pucch_f1_params, pucch_f0_params> f0_f1_params,
                                             pucch_f2_params                                f2_params,
                                             unsigned                                       bwp_size_rbs,
                                             bounded_integer<unsigned, 1, 14>               max_nof_symbols)
{
  auto outcome =
      pucch_parameters_validator(nof_res_f0_f1, nof_res_f2, f0_f1_params, f2_params, bwp_size_rbs, max_nof_symbols);
  if (not outcome.has_value()) {
    srsran_assertion_failure("The cell list could not be generated due to: {}", outcome.error());
    return {};
  }

  const bool has_f0 = std::holds_alternative<pucch_f0_params>(f0_f1_params);

  // Compute the PUCCH F0/F1 and F2 resources separately.
  std::vector<pucch_grant> pucch_f0_f1_resource_list;
  unsigned                 nof_css = 0;
  if (has_f0 and nof_res_f0_f1 > 0) {
    const pucch_f0_params f0_params = std::get<pucch_f0_params>(f0_f1_params);
    pucch_f0_f1_resource_list       = compute_f0_res(nof_res_f0_f1, f0_params, bwp_size_rbs, max_nof_symbols);
  } else if (nof_res_f0_f1 > 0) {
    const pucch_f1_params f1_params = std::get<pucch_f1_params>(f0_f1_params);
    const unsigned        nof_occ_codes =
        f1_params.occ_supported ? format1_symb_to_spreading_factor(f1_params.nof_symbols) : 1;
    nof_css = format1_cp_step_to_uint(f1_params.nof_cyc_shifts);
    pucch_f0_f1_resource_list =
        compute_f1_res(nof_res_f0_f1, f1_params, bwp_size_rbs, nof_css * nof_occ_codes, max_nof_symbols);
  }

  const std::vector<pucch_grant> pucch_f2_resource_list =
      nof_res_f2 > 0 ? compute_f2_res(nof_res_f2, f2_params, bwp_size_rbs, max_nof_symbols)
                     : std::vector<pucch_grant>{};

  auto res_list = merge_f0_f1_f2_resource_lists(pucch_f0_f1_resource_list,
                                                pucch_f2_resource_list,
                                                has_f0 ? std::nullopt : std::optional<unsigned>{nof_css},
                                                bwp_size_rbs);

  if (res_list.size() > pucch_constants::MAX_NOF_CELL_PUCCH_RESOURCES) {
    srsran_assertion_failure("With the given parameters, the number of PUCCH resources generated for the "
                             "cell exceeds maximum supported limit of {}",
                             pucch_constants::MAX_NOF_CELL_PUCCH_RESOURCES);
    return {};
  }

  return res_list;
}

static unsigned
cell_res_list_and_params_validator(const std::vector<pucch_resource>&                  res_list,
                                   bounded_integer<unsigned, 1, max_ue_f0_f1_res_harq> nof_ue_pucch_f0_f1_res_harq,
                                   bounded_integer<unsigned, 1, max_ue_f2_res_harq>    nof_ue_pucch_f2_res_harq,
                                   unsigned                                            nof_harq_pucch_cfgs,
                                   unsigned                                            nof_cell_pucch_f1_res_sr,
                                   unsigned                                            nof_cell_pucch_f2_res_csi)
{
  const unsigned FAILURE_CASE = 0U;

  auto count_resources = [&res_list](pucch_format format) {
    unsigned cnt = 0;
    for (const auto& it : res_list) {
      if (it.format == format) {
        ++cnt;
      }
    }
    return cnt;
  };

  const auto contain_format_0 = std::find_if(res_list.begin(), res_list.end(), [](const pucch_resource& res) {
                                  return res.format == pucch_format::FORMAT_0;
                                }) != res_list.end();

  if (contain_format_0) {
    if (nof_ue_pucch_f0_f1_res_harq.to_uint() > 6U) {
      srsran_assertion_failure("With Format 0, nof_ue_pucch_f0_f1_res_harq cannot be greater than 6, as 2 "
                               "resources in set 0 are reserved.");
      return FAILURE_CASE;
    }
    if (nof_ue_pucch_f2_res_harq.to_uint() > 6U) {
      srsran_assertion_failure("With Format 0, nof_ue_pucch_f2_res_harq cannot be greater than 6, as 2 "
                               "resources in set 1 are reserved.");
      return FAILURE_CASE;
    }
  }

  const unsigned tot_nof_f0_res = count_resources(pucch_format::FORMAT_0);
  const unsigned tot_nof_f1_res = count_resources(pucch_format::FORMAT_1);
  const unsigned tot_nof_f2_res = count_resources(pucch_format::FORMAT_2);

  if (tot_nof_f0_res != 0 and tot_nof_f1_res != 0) {
    srsran_assertion_failure("The cell PUCCH resource list can contain either F0 or F1 PUCCH resources, but not both.");
    return FAILURE_CASE;
  }

  const unsigned tot_nof_f0_f1_res = tot_nof_f0_res + tot_nof_f1_res;

  if (tot_nof_f0_f1_res + tot_nof_f2_res != res_list.size()) {
    srsran_assertion_failure(
        "The sum of F0/F1 and F2 PUCCH resources must be equal to the cell PUCCH resource list size.");
    return FAILURE_CASE;
  }

  if (tot_nof_f0_f1_res < 2 or tot_nof_f2_res < 2) {
    srsran_assertion_failure("The cell PUCCH resource list must contain at least 2 F0/F1 and 2 F2 PUCCH resources.");
    return FAILURE_CASE;
  }

  if (nof_ue_pucch_f0_f1_res_harq.to_uint() > tot_nof_f0_f1_res - nof_cell_pucch_f1_res_sr or
      nof_ue_pucch_f2_res_harq.to_uint() > tot_nof_f2_res - nof_cell_pucch_f2_res_csi) {
    srsran_assertion_failure(
        "The nof requested UE PUCCH resources is greater than the nof of resources available in the cell.");
    return FAILURE_CASE;
  }

  if ((nof_ue_pucch_f0_f1_res_harq.to_uint() * nof_harq_pucch_cfgs > tot_nof_f0_f1_res - nof_cell_pucch_f1_res_sr) or
      (nof_ue_pucch_f2_res_harq.to_uint() * nof_harq_pucch_cfgs > tot_nof_f2_res - nof_cell_pucch_f2_res_csi)) {
    srsran_assertion_failure(
        "The cell PUCCH resource list doesn't contain enough resources to allocate all requested UEs.");
    return FAILURE_CASE;
  }

  for (unsigned res_idx = 0; res_idx != tot_nof_f0_f1_res; ++res_idx) {
    if (res_list[res_idx].format == pucch_format::FORMAT_2) {
      srsran_assertion_failure("The F0/F1 resources in the cell PUCCH resource list must precede all F2 resources.");
      return FAILURE_CASE;
    }
  }

  return tot_nof_f0_res != 0 ? tot_nof_f0_res : tot_nof_f1_res;
}

bool srsran::srs_du::ue_pucch_config_builder(
    serving_cell_config&                                serv_cell_cfg,
    const std::vector<pucch_resource>&                  res_list,
    unsigned                                            du_harq_set_idx,
    unsigned                                            du_sr_res_idx,
    unsigned                                            du_csi_res_idx,
    bounded_integer<unsigned, 1, max_ue_f0_f1_res_harq> nof_ue_pucch_f0_f1_res_harq,
    bounded_integer<unsigned, 1, max_ue_f2_res_harq>    nof_ue_pucch_f2_res_harq,
    unsigned                                            nof_harq_pucch_sets,
    unsigned                                            nof_cell_pucch_f0_f1_res_sr,
    unsigned                                            nof_cell_pucch_f2_res_csi)
{
  const unsigned tot_nof_cell_f0_f1_res = cell_res_list_and_params_validator(res_list,
                                                                             nof_ue_pucch_f0_f1_res_harq,
                                                                             nof_ue_pucch_f2_res_harq,
                                                                             nof_harq_pucch_sets,
                                                                             nof_cell_pucch_f0_f1_res_sr,
                                                                             nof_cell_pucch_f2_res_csi);

  if (tot_nof_cell_f0_f1_res == 0U) {
    return false;
  }

  // PUCCH resource ID corresponding to \c pucch-ResourceId, as part of \c PUCCH-Resource, in \c PUCCH-Config,
  // TS 38.331. By default, we index the PUCCH resource ID for ASN1 message from 0 to pucch_res_list.size() - 1.
  unsigned ue_pucch_res_id = 0;

  pucch_config& pucch_cfg = serv_cell_cfg.ul_config.value().init_ul_bwp.pucch_cfg.value();
  // Clears current PUCCH resource list and PUCCH resource list set 0 and 1.
  pucch_cfg.pucch_res_list.clear();
  pucch_cfg.pucch_res_set[pucch_res_set_idx_to_uint(pucch_res_set_idx::set_0)].pucch_res_id_list.clear();
  pucch_cfg.pucch_res_set[pucch_res_set_idx_to_uint(pucch_res_set_idx::set_1)].pucch_res_id_list.clear();

  // Ensure the PUCCH resource sets ID are 0 and 1.
  pucch_cfg.pucch_res_set[pucch_res_set_idx_to_uint(pucch_res_set_idx::set_0)].pucch_res_set_id =
      pucch_res_set_idx::set_0;
  pucch_cfg.pucch_res_set[pucch_res_set_idx_to_uint(pucch_res_set_idx::set_1)].pucch_res_set_id =
      pucch_res_set_idx::set_1;

  // Add F1 for HARQ.
  const unsigned f0_f1_idx_offset = (du_harq_set_idx % nof_harq_pucch_sets) * nof_ue_pucch_f0_f1_res_harq.to_uint();
  const bool     is_format_0      = res_list[f0_f1_idx_offset].format == pucch_format::FORMAT_0;

  for (unsigned ue_f1_cnt = 0; ue_f1_cnt < nof_ue_pucch_f0_f1_res_harq.to_uint(); ++ue_f1_cnt) {
    const auto& cell_res = res_list[ue_f1_cnt + f0_f1_idx_offset];

    // Add PUCCH resource to pucch_res_list.
    pucch_cfg.pucch_res_list.emplace_back(pucch_resource{.res_id       = {cell_res.res_id.cell_res_id, ue_pucch_res_id},
                                                         .starting_prb = cell_res.starting_prb,
                                                         .second_hop_prb = cell_res.second_hop_prb,
                                                         .format         = cell_res.format,
                                                         .format_params  = cell_res.format_params});

    // Add PUCCH resource index to pucch_res_id_list of PUCCH resource set id=0.
    pucch_cfg.pucch_res_set[pucch_res_set_idx_to_uint(pucch_res_set_idx::set_0)].pucch_res_id_list.emplace_back(
        pucch_res_id_t{cell_res.res_id.cell_res_id, ue_pucch_res_id});

    // Increment the PUCCH resource ID for ASN1 message.
    ++ue_pucch_res_id;
  }

  const unsigned f0_res_on_csi_prbs_syms_idx = nof_ue_pucch_f0_f1_res_harq.to_uint();
  if (is_format_0 and serv_cell_cfg.csi_meas_cfg.has_value()) {
    // Add PUCCH resource to pucch_res_list.
    pucch_cfg.pucch_res_list.emplace_back(pucch_resource{
        .res_id = {std::numeric_limits<unsigned>::max(), ue_pucch_res_id}, .format = pucch_format::FORMAT_0});

    // Add PUCCH resource index to pucch_res_id_list of PUCCH resource set id=0.
    pucch_cfg.pucch_res_set[pucch_res_set_idx_to_uint(pucch_res_set_idx::set_0)].pucch_res_id_list.emplace_back(
        pucch_res_id_t{std::numeric_limits<unsigned>::max(), ue_pucch_res_id});

    // Increment the PUCCH resource ID for ASN1 message.
    ++ue_pucch_res_id;
  }

  // Add SR resource.
  const unsigned sr_res_idx             = nof_ue_pucch_f0_f1_res_harq.to_uint() * nof_harq_pucch_sets + du_sr_res_idx;
  const auto&    sr_cell_res            = res_list[sr_res_idx];
  const unsigned ue_pucch_res_id_for_sr = ue_pucch_res_id;
  pucch_cfg.pucch_res_list.emplace_back(
      pucch_resource{.res_id         = {sr_cell_res.res_id.cell_res_id, ue_pucch_res_id_for_sr},
                     .starting_prb   = sr_cell_res.starting_prb,
                     .second_hop_prb = sr_cell_res.second_hop_prb,
                     .format         = sr_cell_res.format,
                     .format_params  = sr_cell_res.format_params});
  pucch_cfg.sr_res_list.front().pucch_res_id = pucch_res_id_t{sr_cell_res.res_id.cell_res_id, ue_pucch_res_id_for_sr};
  // Increment the PUCCH resource ID for ASN1 message.
  ++ue_pucch_res_id;

  // For Format 0, map the last resource of the set 0 to the SR resource.
  if (is_format_0) {
    auto& last_harq_res_set_0              = pucch_cfg.pucch_res_list.back();
    last_harq_res_set_0.res_id.cell_res_id = sr_cell_res.res_id.cell_res_id;
    last_harq_res_set_0.res_id.ue_res_id   = ue_pucch_res_id_for_sr;
    last_harq_res_set_0.starting_prb       = sr_cell_res.starting_prb;
    last_harq_res_set_0.second_hop_prb     = sr_cell_res.second_hop_prb;
    last_harq_res_set_0.format             = sr_cell_res.format;
    last_harq_res_set_0.format_params      = sr_cell_res.format_params;
    pucch_cfg.pucch_res_set[pucch_res_set_idx_to_uint(pucch_res_set_idx::set_0)].pucch_res_id_list.emplace_back(
        pucch_res_id_t{sr_cell_res.res_id.cell_res_id, ue_pucch_res_id_for_sr});
  }

  // Add F2 for HARQ.
  const unsigned f2_idx_offset =
      tot_nof_cell_f0_f1_res + (du_harq_set_idx % nof_harq_pucch_sets) * nof_ue_pucch_f2_res_harq.to_uint();
  for (unsigned ue_f2_cnt = 0; ue_f2_cnt < nof_ue_pucch_f2_res_harq.to_uint(); ++ue_f2_cnt) {
    const auto& cell_res = res_list[f2_idx_offset + ue_f2_cnt];
    pucch_cfg.pucch_res_list.emplace_back(pucch_resource{.res_id       = {cell_res.res_id.cell_res_id, ue_pucch_res_id},
                                                         .starting_prb = cell_res.starting_prb,
                                                         .second_hop_prb = cell_res.second_hop_prb,
                                                         .format         = cell_res.format,
                                                         .format_params  = cell_res.format_params});

    // Add PUCCH resource index to pucch_res_id_list of PUCCH resource set id=1.
    pucch_cfg.pucch_res_set[pucch_res_set_idx_to_uint(pucch_res_set_idx::set_1)].pucch_res_id_list.emplace_back(
        pucch_res_id_t{cell_res.res_id.cell_res_id, ue_pucch_res_id});
    // Increment the PUCCH resource ID for ASN1 message.
    ++ue_pucch_res_id;
  }

  if (serv_cell_cfg.csi_meas_cfg.has_value()) {
    // Add CSI resource.
    const unsigned csi_res_idx =
        tot_nof_cell_f0_f1_res + nof_ue_pucch_f2_res_harq.to_uint() * nof_harq_pucch_sets + du_csi_res_idx;
    const auto&    csi_cell_res            = res_list[csi_res_idx];
    const unsigned ue_pucch_res_id_for_csi = ue_pucch_res_id;
    pucch_cfg.pucch_res_list.emplace_back(
        pucch_resource{.res_id         = {csi_cell_res.res_id.cell_res_id, ue_pucch_res_id_for_csi},
                       .starting_prb   = csi_cell_res.starting_prb,
                       .second_hop_prb = csi_cell_res.second_hop_prb,
                       .format         = csi_cell_res.format,
                       .format_params  = csi_cell_res.format_params});
    const auto& csi_params_cfg = std::get<pucch_format_2_3_cfg>(csi_cell_res.format_params);
    std::get<csi_report_config::periodic_or_semi_persistent_report_on_pucch>(
        serv_cell_cfg.csi_meas_cfg->csi_report_cfg_list[0].report_cfg_type)
        .pucch_csi_res_list.front()
        .pucch_res_id = {csi_cell_res.res_id.cell_res_id, ue_pucch_res_id_for_csi};
    // Increment the PUCCH resource ID for ASN1 message.
    ++ue_pucch_res_id;

    if (is_format_0) {
      auto& harq_set_1_res_for_csi              = pucch_cfg.pucch_res_list.back();
      harq_set_1_res_for_csi.res_id.cell_res_id = csi_cell_res.res_id.cell_res_id;
      harq_set_1_res_for_csi.res_id.ue_res_id   = ue_pucch_res_id_for_csi;
      harq_set_1_res_for_csi.starting_prb       = csi_cell_res.starting_prb;
      harq_set_1_res_for_csi.second_hop_prb     = csi_cell_res.second_hop_prb;
      harq_set_1_res_for_csi.format             = csi_cell_res.format;
      harq_set_1_res_for_csi.format_params      = csi_cell_res.format_params;
      pucch_cfg.pucch_res_set[pucch_res_set_idx_to_uint(pucch_res_set_idx::set_1)].pucch_res_id_list.emplace_back(
          pucch_res_id_t{csi_cell_res.res_id.cell_res_id, ue_pucch_res_id_for_csi});

      auto& f0_harq_on_csi_resources          = pucch_cfg.pucch_res_list[f0_res_on_csi_prbs_syms_idx];
      f0_harq_on_csi_resources.starting_prb   = csi_cell_res.starting_prb;
      f0_harq_on_csi_resources.second_hop_prb = csi_cell_res.second_hop_prb;
      f0_harq_on_csi_resources.format_params.emplace<pucch_format_0_cfg>(
          pucch_format_0_cfg{.initial_cyclic_shift = 0U,
                             .nof_symbols          = csi_params_cfg.nof_symbols,
                             .starting_sym_idx     = csi_params_cfg.starting_sym_idx});
    }
  }

  const unsigned f2_res_on_sr_prbs_syms_idx = ue_pucch_res_id;
  if (is_format_0) {
    // Add PUCCH resource to pucch_res_list.
    pucch_cfg.pucch_res_list.emplace_back(pucch_resource{
        .res_id = {std::numeric_limits<unsigned>::max(), ue_pucch_res_id}, .format = pucch_format::FORMAT_2});

    // Add PUCCH resource index to pucch_res_id_list of PUCCH resource set id=0.
    pucch_cfg.pucch_res_set[pucch_res_set_idx_to_uint(pucch_res_set_idx::set_1)].pucch_res_id_list.emplace_back(
        pucch_res_id_t{std::numeric_limits<unsigned>::max(), ue_pucch_res_id});

    // Increment the PUCCH resource ID for ASN1 message.
    ++ue_pucch_res_id;

    auto& f2_harq_on_sr_resources          = pucch_cfg.pucch_res_list[f2_res_on_sr_prbs_syms_idx];
    f2_harq_on_sr_resources.starting_prb   = sr_cell_res.starting_prb;
    f2_harq_on_sr_resources.second_hop_prb = sr_cell_res.second_hop_prb;
    const auto& sr_params_cfg              = std::get<pucch_format_0_cfg>(sr_cell_res.format_params);
    f2_harq_on_sr_resources.format_params.emplace<pucch_format_2_3_cfg>(pucch_format_2_3_cfg{
        .nof_prbs = 1U, .nof_symbols = sr_params_cfg.nof_symbols, .starting_sym_idx = sr_params_cfg.starting_sym_idx});
  }

  return true;
}