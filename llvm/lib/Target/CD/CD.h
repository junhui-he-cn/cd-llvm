//===-- CD.h - Top-level CD target definitions ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CD_H
#define LLVM_LIB_TARGET_CD_CD_H

// Keep the generated enum names in one header so the CodeGen and MC halves of
// the target use the same TableGen contract.
#define GET_REGINFO_ENUM
#include "CDGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "CDGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "CDGenSubtargetInfo.inc"

#endif // LLVM_LIB_TARGET_CD_CD_H
