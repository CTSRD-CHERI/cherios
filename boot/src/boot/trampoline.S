#
# Relocatable exception vector; jumps to kernel initialisation
#
		.global kernel_trampoline
		.ent kernel_trampoline
kernel_trampoline:
		dla	$k0, __kernel_entry_point
		jr	$k0
		nop
kernel_trampoline_end:
		nop
		.global kernel_trampoline_end
		.end kernel_trampoline
		.size kernel_trampoline, kernel_trampoline_end - kernel_trampoline
		.size kernel_trampoline_end, 1 # 0 triggers capsizefix warning
