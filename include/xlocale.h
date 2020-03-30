/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2011, 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by David Chisnall under sponsorship from
 * the FreeBSD Foundation.
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
 *
 * $FreeBSD$
 */

#ifndef _XLOCALE_H_
#define _XLOCALE_H_

#define __va_list va_list

#include <locale.h>
__BEGIN_DECLS
#include <xlocale/_locale.h>

#ifdef __STRING_H__
#include <xlocale/_string.h>
#endif

#ifdef _MACHINE_INTTYPES_H_
#include <xlocale/_inttypes.h>
#endif

#ifdef _MONETARY_H_
#include <xlocale/_monetary.h>
#endif

#ifdef __STDLIB_H__
#include <xlocale/_stdlib.h>
#endif

#ifdef CHERIOS_TIME_H
#include <xlocale/_time.h>
#endif

#ifdef CHERIOS_LANGINFO_H
#include <xlocale/_langinfo.h>
#endif

#ifdef __CTYPE_H__
#include <xlocale/_ctype.h>
#endif

#ifdef CHERIOS_WCTYPE_H
#define _XLOCALE_WCTYPES 1
#include <xlocale/_ctype.h>
#endif

#ifdef __STDIO_H__
#include <xlocale/_stdio.h>
#endif

#ifdef CHERIOS_WCHAR_H
#include <xlocale/_wchar.h>
#endif


struct lconv	*localeconv_l(locale_t);
__END_DECLS

#endif