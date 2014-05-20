#Generic demo
add_executable(fileimage_demo main.c)
target_link_libraries(fileimage_demo lwext4)
target_link_libraries(fileimage_demo blockdev)