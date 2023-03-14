/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once
#include "srsran/adt/blocking_queue.h"
#include "srsran/phy/lower/modulation/ofdm_demodulator.h"
#include "srsran/phy/lower/processors/uplink/puxch/puxch_processor.h"
#include "srsran/phy/lower/processors/uplink/puxch/puxch_processor_baseband.h"
#include "srsran/phy/lower/processors/uplink/puxch/puxch_processor_notifier.h"
#include "srsran/phy/lower/processors/uplink/puxch/puxch_processor_request_handler.h"
#include "srsran/phy/support/resource_grid.h"
#include "srsran/ran/slot_point.h"
#include <mutex>

namespace srsran {

class puxch_processor_impl : public puxch_processor,
                             private puxch_processor_baseband,
                             private puxch_processor_request_handler
{
public:
  struct configuration {
    cyclic_prefix cp;
    unsigned      nof_rx_ports;
    unsigned      request_queue_size;
  };

  puxch_processor_impl(std::unique_ptr<ofdm_symbol_demodulator> demodulator_, const configuration& config) :
    nof_symbols_per_slot(get_nsymb_per_slot(config.cp)),
    nof_rx_ports(config.nof_rx_ports),
    demodulator(std::move(demodulator_)),
    request_queue(config.request_queue_size)
  {
    srsran_assert(demodulator, "Invalid demodulator.");
  }

  // See interface for documentation.
  void connect(puxch_processor_notifier& notifier_) override { notifier = &notifier_; }

  // See interface for documentation.
  puxch_processor_request_handler& get_request_handler() override { return *this; }

  // See interface for documentation.
  puxch_processor_baseband& get_baseband() override { return *this; }

private:
  // See interface for documentation.
  void process_symbol(const baseband_gateway_buffer& samples, const lower_phy_rx_symbol_context& context) override;

  // See interface for documentation.
  void handle_request(resource_grid& grid, const resource_grid_context& context) override;

  /// Pairs a slot and resource grid demodulation for the queue.
  struct rg_grid_request {
    slot_point     slot = {};
    resource_grid* grid = nullptr;
  };

  unsigned                                 nof_symbols_per_slot;
  unsigned                                 nof_rx_ports;
  puxch_processor_notifier*                notifier = nullptr;
  std::unique_ptr<ofdm_symbol_demodulator> demodulator;
  slot_point                               current_slot = {};
  resource_grid*                           current_grid = nullptr;
  blocking_queue<rg_grid_request>          request_queue;
};

} // namespace srsran