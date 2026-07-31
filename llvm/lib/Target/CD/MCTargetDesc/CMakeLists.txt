add_llvm_component_library(LLVMCDDesc
  CDMCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  Support
  TargetParser
  CDInfo

  ADD_TO_COMPONENT
  CD
  )
