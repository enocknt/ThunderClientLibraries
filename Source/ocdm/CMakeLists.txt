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

cmake_minimum_required(VERSION 3.15)

find_package(Thunder)

project(OCDM)

project_version(1.0.0)

set(TARGET Client${PROJECT_NAME})

message("Setup ${TARGET} v${PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(CompileSettingsDebug CONFIG REQUIRED)
find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}COM REQUIRED)
find_package(${NAMESPACE}Messaging REQUIRED)

set(CDMI_ADAPTER_IMPLEMENTATION "None" CACHE STRING "Defines which implementation is used.")

add_library(${TARGET}
        CapsParser.cpp
        open_cdm.cpp
        open_cdm_ext.cpp
        open_cdm_impl.cpp
        )

set(PUBLIC_HEADERS
        open_cdm.h
        adapter/open_cdm_adapter.h
        open_cdm_ext.h
        CapsParser.h
        Module.h
        )

target_compile_options (${TARGET} PRIVATE -Wno-psabi)

target_link_libraries(${TARGET}
        PRIVATE
          ${NAMESPACE}Core::${NAMESPACE}Core
          ${NAMESPACE}COM::${NAMESPACE}COM
          ${NAMESPACE}Messaging::${NAMESPACE}Messaging
          CompileSettingsDebug::CompileSettingsDebug
        )

target_include_directories( ${TARGET}
        PUBLIC
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/opencdm>
          $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/..>
        )

target_include_directories( ${TARGET}
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/adapter>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        )

if(NOT "${CDMI_ADAPTER_IMPLEMENTATION}" STREQUAL "None")
        message("${CDMI_ADAPTER_IMPLEMENTATION}")
        find_package(GSTREAMER REQUIRED)
        find_package(GSTREAMER_BASE REQUIRED)

        if(("${CDMI_ADAPTER_IMPLEMENTATION}" STREQUAL "broadcom-svp") OR ("${CDMI_ADAPTER_IMPLEMENTATION}" STREQUAL "broadcom-svp-secbuf"))
            if (OCDM_IMPLEMENTATION_PATH)
                target_sources(${TARGET} PRIVATE ${OCDM_IMPLEMENTATION_PATH}/open_cdm_adapter.cpp)
            else()
                include(GetExternalCode)
                set(OCDM_IMPLEMENTATION_VERSION "master" CACHE STRING "ocdmadapter implementation version: commit_id_or_tag_or_branch_name")
                set(OCDM_IMPLEMENTATION_REPOSITORY "https://code.rdkcentral.com/r/soc/broadcom/components/rdkcentral/thunder/ocdmadapter" CACHE STRING "ocdmadapter implementation repository")
                GetExternalCode(
                    GIT_REPOSITORY ${OCDM_IMPLEMENTATION_REPOSITORY}
                    GIT_VERSION ${OCDM_IMPLEMENTATION_VERSION}
                    SOURCE_DIR "adapter/broadcom-svp"
                )
                if("${CDMI_ADAPTER_IMPLEMENTATION}" STREQUAL "broadcom-svp")
                    target_sources(${TARGET} PRIVATE adapter/broadcom-svp/open_cdm_adapter.cpp)
                else()
                    target_sources(${TARGET} PRIVATE adapter/broadcom-svp/open_cdm_adapter-secbuf.cpp)
                endif()
            endif()
        else()
            target_sources(${TARGET} PRIVATE adapter/${CDMI_ADAPTER_IMPLEMENTATION}/open_cdm_adapter.cpp)
        endif()

        target_link_libraries(${TARGET}
                PRIVATE
                ${GSTREAMER_LIBRARIES}
                ${GSTREAMER_BASE_LIBRARIES}
                CompileSettingsDebug::CompileSettingsDebug
                )
        
        target_include_directories( ${TARGET}
                SYSTEM PRIVATE
                ${GSTREAMER_INCLUDES}
                ${GSTREAMER_BASE_INCLUDES}
                )
else()
        target_link_libraries(${TARGET}
                PRIVATE
                CompileSettingsDebug::CompileSettingsDebug
                )
endif()

if (${CDMI_ADAPTER_IMPLEMENTATION} STREQUAL "broadcom-svp")
    find_package(NEXUS REQUIRED)
    target_link_libraries(${TARGET}
            PRIVATE
            NEXUS::NEXUS
            -lbrcmsvpmeta
            )
endif()

if (${CDMI_ADAPTER_IMPLEMENTATION} STREQUAL "broadcom-svp-secbuf")
    target_link_libraries(${TARGET}
            PRIVATE
            -lb_secbuf
            -lgstsvpext
            )
endif()

if (${CDMI_ADAPTER_IMPLEMENTATION} STREQUAL "rdk")
    target_link_libraries(${TARGET}
            PRIVATE
            -lgstsvpext
            )
endif()

set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        FRAMEWORK FALSE
        PUBLIC_HEADER "${PUBLIC_HEADERS}" # specify the public headers
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        )

install(
        TARGETS ${TARGET}  EXPORT ${TARGET}Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${NAMESPACE}_Development
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${NAMESPACE}_Runtime NAMELINK_COMPONENT ${NAMESPACE}_Development
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${NAMESPACE}_Runtime
        FRAMEWORK DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${NAMESPACE}_Runtime
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${NAMESPACE}/ocdm COMPONENT ${NAMESPACE}_Development
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${NAMESPACE}/ocdm
)

#FIXME: This needs to become a sysmlink.
install(
        FILES ${PUBLIC_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/opencdm
)

InstallCMakeConfig(
        TARGETS ${TARGET})

InstallPackageConfig(
        TARGETS ${TARGET} 
        DESCRIPTION "OCDM library")
