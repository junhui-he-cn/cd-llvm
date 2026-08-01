//===-- CDInstrInfo.td - CD VM pseudo-instructions ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

def cdindex : Operand<i64>;

def cdtarget : Operand<OtherVT>;

class CDPseudo<dag Outs, dag Ins> : Instruction {
  let Namespace = "CD";
  let OutOperandList = Outs;
  let InOperandList = Ins;
  let AsmString = "";
  let Pattern = [];
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// CD_CONSTANT -> CDOpcode::Constant.
def CD_CONSTANT : CDPseudo<(outs CDValue:$dst), (ins cdindex:$constant)>;

// CD_MAKE_FUNCTION -> CDOpcode::MakeFunction.
def CD_MAKE_FUNCTION : CDPseudo<(outs CDValue:$dst), (ins cdindex:$function)>;

// CD_ARRAY -> CDOpcode::Array.
def CD_ARRAY : CDPseudo<(outs CDValue:$dst), (ins variable_ops)>;

// CD_INDEX -> CDOpcode::Index.
def CD_INDEX : CDPseudo<(outs CDValue:$dst),
                        (ins CDValue:$collection, CDValue:$index)>;

// CD_LEN -> CDOpcode::Len.
def CD_LEN : CDPseudo<(outs CDValue:$dst), (ins CDValue:$value)>;

// CD_ASSERT_ARRAY -> CDOpcode::AssertArray.
def CD_ASSERT_ARRAY : CDPseudo<(outs CDValue:$dst), (ins CDValue:$value)>;

// CD_MOVE -> CDOpcode::Move.
def CD_MOVE : CDPseudo<(outs CDValue:$dst), (ins CDValue:$source)>;

// CD_LOAD_VAR -> CDOpcode::LoadVar.
def CD_LOAD_VAR : CDPseudo<(outs CDValue:$dst), (ins cdindex:$name)>;

// CD_STORE_VAR -> CDOpcode::StoreVar.
def CD_STORE_VAR : CDPseudo<(outs), (ins cdindex:$name, CDValue:$source)>;

// CD_CALL -> CDOpcode::Call.
def CD_CALL : CDPseudo<(outs CDValue:$dst),
                       (ins CDValue:$callee, variable_ops)>;

// CD_PRINT -> CDOpcode::Print.
def CD_PRINT : CDPseudo<(outs), (ins CDValue:$value)>;

// CD_RETURN -> CDOpcode::Return.
def CD_RETURN : CDPseudo<(outs), (ins CDValue:$value)>;

// CD_NEGATE -> CDOpcode::Negate.
def CD_NEGATE : CDPseudo<(outs CDValue:$dst), (ins CDValue:$source)>;

// CD_NOT -> CDOpcode::Not.
def CD_NOT : CDPseudo<(outs CDValue:$dst), (ins CDValue:$source)>;

// CD_ADD -> CDOpcode::Add.
def CD_ADD : CDPseudo<(outs CDValue:$dst),
                      (ins CDValue:$left, CDValue:$right)>;

// CD_SUBTRACT -> CDOpcode::Subtract.
def CD_SUBTRACT : CDPseudo<(outs CDValue:$dst),
                           (ins CDValue:$left, CDValue:$right)>;

// CD_MULTIPLY -> CDOpcode::Multiply.
def CD_MULTIPLY : CDPseudo<(outs CDValue:$dst),
                           (ins CDValue:$left, CDValue:$right)>;

// CD_DIVIDE -> CDOpcode::Divide.
def CD_DIVIDE : CDPseudo<(outs CDValue:$dst),
                         (ins CDValue:$left, CDValue:$right)>;

// CD_EQUAL -> CDOpcode::Equal.
def CD_EQUAL : CDPseudo<(outs CDValue:$dst),
                        (ins CDValue:$left, CDValue:$right)>;

// CD_NOT_EQUAL -> CDOpcode::NotEqual.
def CD_NOT_EQUAL : CDPseudo<(outs CDValue:$dst),
                            (ins CDValue:$left, CDValue:$right)>;

// CD_GREATER -> CDOpcode::Greater.
def CD_GREATER : CDPseudo<(outs CDValue:$dst),
                          (ins CDValue:$left, CDValue:$right)>;

// CD_GREATER_EQUAL -> CDOpcode::GreaterEqual.
def CD_GREATER_EQUAL : CDPseudo<(outs CDValue:$dst),
                                (ins CDValue:$left, CDValue:$right)>;

// CD_LESS -> CDOpcode::Less.
def CD_LESS : CDPseudo<(outs CDValue:$dst),
                       (ins CDValue:$left, CDValue:$right)>;

// CD_LESS_EQUAL -> CDOpcode::LessEqual.
def CD_LESS_EQUAL : CDPseudo<(outs CDValue:$dst),
                             (ins CDValue:$left, CDValue:$right)>;

// CD_JUMP -> CDOpcode::Jump.
def CD_JUMP : CDPseudo<(outs), (ins cdtarget:$target)>;

// CD_JUMP_IF_FALSE -> CDOpcode::JumpIfFalse.
def CD_JUMP_IF_FALSE : CDPseudo<(outs),
                                (ins CDValue:$condition, cdtarget:$target)>;

// CD_SELECT expands to jump_if_false/move/jump/move in the artifact bridge.
// It is a machine lowering pseudo, not a new cdbc 0.1 wire opcode.
def CD_SELECT : CDPseudo<(outs CDValue:$dst),
                         (ins CDValue:$condition, CDValue:$if_true,
                               CDValue:$if_false)>;
