cmake_minimum_required(VERSION 3.10)
project(async VERSION 1.0.0 LANGUAGES CXX)

# Настройки компиляции
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Публичные заголовки
set(PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/async/async.h
)

# Исходные файлы библиотеки
set(SOURCE_FILES
    src/async.cpp
)

# Создание библиотеки
add_library(async SHARED ${SOURCE_FILES})
target_include_directories(async PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Создание демо программы
add_executable(async_demo src/demo.cpp)
target_link_libraries(async_demo async)

# Установка
install(TARGETS async
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS async_demo
    RUNTIME DESTINATION bin
)

install(FILES ${PUBLIC_HEADERS}
    DESTINATION include/async
)

target_compile_definitions(async
    PRIVATE
        ASYNC_EXPORTS
)

# Сборка демо
if(BUILD_DEMO)
    add_executable(async_demo
        src/demo.cpp
    )
    
    target_link_libraries(async_demo async)
    
    # Установка демо
    install(TARGETS async_demo
        DESTINATION bin
    )
endif()

# Установка библиотеки и заголовков
install(TARGETS async
    EXPORT async-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)

# Экспорт для пакета
install(EXPORT async-targets
    FILE async-config.cmake
    NAMESPACE async::
    DESTINATION lib/cmake/async
)

# Пакет для CPack
set(CPACK_PACKAGE_NAME "async")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "Async command processing library")
set(CPACK_PACKAGE_CONTACT "async-library@example.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "async-library@example.com")
set(CPACK_GENERATOR "DEB;RPM;ZIP")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.14), libstdc++6 (>= 5.2)")

include(CPack)