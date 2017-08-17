cmake_minimum_required(VERSION 3.8)
project(fdp)

set(CMAKE_CXX_STANDARD 11)

set(SRC RttEstimator.h Congestion.h
		noncopyable.h Congestion.h
		Connection.cpp Timer.h
		Segment.h Segment.cpp
		Logger.c Logger.h
		State.h)

add_compile_options(-Wall -Werror)

add_executable(testRttEstimator test/TestRttEstimator.cpp ${SRC})
add_executable(testCongestion test/TestCongestion.cpp ${SRC})
add_executable(testSegment test/TestSegment.cpp ${SRC})
add_executable(testTimer test/TestTimer.cpp ${SRC})
add_executable(testConnInput test/TestConnInput.cpp ${SRC})
add_executable(testConnOutput test/TestConnOutput.cpp ${SRC})
add_executable(netCat NetCat.cpp ${SRC})
