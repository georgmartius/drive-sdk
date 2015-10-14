set(CMAKE_INCLUDE_CURRENT_DIR ON)

SET (CMAKE_C_FLAGS      "")

include(FindGLIB2)

include_directories(${drivekit_SOURCE_DIR}/include
                    ${bzle_SOURCE_DIR}/include
                    ${GLIB2_INCLUDE_DIRS}
                    )

# Add sources
set(ankiSimplified_SOURCES
  anki-simplified.c anki-simplified.h
)

add_library(ankidrivesimplified SHARED ${ankiSimplified_SOURCES})
target_link_libraries(ankidrivesimplified
                      ankidrive
                      bzle)

add_executable(anki-simplified-test anki-simplified-test.c ${ankiSimplified_SOURCES})

target_link_libraries(anki-simplified-test
                    ankidrive
                    bzle
                    ${GLIB2_LIBRARIES}
)

install(TARGETS ankidrivesimplified
  LIBRARY DESTINATION lib
)

install(FILES anki-simplified.h DESTINATION include/ankidrive)

install(TARGETS anki-simplified-test
  RUNTIME DESTINATION bin
)
