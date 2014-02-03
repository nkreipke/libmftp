include_directories(${libmftp_SOURCE_DIR}/src)
link_directories(${libmftpSOURCE_DIR}/src)
aux_source_directory(. SRC_LIST)

add_executable(libmftp_test.out ${SRC_LIST})
target_link_libraries(libmftp_test.out mftp)
