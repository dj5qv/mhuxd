#ifndef MHSTATES_H
#define MHSTATES_H

#include <stdint.h>

int mk2r_get_mok_value(const uint8_t mok_buffer[8], const char *key);
void mk2r_debug_print_mok_values(const uint8_t mok_buffer[8]);
int mk2r_set_hfocus_value(uint8_t hfocus_buffer[4], const char *key, int value);
void mk2r_set_hfocus_from_mok(uint8_t hfocus_buffer[4], const uint8_t mok_buffer[8]);

#endif // MHSTATES_H
