find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Quick QuickControls2 Concurrent LinguistTools)

add_library(gitapp STATIC
    ChangesModel.h ChangesModel.cpp
    DiffModel.h DiffModel.cpp
    DiffResult.h DiffResult.cpp
    BranchesModel.h BranchesModel.cpp
    HistoryModels.h HistoryModels.cpp
    HistoryController.h HistoryController.cpp
    ConflictModels.h ConflictModels.cpp
    CloneController.h CloneController.cpp
    ConflictController.h ConflictController.cpp
    AsyncTask.h
    Theme.h Theme.cpp
    LanguageController.h LanguageController.cpp
    SyntaxHighlighter.h SyntaxHighlighter.cpp
    RecentRepos.h RecentRepos.cpp
    RepoController.h RepoController.cpp
)
target_include_directories(gitapp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gitapp PUBLIC gitcore Qt6::Core Qt6::Gui Qt6::Concurrent)
if(MSVC)
    target_compile_options(gitapp PRIVATE /W4)
else()
    target_compile_options(gitapp PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(gitgui main.cpp)
qt_add_qml_module(gitgui
    URI GitGui
    VERSION 1.0
    QML_FILES
        Main.qml
        Icon.qml
        StatusIcon.qml
        GhCheckBox.qml
        ToolbarSegment.qml
        DiffView.qml
        ChangesSidebar.qml
        ChangesDetail.qml
        HistorySidebar.qml
        HistoryDetail.qml
        ConflictSidebar.qml
        ConflictDetail.qml
        DiscardDialog.qml
        CloneDialog.qml
        NewRepoDialog.qml
)
target_link_libraries(gitgui PRIVATE gitapp Qt6::Gui Qt6::Quick Qt6::QuickControls2)
qt_add_translations(gitgui
    TS_FILES ${PROJECT_SOURCE_DIR}/translations/gitgui_en.ts
    RESOURCE_PREFIX "/i18n"
)

# Smoke test: dựng toàn bộ giao diện với một repo đang conflict, fail nếu có lỗi/cảnh báo QML.
add_test(NAME smoke_qml
    COMMAND sh -c "d=$(sh ${PROJECT_SOURCE_DIR}/tests/smoke_repo.sh) && $<TARGET_FILE:gitgui> --smoke-test $d")
set_tests_properties(smoke_qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# Cùng kiểm tra nhưng đứng ở tab Lịch sử để các thành phần của tab này cũng được dựng và kiểm tra.
add_test(NAME smoke_qml_history
    COMMAND sh -c "d=$(sh ${PROJECT_SOURCE_DIR}/tests/smoke_repo.sh) && $<TARGET_FILE:gitgui> --smoke-test $d --tab 1")
set_tests_properties(smoke_qml_history PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME smoke_qml_english
    COMMAND sh -c "d=$(sh ${PROJECT_SOURCE_DIR}/tests/smoke_repo.sh) && $<TARGET_FILE:gitgui> --smoke-test $d --tab 1 --language en")
set_tests_properties(smoke_qml_english PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
