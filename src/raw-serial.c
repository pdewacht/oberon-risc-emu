#ifdef _WIN32

#include <windows.h>
#include <stdio.h>
#include "raw-serial.h"

struct RawSerial {
  struct RISC_Serial serial;
  HANDLE handle;
};

static uint32_t read_status(const struct RISC_Serial *serial) {
  struct RawSerial *s = (struct RawSerial *)serial;
  DWORD available_bytes;
  if (!PeekNamedPipe(s->handle, 0, 0, 0, &available_bytes, 0))
    available_bytes = 0;
  return 2 + (available_bytes > 0);
}

static uint32_t read_data(const struct RISC_Serial *serial) {
  if (read_status(serial) != 3) return 0;
  struct RawSerial *s = (struct RawSerial *)serial;
  uint8_t byte = 0;
  DWORD numBytesRead = 0;
  while (numBytesRead == 0) {
    if (!ReadFile(s->handle, &byte, 1, &numBytesRead, NULL))
      return 0;
  }
  return byte;
}

static void write_data(const struct RISC_Serial *serial, uint32_t data) {
  struct RawSerial *s = (struct RawSerial *)serial;
  uint8_t byte = (uint8_t)data;
  DWORD numBytesWritten = 0;
  while (numBytesWritten == 0) {
    if (!WriteFile(s->handle, &byte, 1, &numBytesWritten, NULL))
      return;
  }
}

struct RISC_Serial *raw_serial_new(const char *filename_in, const char *filename_out) {
  char pipe_name[257];
  HANDLE file_handle;

  if (strcmp(filename_out, "/dev/null") == 0) {
    if (filename_in[0] == '\\' && strlen(filename_in) <= 256) {
      strcpy(pipe_name, filename_in);
    } else if (strlen(filename_in) <= 256 - 9) {
      strcpy(pipe_name, "\\\\.\\pipe\\");
      strcat(pipe_name, filename_in);
    } else {
      fprintf(stderr, "Invalid --serial-in pipe name.\n");
      return NULL;
    }
    file_handle = CreateNamedPipe(pipe_name, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS, 1, 256, 256, 0, NULL);
    if (file_handle == NULL || file_handle == INVALID_HANDLE_VALUE) {
      fprintf(stderr, "Failed to create outbound pipe instance.");
      return NULL;
    }
    fprintf(stderr, "Waiting for client to connect.\n");
    if (!ConnectNamedPipe(file_handle, NULL)) {
      fprintf(stderr, "Failed to accept connection to named pipe.\n");
      CloseHandle(file_handle);
      return NULL;
    }
  } else if (strcmp(filename_in, "/dev/null") == 0) {
    if (filename_out[0] == '\\' && strlen(filename_out) <= 256) {
      strcpy(pipe_name, filename_out);
    } else if (strlen(filename_out) <= 256 - 9) {
      strcpy(pipe_name, "\\\\.\\pipe\\");
      strcat(pipe_name, filename_out);
    } else {
      fprintf(stderr, "Invalid --serial-out pipe/device name.\n");
      return NULL;
    }
    file_handle = CreateFile(pipe_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, NULL);
    if (file_handle == NULL || file_handle == INVALID_HANDLE_VALUE) {
      fprintf(stderr, "Failed to connect to pipe/device.\n");
      return NULL;
    }
  } else {
    fprintf(stderr, "On Windows, either specify --serial-out to connect to a named pipe or --serial-in to create one.\n");
    return NULL;
  }

  struct RawSerial *s = malloc(sizeof(*s));
  if (!s) {
    fprintf(stderr, "Allocate structure failed.\n");
    CloseHandle(file_handle);
    return NULL;
  }

  *s = (struct RawSerial) {
    .serial = {
      .read_status = &read_status,
      .read_data = &read_data,
      .write_data = &write_data
    },
    .handle = file_handle
  };

  return &s->serial;
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
