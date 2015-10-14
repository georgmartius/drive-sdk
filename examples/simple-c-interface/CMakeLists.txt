set(CMAKE_INCLUDE_CURRENT_DIR ON)

SET (CMAKE_C_FLAGS      "")

include(FindGLIB2)
include(FindReadline)

include_directories(${drivekit_SOURCE_DIR}/include
                    ${bzle_SOURCE_DIR}/include
                    ${GLIB2_INCLUDE_DIRS}
                    ${READLINE_INCLUDE_DIR}
                    )


# Add sources
set(ankiSimplifiedTest_SOURCES
                anki-simplified.c
                anki-simplified-test.c
)

add_executable(anki-simplified-test ${ankiSimplifiedTest_SOURCES})
target_link_libraries(anki-simplified-test
                    ankidrive
                    bzle
                    ${GLIB2_LIBRARIES}
                    ${READLINE_LIBRARY}
)
install(TARGETS anki-simplified-test
    RUNTIME DESTINATION bin
)
