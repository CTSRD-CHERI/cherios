add_subdirectory(activation_events)
add_subdirectory(idle)
add_subdirectory(type_manager)
add_subdirectory(dylink)

if(NOT ${BAREBONES})
add_subdirectory(fatfs)
#add_subdirectory(dummyfs)
add_subdirectory(dedup)
add_subdirectory(lwip)
add_subdirectory(block_cache)
endif()

add_subdirectory(libsocket)