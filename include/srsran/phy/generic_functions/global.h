// global_var.h
#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H

#define PAYLOAD_SIZE_MASK 22
extern int next_XOR_payload[PAYLOAD_SIZE_MASK];
extern int default_XOR[PAYLOAD_SIZE_MASK];
extern int current_XOR_payload[PAYLOAD_SIZE_MASK];
extern bool global_flag;

#endif // GLOBAL_VAR_H
