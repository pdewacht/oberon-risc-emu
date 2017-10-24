#ifdef _WIN32

#include <stdio.h>

struct RISC_Serial *raw_serial_new(int fd_in, int fd_out) {
  fprintf(stderr, "The --serial-fd feature is not available on Windows.\n");
  return NULL;
}

#else  // _WIN32

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

static int max(int a, int b) {
  return a > b ? a : b;
}

static uint32_t read_status(const struct RISC_Serial *serial) {
  struct RawSerial *s = (struct RawSerial *)serial;
  struct timeval tv = { 0, 0 };
  fd_set read_fds;
  fd_set write_fds;
  FD_ZERO(&read_fds);
  FD_SET(s->fd_in, &read_fds);
  FD_ZERO(&write_fds);
  FD_SET(s->fd_out, &write_fds);
  int nfds = max(s->fd_in, s->fd_out) + 1;
  int r = select(nfds, &read_fds, &write_fds, NULL, &tv);
  int status = 0;
  if (r > 0) {
    if (FD_ISSET(s->fd_in, &read_fds)) {
      status |= 1;
    }
    if (FD_ISSET(s->fd_out, &write_fds)) {
      status |= 2;
    }
  }
  return status;
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

struct RISC_Serial *raw_serial_new(const char *filename_in, const char *filename_out) {
  int fd_in, fd_out;

  fd_in = open(filename_in, O_RDONLY | O_NONBLOCK);
  if (fd_in < 0) {
    perror("Failed to open serial input file");
    goto fail1;
  }

  fd_out = open(filename_out, O_RDWR | O_NONBLOCK);
  if (fd_out < 0) {
    perror("Failed to open serial output file");
    goto fail2;
  }

  struct RawSerial *s = malloc(sizeof(*s));
  if (!s) {
    goto fail3;
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

 fail3:
  close(fd_out);
 fail2:
  close(fd_in);
 fail1:
  return NULL;
}

#endif  // _WIN32
