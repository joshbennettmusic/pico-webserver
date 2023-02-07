add_library(filesys
filesys.cpp
)

target_include_directories(filesys PUBLIC
	.
)

target_link_libraries(filesys pico_stdlib W25Q64 )