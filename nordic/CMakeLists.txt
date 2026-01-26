cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nordic_clean)

# Dołączamy nasze pliki źródłowe
target_sources(app PRIVATE 
    src/main.c 
    src/protocol.c
    src/uart_serializer.c
)