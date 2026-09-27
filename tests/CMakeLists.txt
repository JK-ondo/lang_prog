# tests/cases/<段階>/<名前>.src または <名前>.argv を1件1テストとして登録する。
# ケースの書き方は tests/README.md を参照。
#
# .argv だけのケース（入力ファイルを渡さない検査など）も登録できるよう、両方を glob して
# 拡張子を取り除いた基底パスで重複を除く。run_case.sh にはその基底パスを渡す。

file(GLOB_RECURSE LPP_CASE_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cases/*.src")
file(GLOB_RECURSE LPP_CASE_ARGV CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cases/*.argv")

set(LPP_CASE_BASES "")
foreach(case_file IN LISTS LPP_CASE_SRC LPP_CASE_ARGV)
  string(REGEX REPLACE "\\.(src|argv)$" "" case_base "${case_file}")
  list(APPEND LPP_CASE_BASES "${case_base}")
endforeach()
if(LPP_CASE_BASES)
  list(REMOVE_DUPLICATES LPP_CASE_BASES)
  list(SORT LPP_CASE_BASES)
endif()

foreach(case_base IN LISTS LPP_CASE_BASES)
  file(RELATIVE_PATH case_name "${CMAKE_CURRENT_SOURCE_DIR}/cases" "${case_base}")
  add_test(
    NAME "case/${case_name}"
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/run_case.sh" "$<TARGET_FILE:mpplc>" "${case_base}")
endforeach()

list(LENGTH LPP_CASE_BASES case_count)
message(STATUS "lpp: ケーステスト ${case_count} 件")
