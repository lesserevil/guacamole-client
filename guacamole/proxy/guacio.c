
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "guacio.h"

char __GUACIO_BAS64_CHARACTERS[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', 
};

GUACIO* guac_open(int fd) {

    GUACIO* io = malloc(sizeof(GUACIO));
    io->ready = 0;
    io->written = 0;
    io->fd = fd;

    return io;

}

void guac_close(GUACIO* io) {
    guac_flush(io);
    free(io);
}

ssize_t guac_write_int(GUACIO* io, unsigned int i) {

    char buffer[128];
    char* ptr = &(buffer[127]);

    *ptr = 0;

    do {

        ptr--;
        *ptr = '0' + (i % 10);

        i /= 10;

    } while (i > 0 && ptr >= buffer);

    return guac_write_string(io, ptr);

}

ssize_t guac_write_string(GUACIO* io, const char* str) {

    int fd = io->fd;
    char* out_buf = io->out_buf;

    int retval;

    for (; *str != '\0'; str++) {

        out_buf[io->written++] = *str; 

        /* Flush when necessary, return on error */
        if (io->written > 8188 /* sizeof(out_buf) - 4 */) {
            retval = write(fd, out_buf, io->written);
            if (retval < 0)
                return retval;

            io->written = 0;
        }

    }

    return 0;

}

ssize_t __guac_write_base64_triplet(GUACIO* io, int a, int b, int c) {

    int fd = io->fd;
    char* out_buf = io->out_buf;

    int retval;

    /* Byte 1 */
    out_buf[io->written++] = __GUACIO_BAS64_CHARACTERS[(a & 0xFC) >> 2]; /* [AAAAAA]AABBBB BBBBCC CCCCCC */

    if (b >= 0) {
        out_buf[io->written++] = __GUACIO_BAS64_CHARACTERS[((a & 0x03) << 4) | ((b & 0xF0) >> 4)]; /* AAAAAA[AABBBB]BBBBCC CCCCCC */

        if (c >= 0) {
            out_buf[io->written++] = __GUACIO_BAS64_CHARACTERS[((b & 0x0F) << 2) | ((c & 0xC0) >> 6)]; /* AAAAAA AABBBB[BBBBCC]CCCCCC */
            out_buf[io->written++] = __GUACIO_BAS64_CHARACTERS[c & 0x3F]; /* AAAAAA AABBBB BBBBCC[CCCCCC] */
        }
        else { 
            out_buf[io->written++] = __GUACIO_BAS64_CHARACTERS[((b & 0x0F) << 2)]; /* AAAAAA AABBBB[BBBB--]------ */
            out_buf[io->written++] = '='; /* AAAAAA AABBBB BBBB--[------] */
        }
    }
    else {
        out_buf[io->written++] = __GUACIO_BAS64_CHARACTERS[((a & 0x03) << 4)]; /* AAAAAA[AA----]------ ------ */
        out_buf[io->written++] = '='; /* AAAAAA AA----[------]------ */
        out_buf[io->written++] = '='; /* AAAAAA AA---- ------[------] */
    }

    /* At this point, 4 bytes have been io->written */

    /* Flush when necessary, return on error */
    if (io->written > 8188 /* sizeof(out_buf) - 4 */) {
        retval = write(fd, out_buf, io->written);
        if (retval < 0)
            return retval;

        io->written = 0;
    }

    if (b < 0)
        return 1;

    if (c < 0)
        return 2;

    return 3;

}

ssize_t __guac_write_base64_byte(GUACIO* io, char buf) {

    int* ready_buf = io->ready_buf;

    int retval;

    ready_buf[io->ready++] = buf & 0xFF;

    /* Flush triplet */
    if (io->ready == 3) {
        retval = __guac_write_base64_triplet(io, ready_buf[0], ready_buf[1], ready_buf[2]);
        if (retval < 0)
            return retval;

        io->ready = 0;
    }

    return 1;
}

ssize_t guac_write_base64(GUACIO* io, const void* buf, size_t count) {

    int retval;

    const char* char_buf = (const char*) buf;
    const char* end = char_buf + count;

    while (char_buf < end) {

        retval = __guac_write_base64_byte(io, *(char_buf++));
        if (retval < 0)
            return retval;

    }

    return count;

}

ssize_t guac_flush(GUACIO* io) {

    int retval;

    /* Flush remaining bytes in buffer */
    if (io->written > 0) {
        retval = write(io->fd, io->out_buf, io->written);
        if (retval < 0)
            return retval;

        io->written = 0;
    }

    return 0;

}

ssize_t guac_flush_base64(GUACIO* io) {

    int retval;

    /* Flush triplet to output buffer */
    while (io->ready > 0) {
        retval = __guac_write_base64_byte(io, -1);
        if (retval < 0)
            return retval;
    }

    return 0;

}

