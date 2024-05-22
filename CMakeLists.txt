cmake_minimum_required(VERSION 3.10.0)

project(face_recognition)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# skip 3rd-party lib dependencies
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++fs -lpthread -ldl -Wl,--allow-shlib-undefined") #aarch64
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -std=c++17 -lstdc++fs -Wl,--allow-shlib-undefined")
# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/face_recognition_${CMAKE_SYSTEM_NAME})

set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

if(CMAKE_C_COMPILER MATCHES "aarch64")
  set(LIB_ARCH aarch64)
else()
  set(LIB_ARCH armhf)
endif()

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/3rdparty) #包含各个操作平台的opencv库等

#openmp
find_package(OpenMP REQUIRED)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

#ncnn
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(ncnn_DIR ${CMAKE_SOURCE_DIR}/3rdparty/ncnn/ncnn-android-sdk/${CMAKE_ANDROID_ARCH_ABI}/lib/cmake/ncnn)
  link_directories(${CMAKE_SOURCE_DIR}/3rdparty/ncnn/ncnn-android-sdk/${CMAKE_ANDROID_ARCH_ABI}/lib)
else()   #aarch64
  set(ncnn_DIR ${CMAKE_SOURCE_DIR}/3rdparty/ncnn/ncnn-linux-aarch64/lib/cmake/ncnn)
  link_directories(${CMAKE_SOURCE_DIR}/3rdparty/ncnn/ncnn-linux-aarch64/lib)
endif()
find_package(ncnn REQUIRED)

# opencv
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/3rdparty/opencv/opencv4-android-sdk/native/jni/abi-${CMAKE_ANDROID_ARCH_ABI})

else()
  set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/3rdparty/opencv/opencv4-linux-aarch64/lib/cmake/opencv4)
endif()

find_package(OpenCV REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

include_directories(${CMAKE_CURRENT_LIST_DIR}/include/ncnn)

add_executable(face_recognition	
  src/livefacereco.cpp
  src/live.cpp
  src/arcface.cpp
  src/mtcnn_new.cpp
  src/parallel_video_capture.cpp
  src/main.cpp
)

target_link_libraries(face_recognition
  ${OpenCV_LIBS}
  ncnn
)

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/face_recognition_${CMAKE_SYSTEM_NAME})

install(TARGETS face_recognition DESTINATION ./)
install(DIRECTORY add_picture DESTINATION ./)

install(DIRECTORY models/live DESTINATION ./models)
install(DIRECTORY models/mobilefacenet DESTINATION ./models)
install(DIRECTORY models/mtcnn DESTINATION ./models)

file(GLOB IMAGE_FILES "img/*.jpg")
install(FILES ${IMAGE_FILES} DESTINATION ./img/)    

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  file(GLOB OPENCV_LIBS	"${CMAKE_SOURCE_DIR}/3rdparty/opencv/opencv4-android-sdk/native/staticlibs/${CMAKE_ANDROID_ARCH_ABI}/*.a")
  install(FILES ${OPENCV_LIBS} DESTINATION ./lib/)
  file(GLOB NCNN_LIBS	"${CMAKE_SOURCE_DIR}/3rdparty/ncnn/ncnn-android-sdk/${CMAKE_ANDROID_ARCH_ABI}/lib/*.a")
  install(FILES ${NCNN_LIBS} DESTINATION ./lib/)
else()   #aarch64
  file(GLOB OPENCV_LIBS "${CMAKE_SOURCE_DIR}/3rdparty/opencv/opencv4-linux-aarch64/lib/*.so*")
  install(FILES ${OPENCV_LIBS} DESTINATION ./lib/)
  file(GLOB NCNN_LIBS   "${CMAKE_SOURCE_DIR}/3rdparty/ncnn/ncnn-linux-aarch64/lib/*.a")
  install(FILES ${NCNN_LIBS} DESTINATION ./lib/)
endif()



