# -
# Copyright (c) 2019 Lawrence Esswood
# All rights reserved.
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
# ("CTSRD"), as part of the DARPA CRASH research programme.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#define __ASSEMBLY__ 1
.set MIPS_SZCAP, _MIPS_SZCAP
#include "mips.h"
#include "dylink.h"
#include "asm.S"
.text

# This stub is used to call function pointers (paired with a data argument). We short circuit when its in the same doamin #
.ent call_function_pointer_arg_mem
.global call_function_pointer_arg_mem
call_function_pointer_arg_mem:
.type invoke_c1, "tls_object"
.weak invoke_c1
.type invoke_c2, "tls_object"
.weak invoke_c2
load_func plt_common_untrusting
get_tls_sym $c1, invoke_c1
get_tls_sym $c2, invoke_c2
call_function_pointer:
# Offset 6 instructions if a sealed untrusted reference. 4 if a trusted unsealed reference

# TODO we can probably avoid some branching use conditional sealing
# put code in c1, data in c2, mode in c12
ceq     $t0, $c2, $idc       # If the target data is already idc then we are making a local call
beqz    $t0, go_via_mode
cgetsealed $t0, $c1
beqz    $t0, offset_trusted
clcbi   $c2, CTLP_OFFSET_CDS($idc)
cunseal $c1, $c1, $c2
cincoffset  $c1, $c1, (2 * 4)
offset_trusted:
cincoffset  $c12, $c1, (4 * 4)
go_via_mode:
cjr     $c12
nop
.end call_function_pointer_arg_mem
