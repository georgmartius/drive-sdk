set(CMAKE_INCLUDE_CURRENT_DIR ON)

SET (CMAKE_C_FLAGS      "")

include(FindGLIB2)

include_directories(${drivekit_SOURCE_DIR}/include
                    ${bzle_SOURCE_DIR}/include
                    ${GLIB2_INCLUDE_DIRS}
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
)
install(TARGETS anki-simplified-test
    RUNTIME DESTINATION bin
)
