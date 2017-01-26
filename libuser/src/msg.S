#-
# Copyright (c) 2016 Hadrien Barral
# Copyright (c) 2017 Lawrence Esswood
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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

.set noreorder
.set nobopt
.set noat

.ifeq _MIPS_SZCAP-256
.set CAP_SIZE_BITS, 5
.else
.ifeq _MIPS_SZCAP-128
.set CAP_SIZE_BITS, 4
.else
.error "Unknown cap size"
.endif
.endif

# Dispatch CCalls

	.text
	.global msg_entry
	.ent msg_entry
msg_entry:
	cgetdefault	$c17
	bltz		$v0, control
	nop

	dla		$t0, msg_methods
	dla		$t1, msg_methods_nb
	b		call
	nop

control:
	dla		$t0, ctrl_methods
	dla		$t1, ctrl_methods_nb
	sub		$v0, $0, $v0
call:
	cld		$t1, $t1, 0($c17)
	sub		$t1, $t1, $v0
	blez		$t1, overflow
	nop

	#Save sync_token
	dla		$t1, sync_token
	csc		$c1, $t1, 0($c17)
    #Save caller
    dla     $t1, sync_caller
    csc     $c2, $t1, 0($c17)

	sll		$v0, $v0, CAP_SIZE_BITS
	add		$t0, $t0, $v0
	clc		$c12, $t0, 0($c17)
	cjalr		$c12, $c17
	nop
	b		end
	nop

overflow:
	move		$v0, $0
	move		$v1, $0
	cfromptr	$c3, $c3, $0
	b		end
	nop

end:
	#Restore sync_token
	cgetdefault	$c17
	dla		$t1, sync_token
	clc		$c1, $t1, 0($c17)
	#Restore caller activation reference
	dla     $t1, sync_caller
	clc     $c2, $t1, 0($c17)
	creturn

	#We are scheduled again: new message
	b msg_entry
	nop
msg_entry_end:
	.end msg_entry
	.size msg_entry, msg_entry_end - msg_entry
