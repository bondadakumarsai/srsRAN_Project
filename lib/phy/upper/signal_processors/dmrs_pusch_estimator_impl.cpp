#include "dmrs_pusch_estimator_impl.h"
#include "dmrs_helper.h"
#include "srsgnb/srsvec/copy.h"
#include "srsgnb/srsvec/sc_prod.h"

using namespace srsgnb;

namespace {
constexpr std::array<bool, NRE> RE_PATTERN_TYPE1_DELTA0 =
    {true, false, true, false, true, false, true, false, true, false, true, false};
constexpr std::array<bool, NRE> RE_PATTERN_TYPE1_DELTA1 =
    {false, true, false, true, false, true, false, true, false, true, false, true};
constexpr std::array<bool, NRE> RE_PATTERN_TYPE2_DELTA0 =
    {true, true, false, false, false, false, true, true, false, false, false, false};
constexpr std::array<bool, NRE> RE_PATTERN_TYPE2_DELTA2 =
    {false, false, true, true, false, false, false, false, true, true, false, false};
constexpr std::array<bool, NRE> RE_PATTERN_TYPE2_DELTA4 =
    {false, false, false, false, true, true, false, false, false, false, true, true};
} // namespace

const std::array<dmrs_pusch_estimator_impl::parameters, dmrs_type::DMRS_MAX_PORTS_TYPE1>
    dmrs_pusch_estimator_impl::params_type1 = {{
        /* Port 1000 */ {RE_PATTERN_TYPE1_DELTA0, {+1.0F, +1.0F}, {+1.0F, +1.0F}},
        /* Port 1001 */ {RE_PATTERN_TYPE1_DELTA0, {+1.0F, -1.0F}, {+1.0F, +1.0F}},
        /* Port 1002 */ {RE_PATTERN_TYPE1_DELTA1, {+1.0F, +1.0F}, {+1.0F, +1.0F}},
        /* Port 1003 */ {RE_PATTERN_TYPE1_DELTA1, {+1.0F, -1.0F}, {+1.0F, +1.0F}},
        /* Port 1004 */ {RE_PATTERN_TYPE1_DELTA0, {+1.0F, +1.0F}, {+1.0F, -1.0F}},
        /* Port 1005 */ {RE_PATTERN_TYPE1_DELTA0, {+1.0F, -1.0F}, {+1.0F, -1.0F}},
        /* Port 1006 */ {RE_PATTERN_TYPE1_DELTA1, {+1.0F, +1.0F}, {+1.0F, -1.0F}},
        /* Port 1007 */ {RE_PATTERN_TYPE1_DELTA1, {+1.0F, -1.0F}, {+1.0F, -1.0F}},
    }};

const std::array<dmrs_pusch_estimator_impl::parameters, dmrs_type::DMRS_MAX_PORTS_TYPE2>
    dmrs_pusch_estimator_impl::params_type2 = {{
        /* Port 1000 */ {RE_PATTERN_TYPE2_DELTA0, {+1.0F, +1.0F}, {+1.0F, +1.0F}},
        /* Port 1001 */ {RE_PATTERN_TYPE2_DELTA0, {+1.0F, -1.0F}, {+1.0F, +1.0F}},
        /* Port 1002 */ {RE_PATTERN_TYPE2_DELTA2, {+1.0F, +1.0F}, {+1.0F, +1.0F}},
        /* Port 1003 */ {RE_PATTERN_TYPE2_DELTA2, {+1.0F, -1.0F}, {+1.0F, +1.0F}},
        /* Port 1004 */ {RE_PATTERN_TYPE2_DELTA4, {+1.0F, +1.0F}, {+1.0F, +1.0F}},
        /* Port 1005 */ {RE_PATTERN_TYPE2_DELTA4, {+1.0F, -1.0F}, {+1.0F, +1.0F}},
        /* Port 1006 */ {RE_PATTERN_TYPE2_DELTA0, {+1.0F, +1.0F}, {+1.0F, -1.0F}},
        /* Port 1007 */ {RE_PATTERN_TYPE2_DELTA0, {+1.0F, -1.0F}, {+1.0F, -1.0F}},
        /* Port 1008 */ {RE_PATTERN_TYPE2_DELTA2, {+1.0F, +1.0F}, {+1.0F, -1.0F}},
        /* Port 1009 */ {RE_PATTERN_TYPE2_DELTA2, {+1.0F, -1.0F}, {+1.0F, -1.0F}},
        /* Port 1010 */ {RE_PATTERN_TYPE2_DELTA4, {+1.0F, +1.0F}, {+1.0F, -1.0F}},
        /* Port 1011 */ {RE_PATTERN_TYPE2_DELTA4, {+1.0F, -1.0F}, {+1.0F, -1.0F}},
    }};

void dmrs_pusch_estimator_impl::estimate(channel_estimate&           estimate,
                                         const resource_grid_reader& grid,
                                         const configuration&        config)
{
  unsigned nof_tx_layers = config.nof_tx_layers;
  unsigned nof_rx_ports  = config.rx_ports.size();

  // Select the DM-RS pattern for this PUSCH transmission.
  span<dmrs_pattern> coordinates = span<dmrs_pattern>(temp_coordinates).first(nof_tx_layers);

  // Number of OFDM symbols carrying DM-RS.
  unsigned nof_dmrs_symbols = std::count(config.symbols_mask.begin(), config.symbols_mask.end(), true);

  // Number of DM-RS symbols per OFDM symbol.
  unsigned nof_dmrs_per_symbol = config.rb_mask.count() * config.type.nof_dmrs_per_rb();

  // Prepare symbol buffer.
  temp_symbols.resize(nof_dmrs_per_symbol, nof_dmrs_symbols, config.nof_tx_layers);

  // Generate symbols and allocation patterns.
  generate(temp_symbols, coordinates, config);

  port_channel_estimator::configuration est_cfg = {};
  est_cfg.rb_mask                               = config.rb_mask;
  est_cfg.scs                                   = to_subcarrier_spacing(config.slot.numerology());
  est_cfg.nof_tx_layers                         = nof_tx_layers;
  est_cfg.first_symbol                          = config.first_symbol;
  est_cfg.nof_symbols                           = config.nof_symbols;
  est_cfg.rx_ports                              = config.rx_ports;

  for (unsigned i_port = 0; i_port != nof_rx_ports; ++i_port) {
    ch_estimator->compute(estimate, grid, i_port, temp_symbols, coordinates, est_cfg);
  }
}

void dmrs_pusch_estimator_impl::sequence_generation(span<cf_t>           sequence,
                                                    unsigned             symbol,
                                                    const configuration& config) const
{
  // Get signal amplitude.
  float amplitude = M_SQRT1_2 / config.scaling;

  // Extract parameters to calculate the PRG initial state.
  unsigned nslot    = config.slot.slot_index();
  unsigned nidnscid = config.scrambling_id;
  unsigned nscid    = config.n_scid ? 1 : 0;
  unsigned nsymb    = get_nsymb_per_slot(cyclic_prefix::NORMAL);

  // Calculate initial sequence state.
  unsigned c_init = ((nsymb * nslot + symbol + 1) * (2 * nidnscid + 1) * pow2(17) + (2 * nidnscid + nscid)) % pow2(31);

  // Initialize sequence.
  prg->init(c_init);

  // Generate sequence.
  dmrs_sequence_generate(
      sequence, *prg, amplitude, DMRS_REF_POINT_K_TO_POINT_A, config.type.nof_dmrs_per_rb(), config.rb_mask);
}

void dmrs_pusch_estimator_impl::generate(dmrs_symbol_list&    dmrs_symbol_buffer,
                                         span<dmrs_pattern>   mask,
                                         const configuration& cfg)
{
  // For each symbol in the transmission generate DMRS for layer 0.
  for (unsigned ofdm_symbol_index = cfg.first_symbol,
                ofdm_symbol_end   = cfg.first_symbol + cfg.nof_symbols,
                dmrs_symbol_index = 0;
       ofdm_symbol_index != ofdm_symbol_end;
       ++ofdm_symbol_index) {
    // Skip symbol if it does not carry DMRS.
    if (!cfg.symbols_mask[ofdm_symbol_index]) {
      continue;
    }

    // Extract parameters to calculate the PRG initial state.
    unsigned nslot    = cfg.slot.slot_index();
    unsigned nidnscid = cfg.scrambling_id;
    unsigned nscid    = cfg.n_scid ? 1 : 0;
    unsigned nsymb    = get_nsymb_per_slot(cyclic_prefix::NORMAL);

    // Calculate initial sequence state.
    unsigned c_init =
        ((nsymb * nslot + ofdm_symbol_index + 1) * (2 * nidnscid + 1) * pow2(17) + (2 * nidnscid + nscid)) % pow2(31);

    // Initialize sequence.
    prg->init(c_init);

    // Select a view to the DM-RS symbols for this OFDM symbol and layer 0.
    span<cf_t> dmrs_symbols = dmrs_symbol_buffer.get_subc(dmrs_symbol_index, 0);

    // Generate DM-RS for PUSCH.
    sequence_generation(dmrs_symbols, ofdm_symbol_index, cfg);

    // Increment DM-RS OFDM symbol index.
    ++dmrs_symbol_index;
  }

  // For each layer...
  for (unsigned tx_layer = 0; tx_layer != cfg.nof_tx_layers; ++tx_layer) {
    // Select layer parameters.
    const parameters& params = (cfg.type == dmrs_type::TYPE1) ? params_type1[tx_layer] : params_type2[tx_layer];

    // Skip copy for layer 0.
    if (tx_layer != 0) {
      // For each symbol containing DMRS...
      for (unsigned symbol = 0, symbol_end = dmrs_symbol_buffer.get_nof_symbols(); symbol != symbol_end; ++symbol) {
        // Get a view of the symbols for the current layer.
        span<cf_t> dmrs = dmrs_symbol_buffer.get_subc(symbol, tx_layer);

        // Get a view of the symbols for layer 0.
        span<const cf_t> dmrs_layer0 = dmrs_symbol_buffer.get_subc(symbol, 0);

        // If a time weight is required...
        if (params.w_t[0] != params.w_t[1] && symbol % 2 == 1) {
          // Apply the weight.
          srsvec::sc_prod(dmrs_layer0, params.w_t[1], dmrs);
          fmt::print("w_t {}\n", tx_layer);
        } else {
          // otherwise, copy symbols from layer 0 to the current layer.
          srsvec::copy(dmrs, dmrs_layer0);
        }

        // If a frequency weight is required.
        if (params.w_f[0] != params.w_f[1]) {
          fmt::print("w_f {}\n", tx_layer);
          // Apply frequency domain mask.
          for (unsigned subc = 1, subc_end = 2 * (dmrs.size() / 2) + 1; subc != subc_end; subc += 2) {
            dmrs[subc] *= params.w_f[1];
          }
        }
      }
    }

    mask[tx_layer].symbols    = cfg.symbols_mask;
    mask[tx_layer].rb_mask    = cfg.rb_mask;
    mask[tx_layer].re_pattern = params.re_pattern;
  }
}
