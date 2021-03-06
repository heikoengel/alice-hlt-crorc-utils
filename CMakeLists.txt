CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11 FATAL_ERROR)
PROJECT(crorc-utils)
SET(VERSION "1.6.0")
SET(CPACK_RPM_PACKAGE_RELEASE 1)

MESSAGE( STATUS "Install directory   = ${CMAKE_INSTALL_PREFIX}")

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/")

FIND_PACKAGE(librorc 16.0.0 REQUIRED)
FIND_PACKAGE(ZeroMQ REQUIRED)

INCLUDE_DIRECTORIES(${LIBRORC_INCLUDE_DIR})

ADD_DEFINITIONS(-Wall)
#ADD_DEFINITIONS(-DMODELSIM)

# add actual code
ADD_SUBDIRECTORY(src)

# add helper scripts
ADD_SUBDIRECTORY(scripts)

# CPack
SET(CPACK_PACKAGE_VERSION ${VERSION})
SET(CPACK_GENERATOR "RPM")
SET(CPACK_PACKAGE_NAME "crorc-utils")
SET(CPACK_RPM_PACKAGE_LICENSE "BSD")
SET(CPACK_RPM_PACKAGE_GROUP "System Environment/Base")
SET(CPACK_PACKAGE_CONTACT "Heiko Engel <hengel@cern.ch>")
SET(CPACK_PACKAGE_VENDOR "CERN")
SET(CPACK_PACKAGING_INSTALL_PREFIX /usr)
SET(CPACK_PACKAGE_FILE_NAME
  "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")
INCLUDE(CPack)
