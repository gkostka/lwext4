#Generic demo
add_executable(generic generic.c)
target_link_libraries(generic blockdev)
target_link_libraries(generic lwext4)
