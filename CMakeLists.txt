# MIT License
#
# Copyright (c) 2026 lmschatlib contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.16)
project(lmschatlib LANGUAGES CXX)

option(LMSCHAT_BUILD_EXAMPLES "Build examples" OFF)

add_library(lmschat src/client.cpp)

target_include_directories(lmschat
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(lmschat PUBLIC cxx_std_20)

if(LMSCHAT_BUILD_EXAMPLES)
  add_executable(lmschat_example examples/usage.cpp)
  target_link_libraries(lmschat_example PRIVATE lmschat)

  add_executable(lmschat_one_shot examples/one_shot.cpp)
  target_link_libraries(lmschat_one_shot PRIVATE lmschat)
endif()
