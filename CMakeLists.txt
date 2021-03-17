cmake_minimum_required(VERSION 3.10.2)
project(midi_parser C)

set(CMAKE_C_STANDARD 99)
include_directories(include)
add_executable(midi_parser
        proj1_main_p1.c
        src/alterations.c
        src/parser.c
        src/event_tables.c
        src/library.c
        )