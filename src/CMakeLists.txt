SET( UTIL_LIST
  crorc_ddr3ctrl
  crorc_qsfp_ctrl
  crorc_flash
  crorc_i2c
  crorc_reset
  crorc_sensors
  crorc_free_buffers
  crorc_status_dump )

FOREACH( UTIL ${UTIL_LIST} )
  ADD_EXECUTABLE( ${UTIL} ${UTIL}.cpp )
  TARGET_LINK_LIBRARIES( ${UTIL} ${LIBRORC} ${EXTRA_LIBS} )
  INSTALL( TARGETS ${UTIL} RUNTIME DESTINATION bin )
ENDFOREACH( UTIL )


ADD_EXECUTABLE( crorc_fpga_ctrl
  class_crorc.cpp
  crorc_fpga_ctrl.cpp
  )
TARGET_LINK_LIBRARIES( crorc_fpga_ctrl ${LIBRORC} ${EXTRA_LIBS} )
INSTALL(TARGETS crorc_fpga_ctrl RUNTIME DESTINATION bin )
