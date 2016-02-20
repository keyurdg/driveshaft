set(can_run_int_test YES)

execute_process(
    COMMAND sh -c "php -i | grep gearman.*enabled" 
    RESULT_VARIABLE 
    grep_rv 
    OUTPUT_QUIET 
    ERROR_QUIET
)

if(${grep_rv} EQUAL 0)
  foreach(prog bash gearmand php)
    unset(prog_path CACHE)
    find_program(prog_path ${prog})
    if(NOT prog_path)
      set(can_run_int_test NO)
      break()
    endif()
  endforeach()
else()
  set(can_run_int_test NO)
endif()

if(${can_run_int_test})
  add_executable(integration_test IMPORTED)
  set_property(
    TARGET integration_test 
    PROPERTY IMPORTED_LOCATION ./integration.sh
  )
  add_test(IntegrationTest ../../tests/integration.sh)
endif()
