#Generic demo
add_executable(generic main.c)
target_link_libraries(generic lwext4)
target_link_libraries(generic blockdev)