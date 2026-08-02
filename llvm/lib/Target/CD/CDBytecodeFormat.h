//===-- CDBytecodeFormat.h - CD bytecode artifact model --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CDBYTECODEFORMAT_H
#define LLVM_LIB_TARGET_CD_CDBYTECODEFORMAT_H

#include <cstdint>
#include <limits>
#include <optional>
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
  Map,
  Struct,
  Variant,
  VariantTag,
  VariantField,
  Field,
  AssignField,
  Index,
  AssignIndex,
  Len,
  AssertArray,
  Move,
  LoadVar,
  StoreVar,
  Call,
  NativeCall,
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
  unsigned secondaryReference = InvalidIndex;
  unsigned payloadIndex = InvalidIndex;
  unsigned callee = InvalidIndex;
  std::vector<unsigned> operands;
  unsigned target = InvalidIndex;

  static CDInstruction constant(unsigned destination, unsigned constant);
  static CDInstruction makeFunction(unsigned destination, unsigned function);
  static CDInstruction array(unsigned destination,
                             std::vector<unsigned> elements);
  static CDInstruction map(unsigned destination,
                           std::vector<unsigned> keyValueOperands);
  static CDInstruction structValue(unsigned destination, unsigned typeName,
                                   std::vector<unsigned> fieldNameValueOperands);
  static CDInstruction variant(unsigned destination, unsigned enumName,
                               unsigned variantName,
                               std::vector<unsigned> payload);
  static CDInstruction variantTag(unsigned destination, unsigned value,
                                  unsigned enumName, unsigned variantName);
  static CDInstruction variantField(unsigned destination, unsigned value,
                                    unsigned index);
  static CDInstruction field(unsigned destination, unsigned object,
                             unsigned name);
  static CDInstruction assignField(unsigned destination, unsigned object,
                                   unsigned name, unsigned value);
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
  static CDInstruction nativeCall(unsigned destination, unsigned name,
                                  std::vector<unsigned> arguments);
  static CDInstruction print(unsigned value);
  static CDInstruction returnValue(unsigned value);
  static CDInstruction jump(unsigned target);
  static CDInstruction jumpIfFalse(unsigned condition, unsigned target);
};

struct CDDebugSource {
  std::optional<std::string> module;
  std::string path;
  std::string text;
};

struct CDDebugRange {
  unsigned source = InvalidIndex;
  uint64_t start = 0;
  uint64_t end = 0;
};

struct CDDebugLocation {
  unsigned source = InvalidIndex;
  unsigned line = 0;
  unsigned column = 0;
  std::optional<CDDebugRange> range;
};

struct CDBody {
  unsigned registers = 0;
  std::vector<std::string> parameterNames;
  std::vector<CDInstruction> instructions;
  std::vector<std::optional<CDDebugLocation>> locations;
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
  std::vector<CDDebugSource> debugSources;
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
