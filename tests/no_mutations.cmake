if(EXISTS "$ENV{HOME}/.config/holonight-appearance-adapters-test-marker")
  message(FATAL_ERROR "prototype unexpectedly mutated the user configuration")
endif()
