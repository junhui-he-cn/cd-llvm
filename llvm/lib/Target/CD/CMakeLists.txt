add_llvm_component_group(CD)

add_llvm_target(CDCodeGen
  CDBytecodeFormat.cpp
  CDBytecodeEmitter.cpp
  CDTargetMachine.cpp

  LINK_COMPONENTS
  CodeGen
  Core
  MC
  Support
  Target
  TargetParser
  CDDesc
  CDInfo

  ADD_TO_COMPONENT
  CD
  )

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
