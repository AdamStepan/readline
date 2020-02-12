
function (add_readline_test_executable NAME SOURCES)

    add_executable(${NAME} ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCES})

    set_target_properties(${NAME} PROPERTIES
                                  CXX_STANDARD 17
                                  CXX_EXTENSIONS OFF
                                  CMAKE_CXX_STANDARD_REQUIRED ON)

    target_link_libraries(${NAME} Boost::unit_test_framework)

endfunction()

function (add_readline_test NAME SOURCES)
    add_readline_test_executable(${NAME} ${SOURCES})
    add_test(NAME ${NAME}
             COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${NAME} --logger=HRF,all,stderr --color-output=true)
endfunction()

add_readline_test(test-command-reader test_command_reader.cc)
add_readline_test(test-buffer test_buffer.cc)

