#ifndef RISC_SD_H
#define RISC_SD_H

#include <stdint.h>

struct Disk;

struct Disk *disk_new(const char *filename);
void disk_free(struct Disk *disk);

void disk_write(struct Disk *disk, uint32_t value);
uint32_t disk_read(struct Disk *disk);

#endif  // RISC_SD_H
