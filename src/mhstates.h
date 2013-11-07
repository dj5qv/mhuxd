#ifndef MHSTATES_H
#define MHSTATES_H

#include <stdint.h>

int mk2r_get_mok_value(const uint8_t mok_buffer[8], const char *key);
int mk2_get_mpk_value(const uint8_t mpk_buffer[4], const char *key);
void mk2r_debug_print_mok_values(const uint8_t mok_buffer[8]);
void mk2_debug_print_mpk_values(const uint8_t mpk_buffer[4]);

#endif // MHSTATES_H
