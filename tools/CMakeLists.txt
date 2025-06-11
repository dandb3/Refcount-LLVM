#### static ####

set(static_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/StaticMain.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/StaticCallCounter.cpp"
)

add_executable(static ${static_SOURCES})

target_include_directories(
  static
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")

if(UNIX AND EXISTS "/etc/arch-release")
  # LLVM is built as shared library on Arch Linux (*), so we need to link the
  # static executable against libLLVM.so. See #117
  # (*)  https://gitlab.archlinux.org/archlinux/packaging/packages/llvm/-/blob/main/PKGBUILD?ref_type=heads#L89
  message("LLVM is installed as shared library on Arch Linux")
  target_link_libraries(static LLVM)
else()
  target_link_libraries(static
    LLVMCore LLVMPasses LLVMIRReader LLVMSupport
  )
endif()

#### compare_llvm_clang ####

set(compare_llvm_clang_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/ClangRefcount.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/RefcountMain.cpp"
)

add_executable(compare_llvm_clang ${compare_llvm_clang_SOURCES})

target_include_directories(
  compare_llvm_clang
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")

target_link_libraries(compare_llvm_clang
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

#### recursive ####

set(recursive_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/refcount_recursive_check.cpp"
)

add_executable(recursive ${recursive_SOURCES})

target_include_directories(
  recursive
  PRIVATE
)

target_link_libraries(recursive
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

#### struct-based ####

set(struct_based_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/refcount_struct_based_check.cpp"
)

add_executable(struct_based ${struct_based_SOURCES})

target_include_directories(
  struct_based
  PRIVATE
)

target_link_libraries(struct_based
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
