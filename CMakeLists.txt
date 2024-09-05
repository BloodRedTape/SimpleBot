cmake_minimum_required (VERSION 3.8)

project ("SimpleBot")

add_library(SimpleBot "./sources/simple_bot.cpp")
target_link_libraries(SimpleBot PUBLIC TgBot PUBLIC bsl)
target_include_directories(SimpleBot PUBLIC "./include")