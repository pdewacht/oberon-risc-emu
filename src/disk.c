#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "disk.h"

#ifndef STATIC_BUFSIZ
#define STATIC_BUFSIZ static
#endif

enum DiskState {
  diskCommand,
  diskRead,
  diskWrite,
  diskWriting,
};

struct Disk {
  struct RISC_SPI spi;

  enum DiskState state;
  FILE *file;
  uint32_t offset;

  uint32_t rx_buf[128];
  int rx_idx;

  uint32_t tx_buf[128+2];
  int tx_cnt;
  int tx_idx;
};



static uint32_t disk_read(const struct RISC_SPI *spi);
static void disk_write(const struct RISC_SPI *spi, uint32_t value);
static void disk_run_command(struct Disk *disk);
static void seek_sector(FILE *f, uint32_t secnum);
static void read_sector(FILE *f, uint32_t buf[STATIC_BUFSIZ 128]);
static void write_sector(FILE *f, uint32_t buf[STATIC_BUFSIZ 128]);


struct RISC_SPI *disk_new(const char *filename) {
  struct Disk *disk = calloc(1, sizeof(*disk));
  disk->spi = (struct RISC_SPI) {
    .read_data = disk_read,
    .write_data = disk_write
  };

  disk->state = diskCommand;

  if (filename) {
    disk->file = fopen(filename, "rb+");
    if (disk->file == 0) {
      fprintf(stderr, "Can't open file \"%s\": %s\n", filename, strerror(errno));
      exit(1);
    }

    // Check for filesystem-only image, starting directly at sector 1 (DiskAdr 29)
    read_sector(disk->file, &disk->tx_buf[0]);
    disk->offset = (disk->tx_buf[0] == 0x9B1EA38D) ? 0x80002 : 0;
  }

  return &disk->spi;
}

static void disk_write(const struct RISC_SPI *spi, uint32_t value) {
  struct Disk *disk = (struct Disk *)spi;
  disk->tx_idx++;
  switch (disk->state) {
    case diskCommand: {
      if ((uint8_t)value != 0xFF || disk->rx_idx != 0) {
        disk->rx_buf[disk->rx_idx] = value;
        disk->rx_idx++;
        if (disk->rx_idx == 6) {
          disk_run_command(disk);
          disk->rx_idx = 0;
        }
      }
      break;
    }
    case diskRead: {
      if (disk->tx_idx == disk->tx_cnt) {
        disk->state = diskCommand;
        disk->tx_cnt = 0;
        disk->tx_idx = 0;
      }
      break;
    }
    case diskWrite: {
      if (value == 254) {
        disk->state = diskWriting;
      }
      break;
    }
    case diskWriting: {
      if (disk->rx_idx < 128) {
        disk->rx_buf[disk->rx_idx] = value;
      }
      disk->rx_idx++;
      if (disk->rx_idx == 128) {
        write_sector(disk->file, &disk->rx_buf[0]);
      }
      if (disk->rx_idx == 130) {
        disk->tx_buf[0] = 5;
        disk->tx_cnt = 1;
        disk->tx_idx = -1;
        disk->rx_idx = 0;
        disk->state = diskCommand;
      }
      break;
    }
  }
}

static uint32_t disk_read(const struct RISC_SPI *spi) {
  struct Disk *disk = (struct Disk *)spi;
  uint32_t result;
  if (disk->tx_idx >= 0 && disk->tx_idx < disk->tx_cnt) {
    result = disk->tx_buf[disk->tx_idx];
  } else {
    result = 255;
  }
  return result;
}

static void disk_run_command(struct Disk *disk) {
  uint32_t cmd = disk->rx_buf[0];
  uint32_t arg = (disk->rx_buf[1] << 24)
    | (disk->rx_buf[2] << 16)
    | (disk->rx_buf[3] << 8)
    | disk->rx_buf[4];

  switch (cmd) {
    case 81: {
      disk->state = diskRead;
      disk->tx_buf[0] = 0;
      disk->tx_buf[1] = 254;
      seek_sector(disk->file, arg - disk->offset);
      read_sector(disk->file, &disk->tx_buf[2]);
      disk->tx_cnt = 2 + 128;
      break;
    }
    case 88: {
      disk->state = diskWrite;
      seek_sector(disk->file, arg - disk->offset);
      disk->tx_buf[0] = 0;
      disk->tx_cnt = 1;
      break;
    }
    default: {
      disk->tx_buf[0] = 0;
      disk->tx_cnt = 1;
      break;
    }
  }
  disk->tx_idx = -1;
}

static void seek_sector(FILE *f, uint32_t secnum) {
  if (f) {
    fseek(f, secnum * 512, SEEK_SET);
  }
}

static void read_sector(FILE *f, uint32_t buf[STATIC_BUFSIZ 128]) {
  uint8_t bytes[512] = { 0 };
  if (f) {
    fread(bytes, 512, 1, f);
  }
  for (int i = 0; i < 128; i++) {
    buf[i] = (uint32_t)bytes[i*4+0]
      | ((uint32_t)bytes[i*4+1] << 8)
      | ((uint32_t)bytes[i*4+2] << 16)
      | ((uint32_t)bytes[i*4+3] << 24);
  }
}

static void write_sector(FILE *f, uint32_t buf[STATIC_BUFSIZ 128]) {
  if (f) {
    uint8_t bytes[512];
    for (int i = 0; i < 128; i++) {
      bytes[i*4+0] = (uint8_t)(buf[i]      );
      bytes[i*4+1] = (uint8_t)(buf[i] >>  8);
      bytes[i*4+2] = (uint8_t)(buf[i] >> 16);
      bytes[i*4+3] = (uint8_t)(buf[i] >> 24);
    }
    fwrite(bytes, 512, 1, f);
  }
}
