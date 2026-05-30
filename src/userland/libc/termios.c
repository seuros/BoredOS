// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.
#include "termios.h"
#include <errno.h>

int tcgetattr(int fd, struct termios *termios_p) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }

    // Set typical default terminal configuration.
    // By default, terminal has canonical mode, echoing, and post-processing.
    termios_p->c_iflag = ICRNL | IXON;
    termios_p->c_oflag = OPOST | ONLCR;
    termios_p->c_cflag = CS8 | CREAD | CLOCAL;
    termios_p->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | IEXTEN;
    
    // Default control characters
    termios_p->c_cc[VMIN] = 1;
    termios_p->c_cc[VTIME] = 0;

    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }
    (void)optional_actions;

    // BoredOS is key-buffered at the system level. Echoing is managed
    // by userland applications (e.g. standard reading programs do not automatically
    // print what is typed unless they want to, like bsh or lua).
    // So there is nothing we need to pass down to the kernel to disable echoing/canonical modes.
    // Simply return success.
    return 0;
}
