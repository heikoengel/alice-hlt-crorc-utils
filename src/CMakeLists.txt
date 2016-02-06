SET( UTIL_LIST
  crorc_ddr3ctrl
  crorc_qsfp_ctrl
  crorc_flash
  crorc_i2c
  crorc_reset
  crorc_sensors
  crorc_free_buffers
  crorc_push_file
  crorc_dma_benchmark
  crorc_status_dump )

FOREACH( UTIL ${UTIL_LIST} )
  ADD_EXECUTABLE( ${UTIL} ${UTIL}.cpp )
  TARGET_LINK_LIBRARIES( ${UTIL} ${LIBRORC_LIBRARY} ${EXTRA_LIBS} )
  INSTALL( TARGETS ${UTIL} RUNTIME DESTINATION bin )
ENDFOREACH( UTIL )

ADD_EXECUTABLE( crorc_fpga_ctrl
  class_crorc.cpp
  crorc_fpga_ctrl.cpp
  )
TARGET_LINK_LIBRARIES( crorc_fpga_ctrl ${LIBRORC_LIBRARY} ${EXTRA_LIBS} )
INSTALL(TARGETS crorc_fpga_ctrl RUNTIME DESTINATION bin )

ADD_EXECUTABLE( crorc_dma_in
  event_checker.cpp
  file_writer.cpp
  fcf_mapping.cpp
  crorc_dma_in.cpp)
TARGET_LINK_LIBRARIES( crorc_dma_in ${LIBRORC_LIBRARY} ${EXTRA_LIBS} )
INSTALL(TARGETS crorc_dma_in RUNTIME DESTINATION bin )
