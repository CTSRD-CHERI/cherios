/*-
 * Copyright (c) 2018 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include "sockets.h"
#include <ff.h>
#include "spinlock.h"
#include "ff.h"
#include "misc.h"
#include "virtioblk.h"
#include "stdio.h"
#include "thread.h"
#include "stdlib.h"
#include "atomic.h"
#include "cheristd.h"

FATFS fs;

#define MAX_HANDLES 0x80

struct sessions_t {
    unix_like_socket sock;
    FIL fil;
    locked_t encrypt_lock;
    size_t next_ndx; // either next free, or next in list of open files
    uint64_t read_fptr;
    uint64_t write_fptr;
    enum poll_events events;
    uint8_t current;
    uint8_t more;
    uint8_t in_use;
    uint8_t nice_close;
} sessions[MAX_HANDLES];

size_t first_free = 0;
size_t first_file = MAX_HANDLES;
size_t n_files = 0;

fs_proxy sr_read;
fs_proxy sr_write;

static void set_encrypt_lock(fs_proxy* proxy, locked_t locked) {
    if(proxy->encrypt_lock != locked) {
        int res = socket_requester_space_wait(proxy->requester, 1, 0, 0);
        assert_int_ex(res, ==, 0);
        res = socket_request_oob(proxy->requester, (request_type_e)REQUEST_SET_KEY, (intptr_t)locked, 0, 0);
        assert_int_ex(res, ==, 0);
        proxy->encrypt_lock = locked;
    }
}


void close_file_internal(size_t* prev_ndx, struct sessions_t* session, uint8_t level) {
    assert_int_ex(session->in_use, ==, 1);

    if(session->nice_close < level) {
        if(level > 4) level = 4;
        switch(session->nice_close) {
            case 0:
                // Close the underlying file
                f_close(&session->fil);
                if(level == 1) break;
            case 1:
                // Close the socket
                socket_close(&session->sock);
                if(level == 2) break;
            case 2:
                // Used to have a poll socks structure. Now no more!
                n_files--;
                if(level == 3) break;
            case 3:
                // Then free struct
                session->in_use = 0;
                size_t this_ndx = *prev_ndx;
                *prev_ndx = session->next_ndx;
                session->next_ndx = first_free;
                first_free = this_ndx;

        }
        session->nice_close = level;
    }
}

ssize_t CROSS_DOMAIN_DEFAULT_INSECURE(full_oob)(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length);
__used ssize_t full_oob(capability arg, request_t* request,  uint64_t offset, __unused uint64_t partial_bytes, uint64_t length) {
    struct sessions_t* fil = (struct sessions_t*)arg;

    if(offset != 0) {
        fil->more = 1;
        return E_AGAIN; // Ignore oobs mid stream, we need to proxy first
    }

    assert(!fil->nice_close);
    request_type_e req = request->type;
    uint64_t* modify;
    ssize_t * size_ptr;
    switch(req) {
        case REQUEST_SEEK:
            assert(!fil->encrypt_lock); // currently don't support seek here
            modify = fil->current == 0 ? &fil->read_fptr : &fil->write_fptr;

            int64_t seek_offset = request->request.seek_desc.v.offset;
            int whence = request->request.seek_desc.v.whence;

            FSIZE_t target_offset;

            switch (whence) {
                case SEEK_CUR:
                    target_offset = seek_offset + *modify;
                    break;
                case SEEK_SET:
                    if(seek_offset < 0) return E_OOB;
                    target_offset = (uint64_t)seek_offset;
                    break;
                case SEEK_END:
                    target_offset = (seek_offset + fil->fil.fptr);
                    break;
                default:
                    return E_OOB;
            }

            *modify = target_offset;

            return length;
        case REQUEST_SIZE:
            size_ptr = (ssize_t*)request->request.oob;
            *size_ptr = f_size(&fil->fil);

            return length;
        case REQUEST_TRUNCATE:
            f_truncate(&fil->fil);
            return length;
        case REQUEST_CLOSE:
            close_file_internal(NULL, fil, 1);
            return length;
        case REQUEST_FLUSH:
            assert(0 && "TODO");
        default:
            assert(0 && "UNKNOWN OOB");
            return E_OOB;
    }

}

typedef FRESULT f_rw_type_f (
        FIL* fp,			/* Pointer to the file object */
fulfiller_t fulfill,
        UINT btw,			/* Number of bytes to write */
UINT* bw			/* Pointer to number of bytes written */
);

int proxy_file_to_cache(struct sessions_t* session, uint8_t service_write) {

    FRESULT fresult = 0;
    ssize_t res;
    UINT bytes_handled = 0;

    fulfiller_t fulfiller;
    f_rw_type_f* rw_f;
    fs_proxy* proxy;
    uint64_t* fptr;

    if(service_write) {
        fulfiller = session->sock.read.push_reader;
        rw_f = &f_write;
        proxy = &sr_write;
        fptr = &session->write_fptr;
    } else {
        fulfiller = session->sock.write.pull_writer;
        rw_f = &f_read;
        proxy = &sr_read;
        fptr = &session->read_fptr;
    }

    session->current = service_write;

    // This will handle leading oobs and oobs we already handled once
    res = socket_fulfill_progress_bytes_unauthorised(fulfiller, SOCK_INF,
                                                     F_CHECK | F_PROGRESS | F_DONT_WAIT | F_CANCEL_NON_OOB | F_SET_MARK | F_SKIP_ALL_UNTIL_MARK,
                                                     NULL,
                                                     (capability)session, 0, CROSS_DOMAIN_DEFAULT_INSECURE_SEALED(full_oob),
                                                     NULL, NULL, DATA_DEFAULT_INSECURE);

    assert_int_ex(-res, ==, -E_AGAIN);

    int any_proxy = 0;

    do {
        session->more = 0;

        // This will count bytes and handle oobs. We set the mark to know how much to skip later
        res = socket_fulfill_progress_bytes_unauthorised(fulfiller, SOCK_INF,
                                                         F_CHECK | F_SET_MARK | F_START_FROM_LAST_MARK | F_DONT_WAIT,
                                                         NULL,
                                                         (capability)session, 0, CROSS_DOMAIN_DEFAULT_INSECURE_SEALED(full_oob),
                                                         NULL, NULL, DATA_DEFAULT_INSECURE);

        assert(res >= 0 || res == E_AGAIN || res == E_SOCKET_CLOSED);

        if(res > 0) {
            assert(session->fil.obj.fs);
            if(session->fil.fptr != *fptr) {
                f_lseek(&session->fil, *fptr);
            }
            set_encrypt_lock(proxy, session->encrypt_lock);
            fresult = rw_f(&session->fil, fulfiller, (UINT)res, &bytes_handled);
            // FIXME: we are ignoring fresult here!
            (void)fresult;
            assert(bytes_handled == res);
            *fptr +=res;
            any_proxy = 1;
        }

    } while(res > 0 && session->more);

    return any_proxy;
}

void handle(enum poll_events events, struct sessions_t* session) {

    assert_int_ex(session->in_use, ==, 1);

    assert(session->nice_close < 2);

    int any_proxy = 0;
    __unused int was_partial_closed = session->nice_close;

    if(events & POLL_IN) {
        // service write (we read)

        any_proxy |= proxy_file_to_cache(session, 1);

    }
    if(events & POLL_OUT) {
        // service read (we write)

        assert(session->nice_close == 0);

        any_proxy |= proxy_file_to_cache(session, 0);
    }

    assert(!(any_proxy && was_partial_closed));

    if(session->nice_close && !any_proxy) {
        session->events = 0;
        close_file_internal(NULL, session, 2);
    }
}

static void init_block_cache_requesters(void) {
    ssize_t ret;

    requester_t read = socket_malloc_requester_32(SOCK_TYPE_PULL, NULL);
    requester_t write = socket_malloc_requester_32(SOCK_TYPE_PUSH, NULL);

    assert(read != NULL && write != NULL);

    ret = virtio_new_socket(read, CONNECT_PULL_READ);
    assert_int_ex(ret, ==, 0);
    ret = virtio_new_socket(write, CONNECT_PUSH_WRITE);
    assert_int_ex(ret, ==, 0);

    sr_read.offset = sr_write.offset = sr_read.length = sr_write.length = 0;

    sr_read.requester = read;
    sr_write.requester = write;
}


int open_file_internal(int mode, requester_t read_requester, requester_t write_requester, const char* file_name, locked_t* encrpyt) {

    int read = mode & FA_READ;
    int write = mode & FA_WRITE;

    if(!read && !write) return FR_INVALID_PARAMETER;

    if(first_free == MAX_HANDLES) {
        return FR_TOO_MANY_OPEN_FILES;
    }

    struct sessions_t* session = &sessions[first_free];

    assert_int_ex(session->in_use, ==, 0);

    FIL * fp = &session->fil;

    enum socket_connect_type con_type = CONNECT_NONE;
    enum poll_events events = POLL_NONE;

    if(write) {
        assert(write_requester);
        con_type |= CONNECT_PUSH_READ;
        if(session->sock.read.push_reader) {
            __unused int ret = socket_reuse_fulfiller(session->sock.read.push_reader, SOCK_TYPE_PUSH);
            assert_int_ex(ret, ==, 0);
        } else {
            session->sock.read.push_reader = socket_malloc_fulfiller(SOCK_TYPE_PUSH);
        }
        socket_fulfiller_connect(session->sock.read.push_reader, write_requester);
        events |= POLL_IN;
    }
    if(read) {
        assert(read_requester);
        con_type |= CONNECT_PULL_WRITE;
        mode |= FA_READ;
        if(session->sock.write.pull_writer) {
            __unused int ret = socket_reuse_fulfiller(session->sock.write.pull_writer, SOCK_TYPE_PULL);
            assert_int_ex(ret, ==, 0);
        } else {
            session->sock.write.pull_writer = socket_malloc_fulfiller(SOCK_TYPE_PULL);
        }
        socket_fulfiller_connect(session->sock.write.pull_writer, read_requester);
        events |= POLL_OUT;
    }

    FRESULT fres = FR_INVALID_PARAMETER;
    if(socket_init(&session->sock, MSG_DONT_WAIT | MSG_NO_CAPS, NULL, 0, con_type) == 0) {
        if((fres = f_open(fp, file_name, (BYTE)mode)) == 0) {
            session->read_fptr = session->write_fptr = 0;
            session->encrypt_lock = encrpyt;
            session->in_use = 1;
            session->nice_close = 0;
            size_t next = session->next_ndx;
            session->events = events;
            session->next_ndx = first_file;
            first_file = first_free;
            n_files++;
            first_free = next;
            return 0;
        }
    }

    return fres;
}

int make_dir(const char* name) {
    return f_mkdir(name);
}

capability dir_sealer;

static capability open_dir(const char* name) {
    DIR* dp = (DIR*)malloc(sizeof(DIR));

    FRESULT res = f_opendir(dp, name);

    if(res != FR_OK) {
        free(dp);
        return NULL;
    }

    return cheri_seal(dp, dir_sealer);
}

static FRESULT read_dir(capability sealed_dir, FILINFO* fno) {
    DIR* dp = cheri_unseal(sealed_dir, dir_sealer);
    return f_readdir(dp, fno);
}

static FRESULT close_dir(capability sealed_dir) {
    DIR* dp = cheri_unseal(sealed_dir, dir_sealer);
    FRESULT res = f_closedir(dp);
    free(dp);
    return res;
}

void (*msg_methods[]) = {open_file_internal, make_dir, f_rename, f_unlink, f_stat, open_dir, read_dir, close_dir};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);

void request_loop(void) {

    for(size_t i = 0; i != MAX_HANDLES; i++) {
        sessions[i].next_ndx = i+1;
    }

    POLL_LOOP_START(sock_sleep, sock_event, 1);

        restart_loop: {}

        size_t * prev_ptr = &first_file;
        for(size_t i = first_file; i!= MAX_HANDLES; i = sessions[i].next_ndx) {
            struct sessions_t* session = &sessions[i];
            assert_int_ex(session->in_use, ==, 1);

            if(session->nice_close == 2) {
                close_file_internal(prev_ptr, session, UINT8_MAX);
                goto restart_loop;
            }

            if(session->events != POLL_NONE) {

                enum poll_events event = 0;

                // Don't go from checkpoint, we want to wake up to mark oobs as complete when proxying is finished
                if(session->events & POLL_IN) {
                    POLL_ITEM_F(event_rd, sock_sleep, sock_event, session->sock.read.push_reader, POLL_IN, 0);
                    event |= event_rd;
                }
                if(session->events & POLL_OUT) {
                    POLL_ITEM_F(event_wr, sock_sleep, sock_event, session->sock.write.pull_writer, POLL_OUT, 0);
                    event |= event_wr;
                }

                if(event & (POLL_IN | POLL_OUT)) {
                        handle(event, session);
                } else if(event){
                    if(event & POLL_HUP) {
                        close_file_internal(prev_ptr, session, UINT8_MAX);
                        goto restart_loop;
                    }
                    if(event & (POLL_ER | POLL_NVAL)) {
                        assert(0 && "TODO");
                    }
                }
            }

            prev_ptr = &session->next_ndx;
        }

    POLL_LOOP_END(sock_sleep, sock_event, 1, 0);
}

int main(capability fs_cap) {
    printf("Fatfs: Hello world\n");

    // Still have to do this manually when we send too much
    msg_allow_more_sends();

    /* Init virtio-blk session */
    virtio_blk_session(fs_cap);

    FRESULT res;

    int already_existed;

    if ((res = f_mount(&fs, "", 1)) != FR_NO_FILESYSTEM && res != 0) {
        printf("MT:%d\n", res);
        goto er;
    }

    if (res == FR_NO_FILESYSTEM) {
        already_existed = 0;
        printf("Fatfs: Making FS. This may take some time. Hope you don't need that disk...\n");
        if ((res = f_mkfs("", 0, 0))) {
            printf("MK:%d\n", res);
            goto er;
        }
        virtio_writeback_all(); // Persists the empty FS.
    } else {
        already_existed = 1;
    }

    if(!already_existed && (res = f_mount(&fs, "", 1))) {
        printf("MT(2):%d\n", res);
        goto er;
    }

    init_block_cache_requesters();

    namespace_register(namespace_num_fs, act_self_ref);

    dir_sealer = get_type_owned_by_process();

    assert(dir_sealer != NULL);

    printf("Fatfs: Going into daemon mode\n");

    request_loop();

    er:
    printf("Fatfs: Failed to start");

    return -1;
}
