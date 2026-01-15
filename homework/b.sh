#!/bin/bash

# Очистка
rm -rf build install
rm -f async_demo bulk 2>/dev/null

mkdir build && cd build

# Настройка CMake с установкой в локальную папку
cmake -DCMAKE_INSTALL_PREFIX=../install ..

make -j$(nproc)

# Установка в локальную папку
make install

# Запуск демо из локальной папки
echo "=== Запуск демо ==="
export LD_LIBRARY_PATH=../install/lib:$LD_LIBRARY_PATH
../install/bin/async_demo

# Показать установленные файлы
echo -e "\n=== Установленные файлы ==="
find ../install -type f -name "libasync*" 2>/dev/null
find ../install -type f -name "async.h" 2>/dev/null