//===-- CDCallingConv.td - CD VM calling convention --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// These assignments describe the deterministic future MachineInstr bridge.
// They do not model a host ABI or authorize native register allocation.
def RetCC_CD : CallingConv<[
  CCIfType<[i64], CCAssignToReg<[R0]>>
]>;

def CC_CD : CallingConv<[
  CCIfType<[i64], CCAssignToReg<[R0, R1, R2, R3, R4, R5, R6, R7]>>
]>;
