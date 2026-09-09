# Copyright (c) 2020 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_include_directories(.)

zephyr_library()

zephyr_library_sources(ec11_classic.c)
zephyr_library_sources_ifdef(CONFIG_EC11_CLASSIC_TRIGGER ec11_classic_trigger.c)