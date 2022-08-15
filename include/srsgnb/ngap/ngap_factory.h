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

#include "ngap.h"
#include "srsgnb/support/timers.h"
#include <memory>

namespace srsgnb {

namespace srs_cu_cp {

/// Creates an instance of an NGAP interface, notifying outgoing packets on the specified listener object.
std::unique_ptr<ngap_interface> create_ngap(timer_manager& timer_db, ng_message_notifier& event_notifier);

} // namespace srs_cu_cp

} // namespace srsgnb