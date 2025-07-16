# #### static ####

# set(static_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/StaticMain.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/../lib/StaticCallCounter.cpp"
# )

# add_executable(static ${static_SOURCES})

# target_include_directories(
#   static
#   PRIVATE
#   "${CMAKE_CURRENT_SOURCE_DIR}/../include")

# if(UNIX AND EXISTS "/etc/arch-release")
#   # LLVM is built as shared library on Arch Linux (*), so we need to link the
#   # static executable against libLLVM.so. See #117
#   # (*)  https://gitlab.archlinux.org/archlinux/packaging/packages/llvm/-/blob/main/PKGBUILD?ref_type=heads#L89
#   message("LLVM is installed as shared library on Arch Linux")
#   target_link_libraries(static LLVM)
# else()
#   target_link_libraries(static
#     LLVMCore LLVMPasses LLVMIRReader LLVMSupport
#   )
# endif()

# #### compare_llvm_clang ####

# set(compare_llvm_clang_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/../lib/ClangRefcount.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/../lib/RefcountMain.cpp"
# )

# add_executable(compare_llvm_clang ${compare_llvm_clang_SOURCES})

# target_include_directories(
#   compare_llvm_clang
#   PRIVATE
#   "${CMAKE_CURRENT_SOURCE_DIR}/../include")

# target_link_libraries(compare_llvm_clang
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

# #### recursive ####

# set(recursive_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/refcount_recursive_check.cpp"
# )

# add_executable(recursive ${recursive_SOURCES})

# target_include_directories(
#   recursive
#   PRIVATE
# )

# target_link_libraries(recursive
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

# #### find_llvm_anon ####

# set(find_llvm_anon_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/FindLLVMAnon.cpp"
# )

# add_executable(find_llvm_anon ${find_llvm_anon_SOURCES})

# target_include_directories(
#   find_llvm_anon
#   PRIVATE
# )

# target_link_libraries(find_llvm_anon
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

# #### struct-based ####

# set(struct_based_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/refcount_struct_based_check.cpp"
# )

# add_executable(struct_based ${struct_based_SOURCES})

# target_include_directories(
#   struct_based
#   PRIVATE
# )

# target_link_libraries(struct_based
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

# #### refcount_count ####

# set(refcount_count_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/refcount_count.cpp"
# )

# add_executable(refcount_count ${refcount_count_SOURCES})

# target_include_directories(
#   refcount_count
#   PRIVATE
# )

# target_link_libraries(refcount_count
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

#### refcount_select ####

set(refcount_select_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/refcount_select.cpp"
)

add_executable(refcount_select ${refcount_select_SOURCES})

target_include_directories(
  refcount_select
  PRIVATE
)

target_link_libraries(refcount_select
  LLVMCore
  LLVMSupport
  LLVMIRReader
  LLVMBitWriter
  LLVMOption
  LLVMFrontendOpenMP
  clangTooling
  clangFrontend
  clangDriver
  clangSerialization
  clangAnalysis
  clangAST
  clangASTMatchers
  clangCodeGen
  clangParse
  clangSema
  # clangSupport
  clangLex
  clangToolingCore
  clangBasic
)

# #### test ####

# set(main_test_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/main_test.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/../lib/ClangRefcountFunc/ClangRefcountFunc.cpp"
# )

# add_executable(main_test ${main_test_SOURCES})

# target_include_directories(
#   main_test
#   PRIVATE
#   "${CMAKE_CURRENT_SOURCE_DIR}/../include"
# )

# target_link_libraries(main_test
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

# #### dup_struct ####

# set(dup_struct_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/FindDupStruct.cpp"
# )

# add_executable(dup_struct ${dup_struct_SOURCES})

# target_include_directories(
#   dup_struct
#   PRIVATE
# )

# target_link_libraries(dup_struct
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

# #### dup_struct_local ####

# set(dup_struct_local_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/FindDupStructLocal.cpp"
# )

# add_executable(dup_struct_local ${dup_struct_local_SOURCES})

# target_include_directories(
#   dup_struct_local
#   PRIVATE
# )

# target_link_libraries(dup_struct_local
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

# #### AnalysisCNP ####

# set(AnalysisCNP_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/AnalysisCNP.cpp"
# )

# add_executable(AnalysisCNP ${AnalysisCNP_SOURCES})

# target_include_directories(
#   AnalysisCNP
#   PRIVATE
# )

# target_link_libraries(AnalysisCNP
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

# #### FindNestedStruct ####

# set(FindNestedStruct_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/FindNestedStruct.cpp"
# )

# add_executable(FindNestedStruct ${FindNestedStruct_SOURCES})

# target_include_directories(
#   FindNestedStruct
#   PRIVATE
# )

# target_link_libraries(FindNestedStruct
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

# #### MatcherTest ####

# set(MatcherTest_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/MatcherTest.cpp"
# )

# add_executable(MatcherTest ${MatcherTest_SOURCES})

# target_include_directories(
#   MatcherTest
#   PRIVATE
# )

# target_link_libraries(MatcherTest
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )

# #### new_compare ####

# set(new_compare_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/NewMain.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/../lib/NewClangRefcount.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/../lib/NewRefcountMain.cpp"
# )

# add_executable(new_compare ${new_compare_SOURCES})

# target_include_directories(
#   new_compare
#   PRIVATE
#   "${CMAKE_CURRENT_SOURCE_DIR}/../include")

# target_link_libraries(new_compare
#   LLVMCore
#   LLVMSupport
#   LLVMIRReader
#   LLVMBitWriter
#   LLVMOption
#   LLVMFrontendOpenMP
#   clangTooling
#   clangFrontend
#   clangDriver
#   clangSerialization
#   clangAnalysis
#   clangAST
#   clangASTMatchers
#   clangCodeGen
#   clangParse
#   clangSema
#   # clangSupport
#   clangLex
#   clangToolingCore
#   clangBasic
# )