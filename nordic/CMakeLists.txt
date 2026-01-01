cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nordic)

# Dodaj katalog src do ścieżek nagłówkowych, żeby kompilator widział pliki .h
target_include_directories(app PRIVATE src)

# Lista wszystkich plików źródłowych projektu
target_sources(app PRIVATE 
    src/main.c
    src/uart_handle.c       # Twój nowy plik drivera
    src/command.c           #
    src/slab_alloc.c        #
    src/if_dispatcher.c     #
    src/uart_parser.c       #
    src/uart_serializer.c   #
    src/string_extra.c      #
    src/layer_void.c        #
)

# Wymuszenie włączenia biblioteki CMSIS-RTOS2 (jeśli Kconfig zawodzi)
if(CONFIG_CMSIS_RTOS_V2)
    target_link_libraries(app PRIVATE zephyr_interface)
    # Czasami nagłówek jest w 'portability'
    target_include_directories(app PRIVATE ${ZEPHYR_BASE}/include/zephyr/portability)
endif()