add_library(W25Q64
W25Q64.cpp
)

target_include_directories(W25Q64 PUBLIC
	.
)

target_link_libraries(W25Q64 pico_stdlib hardware_spi hardware_clocks)