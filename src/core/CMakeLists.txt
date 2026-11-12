add_library(gitcore STATIC
    GitTypes.h
    GitRunner.h GitRunner.cpp
    GitParser.h GitParser.cpp
    GitDiff.h GitDiff.cpp
    GitConflict.h GitConflict.cpp
    GitIgnoreTemplates.h GitIgnoreTemplates.cpp
    GitService.h GitService.cpp
)
target_include_directories(gitcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gitcore PUBLIC Qt6::Core)
target_compile_options(gitcore PRIVATE -Wall -Wextra -Wpedantic)
