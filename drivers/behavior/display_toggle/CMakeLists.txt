zephyr_library()
# ZMK keeps its headers private to the app target; behaviors in modules need them explicitly.
zephyr_library_include_directories(${APPLICATION_SOURCE_DIR}/include)
zephyr_library_sources(behavior_display_toggle.c)
