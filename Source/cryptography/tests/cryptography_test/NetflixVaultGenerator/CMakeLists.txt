# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2020 Metrological
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
project(NetflixVaultGenerator)
cmake_minimum_required(VERSION 3.15)

find_package(OpenSSL REQUIRED)

option(USE_PROVISIONING "Prepare data for a provisioning json" ON)

add_executable(NetflixVaultGenerator
   main.cpp
)

target_link_libraries(NetflixVaultGenerator
   PRIVATE
   OpenSSL::Crypto
   dl
)

if(USE_PROVISIONING)
   message(STATUS "Build with provisioning support")

   target_compile_definitions(NetflixVaultGenerator
   PRIVATE
       USE_PROVISIONING
   )
endif()

install(TARGETS NetflixVaultGenerator DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${NAMESPACE}_Test)

