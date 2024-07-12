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

#include "pdxch_processor_impl.h"
#include "srsran/instrumentation/traces/du_traces.h"
#include "srsran/phy/support/resource_grid_reader_empty.h"
#include "srsran/srsvec/zero.h"
#include <complex.h>

// #define ROWS 1536
// #define COLS 1
// // no polar coding
// int startSlot = 7000;
// int endSlot = 9840;
// int flag = 1;


using namespace srsran;

const resource_grid_reader_empty pdxch_processor_impl::empty_rg(MAX_PORTS, MAX_NSYMB_PER_SLOT, MAX_RB);

void pdxch_processor_impl::connect(pdxch_processor_notifier& notifier_)
{
  notifier = &notifier_;
}

pdxch_processor_request_handler& pdxch_processor_impl::get_request_handler()
{
  return *this;
}

pdxch_processor_baseband& pdxch_processor_impl::get_baseband()
{
  return *this;
}

bool pdxch_processor_impl::process_symbol(baseband_gateway_buffer_writer&                 samples,
                                          const pdxch_processor_baseband::symbol_context& context)
{
  srsran_assert(notifier != nullptr, "Notifier has not been connected.");

  // Check if the slot has changed.
  if (context.slot != current_slot) {
    // Update slot.
    current_slot = context.slot;

    // Exchange an empty request with the current slot with a stored request.
    auto request = requests.exchange({context.slot, nullptr});

    // Handle the returned request.
    if (request.grid == nullptr) {
      // If the request resource grid pointer is nullptr, the request is empty.
      current_grid = empty_rg;
      return false;
    }

    if (current_slot != request.slot) {
      // If the slot of the request does not match the current slot, then notify a late event.
      resource_grid_context late_context;
      late_context.slot   = request.slot;
      late_context.sector = context.sector;
      notifier->on_pdxch_request_late(late_context);
      current_grid = empty_rg;
      return false;
    }

    // If the request is valid, then select request grid.
    current_grid = *request.grid;
  }

  // Skip processing if the resource grid is empty.
  if (current_grid.get().is_empty()) {
    return false;
  }

  // Symbol index within the subframe.
  unsigned symbol_index_subframe = context.symbol + context.slot.subframe_slot_index() * nof_symbols_per_slot;
  // if(context.slot.slot_index() == 2)
  // {
  
  // // srsran::span<const srsran::cf_t> re;
  // std::reference_wrapper<const srsran::resource_grid_reader> &b = current_grid;
  // srsran::span<srsran::cf_t> x;

  // // // b.get(,0,1,0);
  // auto y = b.get().get_view(0,2);
  // //     // const srsran::resource_grid_reader &
  // // // srsran::span<const srsran::cf_t> a = current_grid.get().get_view(0,2);


  // //printf("z = %.4f + %.4fi,nof_tx_ports = %d\n",(x[0].real()),(x[0].imag()),nof_tx_ports);
  // // y[0 ] = y[0 ] + std::complex<float>((2.0), (2.0));
  // // printf("z = %.4f + %.4fi\n",(x[418].real()),(x[418].imag()));
  // // printf("z = %.4f + %.4fi\n",(x[518].real()),(x[518].imag()));
  // // printf("z = %.4f + %.4fi\n",(x[619].real()),(x[619].imag()));
  // //printf("symbol_index_subframe = %d, SFN = %d, Slot Idx = %d \n",symbol_index_subframe, context.slot.sfn(),context.slot.slot_index());
  // }
//   int slotID = context.slot.sfn() * 10;
//   span<cf_t> output = samples.get_channel_buffer(0);

//       if( slotID > startSlot && slotID < endSlot)
//     {
//       //printf("SFN = %d, Slot_ID = %d, sym = %d, samples = %d \n", pdxch_context.slot.sfn(),pdxch_context.slot.slot_index(),i_symbol, samples.get_nof_samples());
// /*--------------------------------------------------------------------------------------------------------------------------*/
//     //printf("slotId = %d \n",slotId);
//     char fullfilename[200];
//     // Set the path appropriately
//     sprintf(fullfilename, "/home/vm1/Desktop/txFolderBin/underlay_grid%d_%d.bin",slotID + context.slot.slot_index(),symbol_index_subframe);
//     //sprintf(fullfilename, "/mnt/ramdisk/txFolderBin/underlay_grid%d.bin",slotId);
//     // sprintf(fullfilename, "/home/ric/Desktop/Chapter8/V3/PolarCoding/txFolderBin/underlay_grid%d.bin",slotId);
//     FILE *fp = fopen(fullfilename,"rb");
//     float *real_part = (float *) malloc(ROWS*COLS*sizeof(float));
//     float *imag_part = (float *) malloc(ROWS*COLS*sizeof(float));
//     int a = fread(real_part, sizeof(float), ROWS*COLS, fp);
//     int b = fread(imag_part, sizeof(float), ROWS*COLS, fp);
//     printf("a = %d, b = %d\n",a,b);
//     //int c = a+b;
//     fclose(fp);
//     //printf("%f + %fi, %d, %d\n",crealf(real_part[0]), crealf(imag_part[0]), a , b);
//     if(symbol_index_subframe != 0){
//       for(int i = 0;i<1536;i++){
//         // output[54 + i] = output[54 + i] + (crealf(real_part[i]) + I * cimagf(imag_part[i])); 
//         //printf("z = %.4f + %.4fi, %d, %d \t",(output[108 + i].real()),(output[108 + i].imag()),a,b);
//         output[108 + i] = output[108 + i] + std::complex<float>(crealf(real_part[i]), crealf(imag_part[i]));
        
//         //printf("z = %.4f + %.4fi\n",(output[108 + i].real()),(output[108 + i].imag()));
//       }

//     }
//     else{
//       for(int i = 0;i<1536;i++){
//         // output[60 + i] = output[60 + i] + (crealf(real_part[i]) + I * cimagf(imag_part[i])); 
//         output[120 + i] = output[120 + i] + std::complex<float>(crealf(real_part[i]), crealf(imag_part[i]));
//       }

//     }
//     // // free the memory
//     free(real_part);
//     free(imag_part);

// /*--------------------------------------------------------------------------------------------------------------------------*/ 
//   }

  // current_grid[i];
  
  // Modulate each of the ports.
  for (unsigned i_port = 0; i_port != nof_tx_ports; ++i_port) {
    modulator->modulate(samples.get_channel_buffer(i_port), current_grid, i_port, symbol_index_subframe, context.slot.sfn(),context.slot.slot_index());
  }

  return true;
}

void pdxch_processor_impl::handle_request(const resource_grid_reader& grid, const resource_grid_context& context)
{
  srsran_assert(notifier != nullptr, "Notifier has not been connected.");

  // Swap the new request by the current request in the circular array.
  auto request = requests.exchange({context.slot, &grid});

  // If there was a request with a resource grid, then notify a late event with the context of the discarded request.
  if (request.grid != nullptr) {
    resource_grid_context late_context;
    late_context.slot   = request.slot;
    late_context.sector = context.sector;
    notifier->on_pdxch_request_late(late_context);
    l1_tracer << instant_trace_event{"on_pdxch_request_late", instant_trace_event::cpu_scope::thread};
  }
}
