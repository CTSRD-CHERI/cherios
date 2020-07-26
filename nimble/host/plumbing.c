/*-
 * Copyright (c) 2020 Hadrien Barral
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    uint8_t buffer[70000];
    ssize_t buffer_fill;
    size_t frame_length;
    uint8_t custom_header[4];
    size_t custom_header_fill;
} tmp_buf_to_hci_t;

static int create_hci_socket(void)
{
    int hci_socket = socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (hci_socket == -1) {
        fprintf(stderr, "%s: @socket: ", __func__);
        perror("");
        return -1;
    }

    struct sockaddr_hci addr = {
        .hci_family = AF_BLUETOOTH,
        //.hci_dev = 0, /* TODOX: 0 for hci0, parse this from cmdline */
        .hci_dev = 1, /* TODOX: 1 for hci1, parse this from cmdline */
        .hci_channel = HCI_CHANNEL_USER,
    };



    int ret = bind(hci_socket, (struct sockaddr *) &addr, sizeof(addr));
    if (ret != 0) {
        fprintf(stderr, "%s: @bind: ", __func__);
        perror("");
        return -1;
    }

    return hci_socket;
}

static int create_tcp_socket(uint16_t listen_port)
{
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket == -1) {
        fprintf(stderr, "%s: @socket: ", __func__);
        perror("");
        return -1;
    }

    int optval = 1;
    setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(listen_port),
    };
    int ret = bind(tcp_socket, (struct sockaddr *) &addr, sizeof(addr));
    if (ret != 0) {
        fprintf(stderr, "%s: @bind: ", __func__);
        perror("");
        return -1;
    }

    int backlog = 3; /* Hard-coded without specific thinking */
    ret = listen(tcp_socket, backlog);
    if (ret != 0) {
        fprintf(stderr, "%s: @listen: ", __func__);
        perror("");
        return -1;
    }

    return tcp_socket;
}

static int transfer_data_to_tcp(int from_socket, int to_socket)
{
    unsigned char buffer[0x1000];
    ssize_t len = read(from_socket, buffer, sizeof(buffer));
    if (len < 0) {
        fprintf(stderr, "%s: @read: ", __func__);
        perror("");
        return -1;
    }
    if (len == 0) {
        fprintf(stderr, "%s: @read: EOF\n", __func__);
        return -1;
    }

    printf("HCI->TCP\n");
    printf("  ");
    for (ssize_t i=0; i<len; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("\n");

    ssize_t written_len = write(to_socket, buffer, len);
    if (written_len != len) {
        fprintf(stderr, "%s: @write: ", __func__);
        fprintf(stderr, "socket:%d buffer:%p len:%ld errno:%d ", to_socket, buffer, len, errno);
        perror("");
        return -1;
    }
    return 0;
}

static int transfer_data_to_hci(int from_socket, int to_socket, tmp_buf_to_hci_t *tmp_buf_to_hci)
{
    if (tmp_buf_to_hci->custom_header_fill < 4) {
        ssize_t len = read(from_socket, tmp_buf_to_hci->custom_header,
                           sizeof(tmp_buf_to_hci->custom_header) - tmp_buf_to_hci->custom_header_fill);
        if (len < 0) {
            fprintf(stderr, "%s: @read: ", __func__);
            perror("");
            return -1;
        }
        if (len == 0) {
            fprintf(stderr, "%s: @read: EOF\n", __func__);
            return -1;
        }
        tmp_buf_to_hci->custom_header_fill += len;
        return 0; /* More next time! */
    }

    size_t frame_length =
          ((size_t) tmp_buf_to_hci->custom_header[0]) <<  0;
        + ((size_t) tmp_buf_to_hci->custom_header[1]) <<  8;
        + ((size_t) tmp_buf_to_hci->custom_header[2]) << 16;
        + ((size_t) tmp_buf_to_hci->custom_header[3]) << 24;

    if (frame_length > sizeof(tmp_buf_to_hci->buffer)) {
        fprintf(stderr, "%s: frame too big %zu/%zu\n", __func__, frame_length, sizeof(tmp_buf_to_hci->buffer));
        return -1;
    }

    ssize_t len = read(from_socket, tmp_buf_to_hci->buffer,
                       frame_length-tmp_buf_to_hci->buffer_fill);
    if (len < 0) {
        fprintf(stderr, "%s: @read: ", __func__);
        perror("");
        return -1;
    }
    if (len == 0) {
        fprintf(stderr, "%s: @read: EOF\n", __func__);
        return -1;
    }
    tmp_buf_to_hci->buffer_fill += len;

    if (tmp_buf_to_hci->buffer_fill < frame_length) {
        return 0; /* More next time */
    }

    printf("TCP->HCI\n");
    printf("  ");
    for (ssize_t i=0; i<tmp_buf_to_hci->buffer_fill; i++) {
        printf("%02x ", tmp_buf_to_hci->buffer[i]);
    }
    printf("\n");

    ssize_t written_len = write(to_socket, tmp_buf_to_hci->buffer, tmp_buf_to_hci->buffer_fill);
    if (written_len != tmp_buf_to_hci->buffer_fill) {
        fprintf(stderr, "%s: @write: ", __func__);
        fprintf(stderr, "socket:%d len:%ld errno:%d ", to_socket, tmp_buf_to_hci->buffer_fill, errno);
        perror("");
        return -1;
    }

    /* Reset `tmp_buf_to_hci` */
    tmp_buf_to_hci->custom_header_fill = 0;
    tmp_buf_to_hci->buffer_fill = 0;

    return 0;
}

static void plumbing_hci_tcp(int hci_socket, int tcp_socket)
{
    tmp_buf_to_hci_t tmp_buf_to_hci = {0};

    for(;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(hci_socket, &readfds);
        FD_SET(tcp_socket, &readfds);

        int ndfs = ((hci_socket > tcp_socket) ? hci_socket : tcp_socket) + 1;

        struct timeval tv = {
            .tv_sec = 10,
            .tv_usec = 0,
        };

        int ret = select(ndfs, &readfds, NULL, NULL, &tv);
        if (ret == 0) {
            continue;
        }
        if (ret == -1) {
            fprintf(stderr, "%s: @select: ", __func__);
            perror("");
            break;
        }


        if (FD_ISSET(hci_socket, &readfds)) {
            ret = transfer_data_to_tcp(hci_socket, tcp_socket);
            if (ret != 0) {
                break;
            }
        }
        if (FD_ISSET(tcp_socket, &readfds)) {
            ret = transfer_data_to_hci(tcp_socket, hci_socket, &tmp_buf_to_hci);
            if (ret != 0) {
                break;
            }
        }
    }
}

int main(void)
{
    uint16_t tcp_listen_port = 6666;


    int hci_socket = create_hci_socket();
    if (hci_socket == -1) {
        fprintf(stderr, "Could not create HCI socket\n");
        exit(1);
    }

    int tcp_server_socket = create_tcp_socket(tcp_listen_port);
    if (tcp_server_socket == -1) {
        fprintf(stderr, "Could not create TCP socket\n");
        exit(1);
    }

    while(1) {
        struct sockaddr_in client_addr = {0};
        socklen_t client_addr_len = sizeof(client_addr);
        int conn_fd = accept(tcp_server_socket, (struct sockaddr *) &client_addr, &client_addr_len);
        if (conn_fd == -1) {
            fprintf(stderr, "%s: @accept: ", __func__);
            perror("");
            continue;
        }

        printf("TCP connection established\n");
        plumbing_hci_tcp(hci_socket, conn_fd);

        close(conn_fd);
    }
}
