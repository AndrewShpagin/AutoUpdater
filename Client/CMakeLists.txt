# CMakeList.txt : CMake project for HeapFilesSync, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project (AutoUpdater)

set(CMAKE_CXX_STANDARD 17)

# Add source to this project's executable.
add_executable (
	AutoUpdater
	"main.cpp"
	"../Common/HeapFilesSync.cpp" "../Common/HeapFilesSync.h" 
	"../Common/miniz.c" "../Common/zpp.h"
	"../Common/ui.cpp" "../Common/ui.h" 
	"../Common/ProjectsManager.cpp" "../Common/ProjectsManager.h" 
	"../Common/download.cpp" "../Common/download.h"
 "link.cpp" "../Common/tools.cpp" "../Common/tools.h")

message("The build type: " ${CMAKE_BUILD_TYPE})
IF (WIN32)
	# Need to install OpenSSL libraries, they are available at https://slproweb.com/products/Win32OpenSSL.html
	set(OPENSSL_ROOT_DIR C:/Program Files/OpenSSL-Win64/*)
	IF(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		SET_TARGET_PROPERTIES(AutoUpdater PROPERTIES LINK_FLAGS    "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS")
	ENDIF()
ENDIF()


set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
target_link_libraries(AutoUpdater OpenSSL::Crypto)
target_link_libraries(AutoUpdater OpenSSL::SSL)

# TODO: Add tests and install targets if needed. 
