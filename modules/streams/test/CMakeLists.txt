add_executable(test_tanker_streams
  test_stream.cpp

  main.cpp
)

target_link_libraries(test_tanker_streams
  tankerstreams
  tankertesthelpers
  CONAN_PKG::doctest
)

add_test(NAME test_tanker_streams COMMAND test_tanker_streams)
