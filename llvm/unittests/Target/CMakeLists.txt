foreach(t ${LLVM_TARGETS_TO_BUILD})
  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${t})
    add_subdirectory(${t})
  endif()
endforeach()

if(TARGET LLVMCDCodeGen AND NOT TARGET CDTests)
  add_subdirectory(CD)
endif()

set(LLVM_LINK_COMPONENTS Core Target AllTargetsCodeGens AllTargetsDescs AllTargetsInfos)

add_llvm_unittest(TargetMachineCTests
  TargetMachineOptionsTest.cpp
)
