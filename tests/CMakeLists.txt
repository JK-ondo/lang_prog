# tests/cases/<段階>/<名前>.src を1件1テストとして登録する。
# ケースの書き方は tests/README.md を参照。

file(GLOB_RECURSE LPP_CASES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cases/*.src")

foreach(case_src IN LISTS LPP_CASES)
  file(RELATIVE_PATH case_rel "${CMAKE_CURRENT_SOURCE_DIR}/cases" "${case_src}")
  string(REGEX REPLACE "\\.src$" "" case_name "${case_rel}")
  add_test(
    NAME "case/${case_name}"
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/run_case.sh" "$<TARGET_FILE:mpplc>" "${case_src}")
endforeach()

list(LENGTH LPP_CASES case_count)
message(STATUS "lpp: ケーステスト ${case_count} 件")
