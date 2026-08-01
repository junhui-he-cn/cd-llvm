//===-- CDRegisterInfo.td - CD VM value registers ------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

class CDReg<bits<8> Num, string Name> : Register<Name> {
  let Namespace = "CD";
  let HWEncoding{7-0} = Num;
}

foreach I = 0-31 in
  def R#I : CDReg<I, "r"#I>;

// CDValue is a carrier class for opaque VM values.  The first machine path
// preserves VM register identities until the artifact bridge assigns bytecode
// register numbers; its future TargetRegisterInfo will reserve these
// descriptor registers before any host register allocation is considered.
def CDValue : RegisterClass<"CD", [i64], 64,
                             (sequence "R%u", 0, 31)> {
}
