# TODO RISCV

.macro NANO_FUNC name
    .global \name
    \name:
.endmacro

.text

NANO_FUNC nano_kernel_init

nop

NANO_FUNC create_context

NANO_FUNC destroy_context

NANO_FUNC context_switch

NANO_FUNC critical_section_enter

NANO_FUNC critical_section_exit

NANO_FUNC set_exception_handler

NANO_FUNC rescap_take

NANO_FUNC rescap_nfo

NANO_FUNC rescap_revoke_start

NANO_FUNC rescap_revoke_finish

NANO_FUNC rescap_split

NANO_FUNC rescap_merge

NANO_FUNC rescap_parent

NANO_FUNC rescap_splitsub

NANO_FUNC rescap_getsub

NANO_FUNC get_phy_page

NANO_FUNC create_table

NANO_FUNC create_mapping

NANO_FUNC free_mapping

NANO_FUNC get_top_level_table

NANO_FUNC get_sub_table

NANO_FUNC get_read_only_table

NANO_FUNC make_first_reservation

NANO_FUNC get_book

NANO_FUNC split_phy_page_range

NANO_FUNC merge_phy_page_range

NANO_FUNC zero_page_range

NANO_FUNC obtain_super_powers

NANO_FUNC get_last_exception

NANO_FUNC foundation_create

NANO_FUNC foundation_enter

NANO_FUNC foundation_entry_expose

NANO_FUNC foundation_entry_get_id

NANO_FUNC make_key_for_auth

NANO_FUNC foundation_entry_vaddr

NANO_FUNC foundation_new_entry

NANO_FUNC rescap_take_authed

NANO_FUNC rescap_check_cert

NANO_FUNC rescap_check_single_cert

NANO_FUNC rescap_take_locked

NANO_FUNC rescap_unlock

NANO_FUNC foundation_get_id

NANO_FUNC tres_get

NANO_FUNC tres_take

NANO_FUNC tres_revoke

NANO_FUNC tres_get_ro_bitfield

NANO_FUNC exception_subscribe

NANO_FUNC exception_return

NANO_FUNC exception_replay

NANO_FUNC exception_signal

NANO_FUNC exception_getcause

NANO_FUNC get_integer_space_cap

NANO_FUNC modify_hardware_reg

NANO_FUNC interrupts_mask

NANO_FUNC interrupts_soft_set

NANO_FUNC interrupts_get

NANO_FUNC translate_address

NANO_FUNC if_req_and_mask

NANO_FUNC nano_dummy
