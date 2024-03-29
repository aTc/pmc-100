cmake_minimum_required (VERSION 3.5)
project (pmc)

add_executable(pmc pmc.c)
target_link_libraries (pmc sndfile m)
