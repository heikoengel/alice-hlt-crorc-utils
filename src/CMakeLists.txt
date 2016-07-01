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

ADD_LIBRARY(crorcutils SHARED
  crorc_hwcf_coproc_handler.cpp
  file_writer.cpp
  fcf_mapping.cpp
  event_checker.cpp
  )
TARGET_LINK_LIBRARIES(crorcutils ${LIBRORC_LIBRARY} ${ZEROMQ_LIBRARIES})
INSTALL(TARGETS crorcutils LIBRARY DESTINATION lib)
SET(EXTRA_LIBS crorcutils)

SET ( UTIL_LIB_LIST
  crorc_dma_in
  crorc_hwcf_coproc_zmq
  crorc_fcf_mapping_dump
  )

FOREACH ( UTIL ${UTIL_LIB_LIST} )
  ADD_EXECUTABLE( ${UTIL} ${UTIL}.cpp )
  TARGET_LINK_LIBRARIES( ${UTIL} ${LIBRORC_LIBRARY} ${ZEROMQ_LIBRARIES} ${EXTRA_LIBS} )
  INSTALL( TARGETS ${UTIL} RUNTIME DESTINATION bin )
ENDFOREACH( UTIL )
