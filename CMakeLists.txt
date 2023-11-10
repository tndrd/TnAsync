cmake_minimum_required(VERSION 3.0)
project(ThreadPool C CXX)

include_directories(Inc/)

set(CMAKE_C_FLAGS "-Werror")

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        main
)

FetchContent_Declare(
  TnStatus
  GIT_REPOSITORY https://github.com/tndrd/TnStatus.git
  GIT_TAG        master
)

FetchContent_MakeAvailable(googletest TnStatus)

add_library(Worker Src/Worker/Worker.c)
target_link_libraries(Worker PUBLIC pthread TnStatus)

add_library(WorkerArray Src/ThreadPool/WorkerArray.c)
target_link_libraries(WorkerArray PUBLIC Worker)

add_library(WorkerQueue Src/ThreadPool/WorkerQueue.c)
target_link_libraries(WorkerQueue PUBLIC Worker)

add_library(WQMonitor Src/ThreadPool/WQMonitor.c)
target_link_libraries(WQMonitor PUBLIC WorkerQueue pthread)

add_library(TaskQueue Src/ThreadPool/TaskQueue.c)
target_link_libraries(TaskQueue PUBLIC Worker)

add_library(TQMonitor Src/ThreadPool/TQMonitor.c)
target_link_libraries(TQMonitor PUBLIC TaskQueue pthread)

add_library(ThreadPool Src/ThreadPool/ThreadPool.c)
target_link_libraries(ThreadPool PUBLIC TQMonitor WQMonitor WorkerArray)
target_include_directories(ThreadPool PUBLIC Inc/)

set(TEST_EXECUTABLE ${PROJECT_NAME}_RunTests)

add_executable(${TEST_EXECUTABLE} Tests/RunTests.cpp)
target_link_libraries(${TEST_EXECUTABLE} PRIVATE ThreadPool GTest::gtest_main)