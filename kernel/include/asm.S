#-
# Copyright (c) 2016 Hadrien Barral
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

.set CAP_SIZE, (MIPS_SZCAP/8)
.set REG_SIZE, (64/8)
.set MIPS_FRAME_SIZE, (32*REG_SIZE)

.set noreorder
.set nobopt
.set noat

#
# A few useful macros
#

# Saves capability register 'creg' to offset 'offset' of frame 'frame'
.macro csave creg, offset, frame
	csc	\creg, $0, \offset*CAP_SIZE+MIPS_FRAME_SIZE(\frame)
.endm

# Restores capability register 'creg' to offset 'offset' of frame 'frame'
.macro crestore creg, offset, frame
	clc	\creg, $0, \offset*CAP_SIZE+MIPS_FRAME_SIZE(\frame)
.endm

# Saves general-purpose register 'greg' to offset 'offset' of frame 'frame'
.macro gsave greg, offset, frame
	csd	\greg, $0, \offset*REG_SIZE(\frame)
.endm

# Restores general-purpose register 'greg' to offset 'offset' of frame 'frame'
.macro grestore greg, offset, frame
	cld	\greg, $0, \offset*REG_SIZE(\frame)
.endm

# Calls 'macro elem, index, global' for each 'elem' of the input list
.macro foreachi macro, index, global, head, tail:vararg
	\macro \head, \index, \global
	.ifnb  \tail
	foreachi \macro, "(\index+1)", \global, \tail
	.endif
.endm
