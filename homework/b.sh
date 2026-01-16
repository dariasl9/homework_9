#!/bin/bash

set -ex -o pipefail

# Очистка
rm -rf build install
rm -f async_demo bulk 2>/dev/null

mkdir build && cd build

# Поиск Boost
BOOST_ROOT="/usr/local/boost"  # укажите ваш путь к Boost
if [ -d "$BOOST_ROOT" ]; then
    cmake -DBOOST_ROOT=$BOOST_ROOT ..
else
    cmake ..
fi

# # Настройка CMake с установкой в локальную папку
# cmake -DCMAKE_INSTALL_PREFIX=../install ..

make -j$(nproc)

# Запуск сервера в фоне
echo "=== Запуск сервера на порту 9000 с размером блока 3 ==="
./bulk_server 9000 3 &
SERVER_PID=$!

sleep 2

# Тест 1: последовательные команды
echo "=== Тест 1: seq 0 9 | nc localhost 9000 ==="
seq 0 9 | timeout 2 nc localhost 9000 2>/dev/null || true
sleep 1

# Тест 2: динамические блоки
echo -e "\n=== Тест 2: динамические блоки ==="
echo -e "cmd1\ncmd2\n{\ndyn1\ndyn2\n}\ncmd3\n" | timeout 2 nc localhost 9000 2>/dev/null || true
sleep 1

# Тест 3: несколько клиентов одновременно
echo -e "\n=== Тест 3: несколько клиентов ==="
(
    echo "client1_cmd1"
    echo "client1_cmd2"
    sleep 0.5
    echo "client1_cmd3"
) | timeout 3 nc localhost 9000 2>/dev/null &
PID1=$!

(
    sleep 0.2
    echo "client2_cmd1"
    echo "client2_cmd2"
    sleep 0.5
    echo "client2_cmd3"
) | timeout 3 nc localhost 9000 2>/dev/null &
PID2=$!

wait $PID1 $PID2
sleep 2

# Остановка сервера
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

# Показать созданные файлы
echo -e "\n=== Созданные файлы ==="
ls -la bulk*.log 2>/dev/null || echo "Файлы не найдены"

# Подсчет файлов
echo -e "\n=== Статистика ==="
FILE_COUNT=$(ls bulk*.log 2>/dev/null | wc -l)
echo "Создано файлов: $FILE_COUNT"

# Показать содержимое нескольких файлов
if [ $FILE_COUNT -gt 0 ]; then
    echo -e "\n=== Содержимое первых 3 файлов ==="
    for file in $(ls bulk*.log 2>/dev/null | head -3); do
        echo "=== $file ==="
        cat $file
    done
fi

# # Установка в локальную папку
# make install

# # Запуск демо из локальной папки
# echo "=== Запуск демо ==="
# export LD_LIBRARY_PATH=../install/lib:$LD_LIBRARY_PATH
# ../install/bin/async_demo

# # Показать установленные файлы
# echo -e "\n=== Установленные файлы ==="
# find ../install -type f -name "libasync*" 2>/dev/null
# find ../install -type f -name "async.h" 2>/dev/null