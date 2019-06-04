/*-
 * Copyright (c) 2019 Lawrence Esswood
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
#ifndef CHERIOS_ANSI_ESCAPES_H
#define CHERIOS_ANSI_ESCAPES_H

#define ANSI_ESC    "\x1B"
#define ANSI_CURSOR_SAVE ANSI_ESC "7"
#define ANSI_CURSOR_RESTORE ANSI_ESC "8"

#define ANSI_ESC_C "\x1B["
#define ANSI_CURSOR_PREV "F"
#define ANSI_CURSOR_SET_WINDOW "r"
#define ANSI_CURSOR_HOME "H"
#define ANSI_SET_CURSOR  "H"
#define ANSI_CURSOR_FORWARD "C"
#define ANSI_ERASE "J"
#define ANSI_CLEAR_UP "1J"
#define ANSI_CLEAR_ALL "2J"

#define CTRL_START ANSI_CURSOR_SAVE ANSI_ESC_C ANSI_CURSOR_HOME
#define CTRL_END ANSI_CURSOR_RESTORE

#define ANSI_BACK_BLACK "40m"
#define ANSI_BACK_RED "41m"
#define ANSI_BACK_GREEN "42m"
#define ANSI_BACK_YELLOW "43m"
#define ANSI_BACK_BLUE "44m"
#define ANSI_BACK_MAGENTA "45m"
#define ANSI_BACK_CYAN "46m"
#define ANSI_BACK_WHITE "47m"

#endif //CHERIOS_ANSI_ESCAPES_H
