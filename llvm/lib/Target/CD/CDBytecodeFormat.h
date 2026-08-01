//===-- CDBytecodeFormat.h - CD bytecode artifact model --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CDBYTECODEFORMAT_H
#define LLVM_LIB_TARGET_CD_CDBYTECODEFORMAT_H

#include <limits>
#include <string>
#include <vector>

#include "llvm/ADT/StringRef.h"

namespace llvm {
class raw_ostream;

namespace cd {

inline constexpr unsigned InvalidIndex = std::numeric_limits<unsigned>::max();

struct CDConstant {
  enum Kind { Nil, Number, Bool, String };

  Kind kind = Nil;
  std::string text;

  static CDConstant nil();
  static CDConstant number(double value);
  static CDConstant boolean(bool value);
  static CDConstant string(StringRef value);
};

enum class CDOpcode {
  Constant,
  MakeFunction,
  Array,
  Index,
  AssignIndex,
  Len,
  AssertArray,
  Move,
  LoadVar,
  StoreVar,
  Call,
  Print,
  Return,
  Negate,
  Not,
  Add,
  Subtract,
  Multiply,
  Divide,
  Equal,
  NotEqual,
  Greater,
  GreaterEqual,
  Less,
  LessEqual,
  Jump,
  JumpIfFalse,
  JumpIfTrue,
};

struct CDInstruction {
  CDOpcode opcode = CDOpcode::Return;
  unsigned result = InvalidIndex;
  unsigned reference = InvalidIndex;
  unsigned callee = InvalidIndex;
  std::vector<unsigned> operands;
  unsigned target = InvalidIndex;

  static CDInstruction constant(unsigned destination, unsigned constant);
  static CDInstruction makeFunction(unsigned destination, unsigned function);
  static CDInstruction array(unsigned destination,
                             std::vector<unsigned> elements);
  static CDInstruction index(unsigned destination, unsigned collection,
                             unsigned index);
  static CDInstruction assignIndex(unsigned destination, unsigned collection,
                                   unsigned index, unsigned value);
  static CDInstruction len(unsigned destination, unsigned value);
  static CDInstruction assertArray(unsigned destination, unsigned value);
  static CDInstruction move(unsigned destination, unsigned source);
  static CDInstruction unary(CDOpcode opcode, unsigned destination,
                             unsigned source);
  static CDInstruction binary(CDOpcode opcode, unsigned destination,
                              unsigned left, unsigned right);
  static CDInstruction loadVar(unsigned destination, unsigned name);
  static CDInstruction storeVar(unsigned name, unsigned value);
  static CDInstruction call(unsigned destination, unsigned callee,
                            std::vector<unsigned> arguments);
  static CDInstruction print(unsigned value);
  static CDInstruction returnValue(unsigned value);
  static CDInstruction jump(unsigned target);
  static CDInstruction jumpIfFalse(unsigned condition, unsigned target);
};

struct CDBody {
  unsigned registers = 0;
  std::vector<std::string> parameterNames;
  std::vector<CDInstruction> instructions;
};

struct CDFunction {
  std::string name;
  unsigned arity = 0;
  CDBody body;
};

struct CDArtifact {
  std::vector<CDConstant> constants;
  std::vector<std::string> names;
  CDBody main;
  std::vector<CDFunction> functions;
};

/// Validate all references and structural invariants before serialization.
/// The returned error is suitable for inclusion in a target diagnostic.
bool validateArtifact(const CDArtifact &artifact, std::string &error);

/// Serialize a validated artifact using the canonical cdbc 0.1 spelling.
/// Invalid artifacts are rejected with a fatal CD-target diagnostic.
void serializeArtifact(const CDArtifact &artifact, raw_ostream &OS);

const char *opcodeName(CDOpcode opcode);

} // namespace cd
} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDBYTECODEFORMAT_H
