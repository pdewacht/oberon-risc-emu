#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include "raw-serial.h"

struct RawSerial {
  struct RISC_Serial serial;
  int fd_in;
  int fd_out;
};

static uint32_t read_status(const struct RISC_Serial *serial) {
  struct RawSerial *s = (struct RawSerial *)serial;
  struct timeval tv = { 0, 0 };
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(s->fd_in, &rfds);
  int nfds = s->fd_in + 1;
  int r = select(nfds, &rfds, NULL, NULL, &tv);
  return r == 1;
}

static uint32_t read_data(const struct RISC_Serial *serial) {
  struct RawSerial *s = (struct RawSerial *)serial;
  uint8_t byte = 0;
  read(s->fd_in, &byte, 1);
  return byte;
}

static void write_data(const struct RISC_Serial *serial, uint32_t data) {
  struct RawSerial *s = (struct RawSerial *)serial;
  uint8_t byte = (uint8_t)data;
  write(s->fd_out, &byte, 1);
}

static bool set_non_blocking(int fd) {
  int r = fcntl(fd, F_GETFL);
  if (r != -1) {
    int flags = r | O_NONBLOCK;
    r = fcntl(fd, F_SETFL, flags);
  }
  return r != -1;
}

struct RISC_Serial *raw_serial_new(int fd_in, int fd_out) {
  if (!set_non_blocking(fd_in)) {
    fprintf(stderr, "serial fd %d (input): ", fd_in);
    perror(NULL);
    return NULL;
  }
  if (!set_non_blocking(fd_out)) {
    fprintf(stderr, "serial fd %d (output): ", fd_out);
    perror(NULL);
    return NULL;
  }

  struct RawSerial *s = malloc(sizeof(*s));
  if (!s) {
    return NULL;
  }

  *s = (struct RawSerial){
    .serial = {
      .read_status = &read_status,
      .read_data = &read_data,
      .write_data = &write_data
    },
    .fd_in = fd_in,
    .fd_out = fd_out
  };
  return &s->serial;
}
