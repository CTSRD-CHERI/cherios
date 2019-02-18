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


#include <sockets.h>
#include "unistd.h"
#include "stdlib.h"
#include "namespace.h"
#include "stdio.h"
#include "sockets.h"
#include "assert.h"

act_kt fs_act;

act_kt try_get_fs(void) {
    if(!fs_act) {
        fs_act = namespace_get_ref(namespace_num_fs);
    }
    return fs_act;
}

#define DEFAULT_DRB_SIZE 0x800

void alloc_drb(FILE_t file) {
    char* buffer = (char*)malloc(DEFAULT_DRB_SIZE);
    init_data_buffer(&file->write_copy_buffer, buffer, DEFAULT_DRB_SIZE);
    file->write.push_writer->drb_fulfill_ptr = &file->write_copy_buffer.fulfill_ptr;
}

FRESULT mkdir(const char* name) {
    act_kt dest = try_get_fs();

    if(!dest) return -1;
    return (int)message_send(0,0,0,0,name,NULL,NULL,NULL,dest, SYNC_CALL, 1);

}


FRESULT rename(const char* old, const char* new) {
    act_kt dest = try_get_fs();

    if(!dest) return -1;
    return (int)message_send(0,0,0,0,old,new,NULL,NULL,dest, SYNC_CALL, 2);
}

FRESULT unlink(const char* name) {
    act_kt dest = try_get_fs();

    if(!dest) return -1;
    return (int)message_send(0,0,0,0,name,NULL,NULL,NULL,dest, SYNC_CALL, 3);
}

ssize_t truncate(FILE_t file) {
    ssize_t flush = socket_flush_drb(file);
    if(flush < 0) return flush;
    assert(file->con_type & CONNECT_PUSH_WRITE);
    ssize_t space = socket_internal_requester_space_wait(file->write.push_writer, 1, file->flags & MSG_DONT_WAIT, 0);
    if(space < 0) return space;
    return socket_internal_request_oob(file->write.push_writer, REQUEST_TRUNCATE, NULL, 0, 0);
}

struct socket_with_file_drb {
    unix_like_socket sock;
    char drb_data[DEFAULT_DRB_SIZE];
};

ERROR_T(FILE_t) open_er(const char* name, int mode, enum SOCKET_FLAGS flags) {

    int read = mode & FA_READ;
    int write = mode & FA_WRITE;

    if(!read && !write) return MAKE_ER(FILE_t,FR_INVALID_PARAMETER);

    act_kt dest = try_get_fs();

    if(!dest) return MAKE_ER(FILE_t,FR_NOT_READY);

    struct requester_32* r32_read = NULL, *r32_write = NULL;

    enum socket_connect_type  con_type = CONNECT_NONE;

    flags |= SOCKF_DRB_INLINE;
    struct socket_with_file_drb* sock_and_drb = (struct socket_with_file_drb*)(malloc(sizeof(struct socket_with_file_drb)));
    unix_like_socket* sock = &sock_and_drb->sock;

    if(read) {
        con_type |= CONNECT_PULL_READ;
        r32_read = (struct requester_32*)malloc(SIZE_OF_request(32));
        socket_internal_requester_init(&r32_read->r, 32, SOCK_TYPE_PULL, NULL);
    }
    if(write) {
        con_type |= CONNECT_PUSH_WRITE;
        r32_write = (struct requester_32*)malloc(SIZE_OF_request(32));
        socket_internal_requester_init(&r32_write->r, 32, SOCK_TYPE_PUSH, &sock->write_copy_buffer);
    }

    ssize_t result;

    if((result = message_send(mode,0,0,0,r32_read, r32_write, name, NULL, dest, SYNC_CALL, 0))) goto er1;
    if(r32_read) socket_internal_requester_connect(&r32_read->r);
    if(r32_write) socket_internal_requester_connect(&r32_write->r);





    flags |= MSG_NO_COPY;

    if(read & write) {
        flags |= MSG_EMULATE_SINGLE_PTR;
    }

    if((result = socket_init(sock, flags, sock_and_drb->drb_data, DEFAULT_DRB_SIZE, con_type))) goto er2;

    sock->write.push_writer = &r32_write->r;
    sock->read.pull_reader = &r32_read->r;
    sock->read_behind = sock->read_behind = 0;
    return MAKE_VALID(FILE_t,sock);

    er2:
    free(sock);
    er1:
    free(r32_read);
    free(r32_write);
    return MAKE_ER(FILE_t,result);
}

__thread FILE_t async_free_head;

asyn_close_state_e try_close(FILE_t file) {
    asyn_close_state_e new_state = file->close_state;

    ssize_t res;

    int dont_wait = file->flags & MSG_DONT_WAIT;

    switch (new_state) {
        case ASYNC_NEED_FLUSH_DRB:
            res = socket_flush_drb(file);
            if(res == E_SOCKET_CLOSED) res = 0;
            if(res < 0) break;

            new_state =  ASYNC_NEED_REQ_CLOSE;
        case ASYNC_NEED_REQ_CLOSE:
            if(file->con_type & CONNECT_PUSH_WRITE) {
                res = socket_internal_requester_space_wait(file->write.push_writer, 1, dont_wait, 0);
                if(res == E_SOCKET_CLOSED) res = 0;
                if(res < 0) break;
                socket_internal_request_oob(file->write.push_writer, REQUEST_CLOSE, NULL, 0, 0);
            }

            new_state = ASYNC_NEED_REQS_WRITE;
        case ASYNC_NEED_REQS_WRITE:
            if(file->con_type & CONNECT_PUSH_WRITE) {
                res = socket_internal_requester_space_wait(file->write.push_writer, file->write.push_writer->buffer_size, dont_wait, 0);
                if(res == E_SOCKET_CLOSED) res = 0;
                if(res < 0) break;
            }

            new_state = ASYNC_NEED_REQS_READ;
        case ASYNC_NEED_REQS_READ:

            if(file->con_type & CONNECT_PULL_READ) {
                res = socket_internal_requester_space_wait(file->read.pull_reader, file->read.pull_reader->buffer_size, dont_wait, 0);
                if(res == E_SOCKET_CLOSED) res = 0;
                if(res < 0) break;
            }

            new_state = ASYNC_FREE_RES;
        case ASYNC_FREE_RES:
            res = socket_close(file);
            if(res == E_SOCKET_CLOSED) res = 0;
            assert(res == 0);
            if(!(file->flags & SOCKF_RR_INLINE) && (file->con_type & CONNECT_PULL_READ)) free(file->read.pull_reader);
            if(!(file->flags & SOCKF_WR_INLINE) && (file->con_type & CONNECT_PUSH_WRITE)) free(file->write.push_writer);
            if(!(file->flags & SOCKF_DRB_INLINE) && (file->write_copy_buffer.buffer)) free(file->write_copy_buffer.buffer);
            if(!(file->flags & SOCKF_SOCK_INLINE)) free(file);

            new_state = ASYNC_DONE;
        case ASYNC_DONE:
            {}
    }

    if(new_state != ASYNC_DONE) file->close_state = new_state;
    return new_state;
}

static ssize_t delay_close(FILE_t file) {
    file->delay_close_next = async_free_head;
    async_free_head = file;
    return 0;
}

void process_async_closes(int force) {
    FILE_t* last_ptr = &async_free_head;
    FILE_t f = async_free_head;

    while(f) {
        FILE_t next = f->delay_close_next;
        if(force) f->flags &=~MSG_DONT_WAIT;
        if(try_close(f) != ASYNC_DONE) {
            assert(!force);
            *last_ptr = f;
            last_ptr = &f->delay_close_next;
        }
        f = next;
    }

    *last_ptr = NULL;
}



ssize_t close(FILE_t file) {
    process_async_closes(0); // Now is a good time to do this

    if(file->custom_close) return file->custom_close(file);

    asyn_close_state_e try = try_close(file);

    if(try != ASYNC_DONE) delay_close(file);

    return 0;
}

ssize_t lseek(FILE_t file, int64_t offset, int whence) {

    ssize_t flush = socket_flush_drb(file);
    if(flush < 0) return flush;

    unix_like_socket* sock = file;
    int em = file->flags & MSG_EMULATE_SINGLE_PTR;

    int dont_wait = sock->flags & MSG_DONT_WAIT;
    ssize_t ret;

    if(sock->con_type & CONNECT_PUSH_WRITE) {
        int64_t offset_w = offset;
        if(em && whence == SEEK_CUR) offset_w+=file->write_behind;
        if(whence != SEEK_CUR || offset_w) {
            ret = socket_internal_requester_lseek(sock->write.push_writer, offset_w, whence, dont_wait);
            if(ret < 0) return ret;
        }
        if(em) file->write_behind = 0;
    }
    if(sock->con_type & CONNECT_PULL_READ) {
        int64_t offset_r = offset;
        if(em && whence == SEEK_CUR) offset_r+=file->read_behind;
        if(whence != SEEK_CUR || offset_r) {
            ret = socket_internal_requester_lseek(sock->read.pull_reader, offset_r, whence, dont_wait);
            if(ret < 0) return ret;
        }
        if(em) file->read_behind = 0;
    }

    if(sock->con_type & (CONNECT_PUSH_READ | CONNECT_PULL_WRITE)) assert(0 && "TODO\n");

    return 0;
}

ssize_t soft_flush(FILE_t file) {
    // TODO send a flush on the socket.
    return 0;
}

ssize_t flush(FILE_t file) {
    if(file->con_type & CONNECT_PUSH_WRITE) {
        ssize_t flush = socket_flush_drb(file);
        assert(flush >= 0);
        socket_internal_requester_wait_all_finish(file->write.push_writer, 0);
    }
    if(file->con_type & CONNECT_PULL_READ) {
        socket_internal_requester_wait_all_finish(file->read.pull_reader, 0);
    }
    return 0;
    // TODO send a flush on the socket.
}

ssize_t filesize(FILE_t file) {
    _unsafe ssize_t fsize;
    uni_dir_socket_requester* req = file->con_type & CONNECT_PUSH_WRITE ?
                                    file->write.push_writer :
                                    file->read.pull_reader;

    ssize_t res = socket_internal_requester_space_wait(req,1,0,0);
    if(res < 0) return res;
    socket_internal_request_oob(req, REQUEST_SIZE, (intptr_t)&fsize, 0, 0);
    res = socket_internal_requester_wait_all_finish(req, 0);
    if(res < 0) return res;
    return fsize;
}

FRESULT stat(const char* path, FILINFO* fno) {
    act_kt dest = try_get_fs();

    if(dest == NULL) return FR_NO_FILESYSTEM;

    FRESULT res = (FRESULT)message_send(0,0,0,0,path,fno,NULL,NULL,dest,SYNC_CALL,4);

    return res;
}

typedef capability dir_token_t;

dir_token_t opendir(const char* name) {
    act_kt dest = try_get_fs();

    if(dest == NULL) return NULL;

    return message_send_c(0,0,0,0,name,NULL,NULL,NULL,dest,SYNC_CALL,5);
}

FRESULT readdir(dir_token_t dir, FILINFO* fno) {
    act_kt dest = try_get_fs();

    if(dest == NULL) return NULL;

    return (FRESULT)message_send(0,0,0,0,dir,fno,NULL,NULL,dest,SYNC_CALL,6);
}

FRESULT closedir(dir_token_t dir) {
    act_kt dest = try_get_fs();

    if(dest == NULL) return NULL;

    return (FRESULT)message_send(0,0,0,0,dir,NULL,NULL,NULL,dest,SYNC_CALL,7);
}

