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

#include "lib/gtpu/gtpu_pdu.h"
#include "srsran/gtpu/gtpu_tunnel_rx.h"
#include "srsran/gtpu/gtpu_tunnel_tx.h"
#include <gtest/gtest.h>

namespace srsran {

const uint8_t gtpu_ping_vec[] = {
    0x30, 0xff, 0x00, 0x54, 0x00, 0x00, 0x00, 0x01, 0x45, 0x00, 0x00, 0x54, 0xe8, 0x83, 0x40, 0x00, 0x40, 0x01, 0xfa,
    0x00, 0xac, 0x10, 0x00, 0x03, 0xac, 0x10, 0x00, 0x01, 0x08, 0x00, 0x2c, 0xbe, 0xb4, 0xa4, 0x00, 0x01, 0xd3, 0x45,
    0x61, 0x63, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x20, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14,
    0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37};

const uint8_t gtpu_ping_ext_vec[] = {
    0x34, 0xff, 0x00, 0x5c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x85, 0x01, 0x00, 0x01, 0x00, 0x45,
    0x00, 0x00, 0x54, 0x1f, 0x6c, 0x00, 0x00, 0x40, 0x01, 0x46, 0x9c, 0x0a, 0x2d, 0x00, 0x01, 0x0a, 0x2d,
    0x00, 0x47, 0x00, 0x00, 0x86, 0xb0, 0x00, 0x04, 0x00, 0x0d, 0x01, 0x70, 0xc1, 0x63, 0x00, 0x00, 0x00,
    0x00, 0xf0, 0x97, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37};

const uint8_t gtpu_ping_two_ext_vec[] = {
    0x34, 0xff, 0x00, 0x5c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x01, 0x85, 0x01, 0x00,
    0x01, 0x00, 0x45, 0x00, 0x00, 0x54, 0x1f, 0x6c, 0x00, 0x00, 0x40, 0x01, 0x46, 0x9c, 0x0a, 0x2d, 0x00, 0x01,
    0x0a, 0x2d, 0x00, 0x47, 0x00, 0x00, 0x86, 0xb0, 0x00, 0x04, 0x00, 0x0d, 0x01, 0x70, 0xc1, 0x63, 0x00, 0x00,
    0x00, 0x00, 0xf0, 0x97, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37};

class gtpu_test_rx_lower : public gtpu_tunnel_rx_lower_layer_notifier
{
  void on_new_sdu(byte_buffer buf) final { last_rx = std::move(buf); }

public:
  byte_buffer last_rx;
};
class gtpu_test_tx_upper : public gtpu_tunnel_tx_upper_layer_notifier
{
  void on_new_pdu(byte_buffer buf, const ::sockaddr_storage& /*addr*/) final { last_tx = std::move(buf); }

public:
  byte_buffer last_tx;
};

class gtpu_test_rx_upper : public gtpu_tunnel_rx_upper_layer_interface
{
public:
  void handle_pdu(byte_buffer pdu) final { last_rx = std::move(pdu); }

  byte_buffer last_rx;
};

/// Fixture class for GTP-U PDU tests
class gtpu_test : public ::testing::Test
{
public:
  gtpu_test() :
    logger(srslog::fetch_basic_logger("TEST", false)), gtpu_logger(srslog::fetch_basic_logger("GTPU", false))
  {
  }

protected:
  void SetUp() override
  {
    // init test's logger
    srslog::init();
    logger.set_level(srslog::basic_levels::debug);
    logger.set_hex_dump_max_size(100);

    // init GTPU logger
    gtpu_logger.set_level(srslog::basic_levels::debug);
    gtpu_logger.set_hex_dump_max_size(100);
  }

  void TearDown() override
  {
    // flush logger after each test
    srslog::flush();
  }

  // Test logger
  srslog::basic_logger& logger;

  // GTP-U logger
  srslog::basic_logger& gtpu_logger;
  gtpu_tunnel_logger    gtpu_rx_logger{"GTPU", {0, 1, "DL"}};
  gtpu_tunnel_logger    gtpu_tx_logger{"GTPU", {0, 1, "UL"}};
};
} // namespace srsran
