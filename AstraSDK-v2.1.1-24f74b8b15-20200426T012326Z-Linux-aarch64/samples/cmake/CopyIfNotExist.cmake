if (DEFINED CMAKE_SCRIPT_MODE_FILE)

  if(NOT EXISTS ${DEST})
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DEST})
  endif()

else()

  # Save the folder name of the current file so it is used correctly when the
  # function below is invoked.
  set(COPY_IF_NOT_EXIST_DIR ${CMAKE_CURRENT_LIST_DIR})

  function(copy_if_not_exist tgt src dest)

    if (NOT IS_ABSOLUTE ${src})
      set(src "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
    endif()

    add_custom_command(
      TARGET ${tgt}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -DDEST=${dest} -DSRC=${src} -P ${COPY_IF_NOT_EXIST_DIR}/CopyIfNotExist.cmake)

  endfunction()

endif()
