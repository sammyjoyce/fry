#include "tui_pty.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

#include <utils/logging.h>
#include <utils/memory.h>

// Create PTY structure
app_error app_pty_create(app_pty_t **pty) {
  if (!pty) {
    return APP_ERROR_INVALID_ARG;
  }

  app_pty_t *p = app_calloc(1, sizeof(app_pty_t));
  if (!p) {
    return APP_ERROR_MEMORY;
  }

  p->master_fd = -1;
  p->slave_fd = -1;
  p->child_pid = -1;
  p->is_open = false;

  *pty = p;
  return APP_SUCCESS;
}

// Destroy PTY
app_error app_pty_destroy(app_pty_t *pty) {
  if (!pty) {
    return APP_SUCCESS;
  }

  if (pty->is_open) {
    app_pty_close(pty);
  }

  app_free(pty->slave_name);
  app_free(pty);

  return APP_SUCCESS;
}

// Open PTY
app_error app_pty_open(app_pty_t *pty, const app_pty_config_t *config) {
  if (!pty || !config) {
    return APP_ERROR_INVALID_ARG;
  }

  if (pty->is_open) {
    return APP_ERROR_INVALID_ARG;
  }

  // Open PTY master/slave pair
  char name_buf[256];
  int master, slave;

#ifdef __APPLE__
  if (openpty(&master, &slave, name_buf, NULL, NULL) < 0) {
    LOG_ERROR("openpty failed: %s", strerror(errno));
    return APP_ERROR_IO;
  }
#else
  // Linux
  master = posix_openpt(O_RDWR | O_NOCTTY);
  if (master < 0) {
    LOG_ERROR("posix_openpt failed: %s", strerror(errno));
    return APP_ERROR_IO;
  }

  if (grantpt(master) < 0 || unlockpt(master) < 0) {
    LOG_ERROR("grantpt/unlockpt failed: %s", strerror(errno));
    close(master);
    return APP_ERROR_IO;
  }

  const char *slave_name = ptsname(master);
  if (!slave_name) {
    LOG_ERROR("ptsname failed: %s", strerror(errno));
    close(master);
    return APP_ERROR_IO;
  }

  strncpy(name_buf, slave_name, sizeof(name_buf) - 1);
  name_buf[sizeof(name_buf) - 1] = '\0';

  slave = open(slave_name, O_RDWR | O_NOCTTY);
  if (slave < 0) {
    LOG_ERROR("open slave failed: %s", strerror(errno));
    close(master);
    return APP_ERROR_IO;
  }
#endif

  // Set terminal size
  struct winsize ws = {.ws_row = config->rows,
                       .ws_col = config->cols,
                       .ws_xpixel = 0,
                       .ws_ypixel = 0};

  if (ioctl(master, TIOCSWINSZ, &ws) < 0) {
    LOG_WARNING("Failed to set terminal size: %s", strerror(errno));
  }

  pty->master_fd = master;
  pty->slave_fd = slave;
  pty->slave_name = app_strdup(name_buf);
  pty->is_open = true;

  LOG_DEBUG("Opened PTY: master=%d, slave=%d (%s)", master, slave, name_buf);

  return APP_SUCCESS;
}

// Close PTY
app_error app_pty_close(app_pty_t *pty) {
  if (!pty) {
    return APP_ERROR_INVALID_ARG;
  }

  if (!pty->is_open) {
    return APP_SUCCESS;
  }

  if (pty->master_fd >= 0) {
    close(pty->master_fd);
    pty->master_fd = -1;
  }

  if (pty->slave_fd >= 0) {
    close(pty->slave_fd);
    pty->slave_fd = -1;
  }

  pty->is_open = false;

  return APP_SUCCESS;
}

// Set non-blocking mode
app_error app_pty_set_nonblocking(app_pty_t *pty, bool non_blocking) {
  if (!pty || !pty->is_open) {
    return APP_ERROR_INVALID_ARG;
  }

  int flags = fcntl(pty->master_fd, F_GETFL, 0);
  if (flags < 0) {
    return APP_ERROR_IO;
  }

  if (non_blocking) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  if (fcntl(pty->master_fd, F_SETFL, flags) < 0) {
    return APP_ERROR_IO;
  }

  return APP_SUCCESS;
}

// Resize PTY
app_error app_pty_resize(app_pty_t *pty, int rows, int cols) {
  if (!pty || !pty->is_open) {
    return APP_ERROR_INVALID_ARG;
  }

  struct winsize ws = {
      .ws_row = rows, .ws_col = cols, .ws_xpixel = 0, .ws_ypixel = 0};

  if (ioctl(pty->master_fd, TIOCSWINSZ, &ws) < 0) {
    LOG_ERROR("Failed to resize PTY: %s", strerror(errno));
    return APP_ERROR_IO;
  }

  return APP_SUCCESS;
}

// TODO: Implement remaining PTY functions (spawn, read, write, etc.)