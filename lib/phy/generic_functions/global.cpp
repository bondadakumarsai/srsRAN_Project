// global_var.cpp
#include "srsran/phy/generic_functions/global.h"
int next_XOR_payload[PAYLOAD_SIZE_MASK] = {0};
int current_XOR_payload[PAYLOAD_SIZE_MASK] = {0};
int default_XOR[PAYLOAD_SIZE_MASK] = {1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0};
//int default_XOR[PAYLOAD_SIZE_MASK] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0};
bool global_flag = false;     // underlay flag: false - No underlay transmission, true - 
