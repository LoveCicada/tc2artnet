cmake_minimum_required(VERSION 3.10)
project(TC2Artnet LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)

# 添加定义
add_definitions(-DQT_DEPRECATED_WARNINGS)
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x060000)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/3rd/libltc/include)
include_directories(${CMAKE_SOURCE_DIR}/../libltc_build/include)

# 源文件和头文件
set(SOURCES
    src/main.cpp
    src/tc2artnet.cpp
    src/mainwindow.cpp
    src/ltcreceiver.cpp
    src/ltcdecoder.cpp
    src/transmitter.cpp
)

set(HEADERS
    src/tc2artnet.h
    src/mainwindow.h
    src/ltcreceiver.h
    src/ltcdecoder.h
    src/transmitter.h
    src/timecode_frame.h
)

# 平台特定配置
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-DENABLE_MTC)

    # 添加 Windows 特定源文件
    list(APPEND SOURCES
        src/mtcreceiver.cpp
        src/mididev_win32.cpp
    )

    list(APPEND HEADERS
        src/mtcreceiver.h
        src/mididev.h
    )
endif()

# 添加可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 查找 Qt 模块
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::Multimedia
)

# 链接 libltc 库
message(STATUS "LINK_DIR: ${CMAKE_SOURCE_DIR}/3rd/libltc/lib/Debug")
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/3rd/libltc/lib/Debug/ltc.lib)

get_target_property(LINK_LIBS ${PROJECT_NAME} LINK_LIBRARIES)
message(STATUS "Linked libraries: ${LINK_LIBS}")

# Windows 特定配置
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Winmm)

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE "/MT")
    endif()
endif()

# 安装规则
if(QNX)
    set(TARGET_PATH /tmp/${PROJECT_NAME}/bin)
elseif(UNIX AND NOT ANDROID)
    set(TARGET_PATH /opt/${PROJECT_NAME}/bin)
endif()

if(TARGET_PATH)
    install(TARGETS ${PROJECT_NAME} DESTINATION ${TARGET_PATH})
endif()