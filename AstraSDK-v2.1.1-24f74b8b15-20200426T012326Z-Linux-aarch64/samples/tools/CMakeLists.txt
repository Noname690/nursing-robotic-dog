if (ASTRA_HAND)
    add_subdirectory(DebugHandViewer)
endif()

if (ASTRA_SDK_BUILD AND ASTRA_STREAMPLAYER)
  add_subdirectory(StreamRecorder)
endif()
