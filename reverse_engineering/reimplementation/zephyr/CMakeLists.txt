# G6 HE clean-room target project (Zephyr / nRF Connect SDK).
#
# This project is TARGET-READY but FAIL-CLOSED. It builds the portable modules
# and the mock backends. Enabling physical hardware (-DG6HE_ENABLE_HARDWARE=ON)
# is intended to fail until the board pin table and the safety-critical
# electrical constants are evidenced, because include/g6he_board.h emits
# #error in that case.
#
# It never signs an image and provides no flash command. Any produced binary is
# an engineering artifact, not a release.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(g6he_target LANGUAGES C)

option(G6HE_ENABLE_HARDWARE
       "Enable physical peripherals (requires an evidenced pin/electrical table)"
       OFF)

set(G6HE_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

target_sources(app PRIVATE
    src/main.c
    ${G6HE_ROOT}/src/g6he_crc.c
    ${G6HE_ROOT}/src/g6he_sha512.c
    ${G6HE_ROOT}/src/g6he_sha256.c
    ${G6HE_ROOT}/src/g6he_mcuboot.c
    ${G6HE_ROOT}/src/g6he_identity.c
    ${G6HE_ROOT}/src/g6he_hall.c
    ${G6HE_ROOT}/src/g6he_defaults.c
    ${G6HE_ROOT}/src/g6he_protocol.c
    ${G6HE_ROOT}/src/g6he_event.c
    ${G6HE_ROOT}/src/g6he_optomagnetic.c
    ${G6HE_ROOT}/src/g6he_sensor.c
    ${G6HE_ROOT}/src/g6he_power.c
    ${G6HE_ROOT}/src/g6he_settings.c
    ${G6HE_ROOT}/src/g6he_hid.c
    ${G6HE_ROOT}/src/g6he_wireless.c
)

target_include_directories(app PRIVATE ${G6HE_ROOT}/include)

target_compile_definitions(app PRIVATE
    G6HE_ENABLE_HARDWARE=$<BOOL:${G6HE_ENABLE_HARDWARE}>)

# Explicitly refuse to define a release/flash target.
add_custom_target(g6he-reject-flash
    COMMAND ${CMAKE_COMMAND} -E echo
            "This project is ENGINEERING-UNSIGNED-NOT-FOR-FLASHING and defines no flash target."
    COMMAND ${CMAKE_COMMAND} -E false)
