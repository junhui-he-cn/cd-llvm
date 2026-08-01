add_llvm_component_group(CD)

set(LLVM_TARGET_DEFINITIONS CD.td)

tablegen(LLVM CDGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM CDGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM CDGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(CDCommonTableGen)

add_llvm_target(CDCodeGen
  CDBytecodeFormat.cpp
  CDBytecodeEmitter.cpp
  CDMachineBytecodeEmitter.cpp
  CDInstrInfo.cpp
  CDRegisterInfo.cpp
  CDISelLowering.cpp
  CDSubtarget.cpp
  CDTargetMachine.cpp

  DEPENDS
  CDCommonTableGen

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
