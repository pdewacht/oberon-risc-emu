#ifndef PCLINK_H
#define PCLINK_H

#include <stdbool.h>
#include <stdint.h>

uint32_t PCLink_RData();
uint32_t PCLink_RStat();
void PCLink_TData(uint32_t value);

#endif  // PCLINK_H
