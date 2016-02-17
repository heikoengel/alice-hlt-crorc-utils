# Try to find librorc
# once found this module will define:
#  LIBRORC_FOUND
#  LIBRORC_LIBRARY
#  LIBRORC_INCLUDE_DIR
#  LIBRORC_VERSION


FIND_PATH(LIBRORC_INCLUDE_DIR NAMES librorc.h)
FIND_LIBRARY(LIBRORC_LIBRARY NAMES rorc)

FILE(READ ${LIBRORC_INCLUDE_DIR}/librorc.h _librorc_VERSION_H_CONTENTS)
STRING(REGEX REPLACE ".*#define LIBRORC_VERSION ([0-9]+).*" "\\1" librorc_VERSION_NUMERIC "${_librorc_VERSION_H_CONTENTS}")
IF(librorc_VERSION_NUMERIC GREATER 0)
  MATH(EXPR LIBRORC_MAJOR_VERSION "${librorc_VERSION_NUMERIC} / 100000")
  MATH(EXPR LIBRORC_MINOR_VERSION "${librorc_VERSION_NUMERIC} / 100 % 1000")
  MATH(EXPR LIBRORC_SUBMINOR_VERSION "${librorc_VERSION_NUMERIC} % 100")
ENDIF(librorc_VERSION_NUMERIC GREATER 0)
SET(LIBRORC_VERSION "${LIBRORC_MAJOR_VERSION}.${LIBRORC_MINOR_VERSION}.${LIBRORC_SUBMINOR_VERSION}")

if(librorc_FIND_VERSION)
endif()

SET(LIBRORC_LIBRARIES ${LIBRORC_LIBRARY})
SET(LIBRORC_INCLUDE_DIRS ${LIBRORC_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(librorc REQUIRED_VARS LIBRORC_LIBRARIES
  VERSION_VAR LIBRORC_VERSION)
mark_as_advanced(LIBRORC_INCLUDE_DIRS, LIBRORC_LIBRARIES)