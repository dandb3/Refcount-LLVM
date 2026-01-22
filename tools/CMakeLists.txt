# #### static ####

# set(static_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/StaticMain.cpp"
#   "${CMAKE_CURRENT_SOURCE_DIR}/../lib/StaticCallCounter.cpp"
# )

#### FindCNP ####

set(FindCNP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/FindCNP.cpp"
)

add_executable(FindCNP ${FindCNP_SOURCES})

target_include_directories(
  FindCNP
  PRIVATE
)

target_link_libraries(FindCNP
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

# #### refcount_select ####

# set(refcount_select_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/refcount_select.cpp"
# )

# add_executable(refcount_select ${refcount_select_SOURCES})

# target_include_directories(
#   refcount_select
#   PRIVATE
# )

# target_link_libraries(refcount_select
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

# #### LLVMpassTest ####

# set(LLVMpassTest_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/LLVMpassTest.cpp"
# )

# add_executable(LLVMpassTest ${LLVMpassTest_SOURCES})

# target_include_directories(
#   LLVMpassTest
#   PRIVATE
# )

# target_link_libraries(LLVMpassTest
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

#### AnalysisLTO ####

set(AnalysisLTO_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/AnalysisLTO.cpp"
)

add_executable(AnalysisLTO ${AnalysisLTO_SOURCES})

target_include_directories(
  AnalysisLTO
  PRIVATE
)

target_link_libraries(AnalysisLTO
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

# #### FindRefcountOps ####

# set(FindRefcountOps_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/FindRefcountOps.cpp"
# )

# add_executable(FindRefcountOps ${FindRefcountOps_SOURCES})

# target_include_directories(
#   FindRefcountOps
#   PRIVATE
# )

# target_link_libraries(FindRefcountOps
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

# #### CIDRefcountCollection ####

# set(CIDRefcountCollection_SOURCES
#   "${CMAKE_CURRENT_SOURCE_DIR}/CIDRefcountCollection.cpp"
# )

# add_executable(CIDRefcountCollection ${CIDRefcountCollection_SOURCES})

# target_include_directories(
#   CIDRefcountCollection
#   PRIVATE
# )

# target_link_libraries(CIDRefcountCollection
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
