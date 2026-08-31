//===-- CDBytecodeFormat.cpp - CD bytecode artifact model ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDBytecodeFormat.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include <cmath>
#include <algorithm>
#include <map>
#include <set>
#include <utility>

using namespace llvm;

namespace {

using namespace llvm::cd;

static std::string numberText(double Number) {
  std::string Text;
  raw_string_ostream Stream(Text);
  Stream << format("%.17g", Number);
  Stream.flush();
  return Text;
}

static std::string escapeString(StringRef Value) {
  std::string Result;
  Result.reserve(Value.size() + 2);
  for (char Character : Value) {
    switch (Character) {
    case '\\':
      Result += "\\\\";
      break;
    case '"':
      Result += "\\\"";
      break;
    case '\n':
      Result += "\\n";
      break;
    case '\r':
      Result += "\\r";
      break;
    case '\t':
      Result += "\\t";
      break;
    default:
      Result += Character;
      break;
    }
  }
  return Result;
}

static void writeQuoted(raw_ostream &OS, StringRef Value) {
  OS << '"' << escapeString(Value) << '"';
}

static std::string registerName(unsigned Register) {
  return "r" + std::to_string(Register);
}

static std::string constantName(unsigned Constant) {
  return "c" + std::to_string(Constant);
}

static std::string nameName(unsigned Name) {
  return "n" + std::to_string(Name);
}

static std::string functionName(unsigned Function) {
  return "f" + std::to_string(Function);
}

static bool fail(std::string &Error, Twine Message) {
  Error = Message.str();
  return false;
}

static bool isSupportedNativeName(StringRef Name) {
  return Name == "floor" || Name == "ceil" || Name == "sqrt" ||
         Name == "str" || Name == "typeOf" || Name == "hash" ||
         Name == "contains" || Name == "slice" || Name == "copy" ||
         Name == "concat" || Name == "push" || Name == "pop" ||
         Name == "remove" ||
         Name == "clear" ||
         Name == "merge" || Name == "keys" ||
         Name == "values" ||
         Name == "range" ||
         Name == "substr" ||
         Name == "charAt" ||
         Name == "map" || Name == "filter" || Name == "flatMap" ||
         Name == "any" ||
         Name == "all" || Name == "count" || Name == "find" ||
         Name == "findIndex" || Name == "reduce" || Name == "print";
}

static bool validateRegister(const CDBody &Body, unsigned Register,
                             StringRef BodyName, unsigned InstructionIndex,
                             StringRef Role, std::string &Error) {
  if (Register == InvalidIndex || Register >= Body.registers)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has " + Role +
                         " register outside the body register range");
  return true;
}

static bool validateReference(unsigned Reference, unsigned Count,
                              StringRef Kind, StringRef BodyName,
                              unsigned InstructionIndex, std::string &Error) {
  if (Reference == InvalidIndex || Reference >= Count)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has an out-of-range " +
                         Kind + " reference");
  return true;
}

static bool validateResult(const CDInstruction &Instruction,
                           const CDBody &Body, StringRef BodyName,
                           unsigned InstructionIndex, std::string &Error) {
  if (Instruction.result == InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " is missing a result register");
  return validateRegister(Body, Instruction.result, BodyName, InstructionIndex,
                          "result", Error);
}

static bool validateNoResult(const CDInstruction &Instruction,
                             StringRef BodyName, unsigned InstructionIndex,
                             std::string &Error) {
  if (Instruction.result != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has an unexpected result register");
  return true;
}

static bool validateOperandCount(const CDInstruction &Instruction,
                                 size_t Count, StringRef BodyName,
                                 unsigned InstructionIndex,
                                 std::string &Error) {
  if (Instruction.operands.size() != Count)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has the wrong operand count");
  return true;
}

static bool validateUnusedFieldsWithVariantFields(
    const CDInstruction &Instruction, bool HasReference, bool HasCallee,
    bool HasTarget, bool HasSecondaryReference, bool HasIndex,
    StringRef BodyName, unsigned InstructionIndex, std::string &Error) {
  if (!HasReference && Instruction.reference != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) +
                         " has an unexpected table reference");
  if (!HasCallee && Instruction.callee != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) +
                         " has an unexpected callee register");
  if (!HasTarget && Instruction.target != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) +
                         " has an unexpected jump target");
  if (!HasSecondaryReference && Instruction.secondaryReference != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) +
                         " has an unexpected secondary table reference");
  if (!HasIndex && Instruction.payloadIndex != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has an unexpected index");
  return true;
}

static bool validateUnusedFields(const CDInstruction &Instruction,
                                 bool HasReference, bool HasCallee,
                                 bool HasTarget, StringRef BodyName,
                                 unsigned InstructionIndex,
                                 std::string &Error) {
  return validateUnusedFieldsWithVariantFields(
      Instruction, HasReference, HasCallee, HasTarget,
      /*HasSecondaryReference=*/false, /*HasIndex=*/false, BodyName,
      InstructionIndex, Error);
}

static bool validateOperands(const CDInstruction &Instruction,
                             const CDBody &Body, StringRef BodyName,
                             unsigned InstructionIndex, std::string &Error) {
  for (unsigned Register : Instruction.operands)
    if (!validateRegister(Body, Register, BodyName, InstructionIndex, "operand",
                          Error))
      return false;
  return true;
}

static bool validateInstruction(const CDInstruction &Instruction,
                                const CDArtifact &Artifact,
                                const CDBody &Body, StringRef BodyName,
                                unsigned InstructionIndex, std::string &Error) {
  switch (Instruction.opcode) {
  case CDOpcode::Constant:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.constants.size(),
                           "constant", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 0, BodyName, InstructionIndex,
                              Error))
      return false;
    return true;
  case CDOpcode::MakeFunction:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.functions.size(),
                           "function", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 0, BodyName, InstructionIndex,
                              Error))
      return false;
    return true;
  case CDOpcode::Array:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Map:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    if (Instruction.operands.size() % 2 != 0)
      return fail(Error, Twine(BodyName) + " instruction " +
                           Twine(InstructionIndex) +
                           " map requires key/value operand pairs");
    if (!validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Struct:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    if (Instruction.operands.size() % 2 != 0)
      return fail(Error, Twine(BodyName) + " instruction " +
                           Twine(InstructionIndex) +
                           " struct requires field name/value operand pairs");
    if (Instruction.reference != InvalidIndex &&
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "struct type name", BodyName, InstructionIndex,
                           Error))
      return false;
    for (unsigned Index = 0; Index < Instruction.operands.size(); Index += 2) {
      if (!validateReference(Instruction.operands[Index], Artifact.names.size(),
                             "struct field name", BodyName, InstructionIndex,
                             Error) ||
          !validateRegister(Body, Instruction.operands[Index + 1], BodyName,
                            InstructionIndex, "struct field value", Error))
        return false;
    }
    return true;
  case CDOpcode::Variant:
    if (!validateUnusedFieldsWithVariantFields(
            Instruction, true, false, false,
            /*HasSecondaryReference=*/true, /*HasIndex=*/false, BodyName,
            InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "variant enum name", BodyName, InstructionIndex,
                           Error) ||
        !validateReference(Instruction.secondaryReference,
                           Artifact.names.size(), "variant name", BodyName,
                           InstructionIndex, Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex,
                          Error))
      return false;
    return true;
  case CDOpcode::VariantTag:
    if (!validateUnusedFieldsWithVariantFields(
            Instruction, true, false, false,
            /*HasSecondaryReference=*/true, /*HasIndex=*/false, BodyName,
            InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "variant enum name", BodyName, InstructionIndex,
                           Error) ||
        !validateReference(Instruction.secondaryReference,
                           Artifact.names.size(), "variant name", BodyName,
                           InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex,
                          Error))
      return false;
    return true;
  case CDOpcode::VariantField:
    if (!validateUnusedFieldsWithVariantFields(
            Instruction, false, false, false,
            /*HasSecondaryReference=*/false, /*HasIndex=*/true, BodyName,
            InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex,
                          Error))
      return false;
    return true;
  case CDOpcode::Field:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "field name", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::AssignField:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "field name", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 2, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Index:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 2, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::AssignIndex:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 3, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Len:
  case CDOpcode::AssertArray:
  case CDOpcode::Move:
  case CDOpcode::Negate:
  case CDOpcode::Not:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::LoadVar:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "name", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 0, BodyName, InstructionIndex,
                              Error))
      return false;
    return true;
  case CDOpcode::StoreVar:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateNoResult(Instruction, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "name", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Call:
    if (!validateUnusedFields(Instruction, false, true, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateRegister(Body, Instruction.callee, BodyName, InstructionIndex,
                          "callee", Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::NativeCall:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "native name", BodyName, InstructionIndex, Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex,
                          Error))
      return false;
    if (!isSupportedNativeName(Artifact.names[Instruction.reference]))
      return fail(Error, Twine(BodyName) + " instruction " +
                           Twine(InstructionIndex) +
                           " has an unsupported native name");
    return true;
  case CDOpcode::Print:
  case CDOpcode::Return:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateNoResult(Instruction, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Add:
  case CDOpcode::Subtract:
  case CDOpcode::Multiply:
  case CDOpcode::Divide:
  case CDOpcode::Equal:
  case CDOpcode::NotEqual:
  case CDOpcode::Greater:
  case CDOpcode::GreaterEqual:
  case CDOpcode::Less:
  case CDOpcode::LessEqual:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 2, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Jump:
    if (!validateUnusedFields(Instruction, false, false, true, BodyName,
                              InstructionIndex, Error) ||
        !validateNoResult(Instruction, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 0, BodyName, InstructionIndex,
                              Error))
      return false;
    break;
  case CDOpcode::JumpIfFalse:
  case CDOpcode::JumpIfTrue:
    if (!validateUnusedFields(Instruction, false, false, true, BodyName,
                              InstructionIndex, Error) ||
        !validateNoResult(Instruction, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    break;
  default:
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has an unknown opcode");
  }

  if (Instruction.target > Body.instructions.size())
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has an out-of-range jump target");
  return true;
}

static bool validateBody(const CDArtifact &Artifact, const CDBody &Body,
                         StringRef BodyName, std::string &Error) {
  if (!Body.locations.empty() &&
      Body.locations.size() != Body.instructions.size())
    return fail(Error, Twine(BodyName) + " has " +
                         Twine(Body.locations.size()) +
                         " debug locations for " +
                         Twine(Body.instructions.size()) + " instructions");

  for (unsigned Index = 0; Index < Body.locations.size(); ++Index) {
    const std::optional<CDDebugLocation> &Location = Body.locations[Index];
    if (!Location)
      continue;
    if (Location->source >= Artifact.debugSources.size())
      return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                           " has a debug location source outside the source table");
    if (Location->line == 0 || Location->column == 0)
      return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                           " has a non-positive debug location");
    if (Location->range) {
      const CDDebugRange &Range = *Location->range;
      if (Range.source >= Artifact.debugSources.size())
        return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                             " has a debug range source outside the source table");
      if (Range.source != Location->source)
        return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                             " debug range source does not match its location");
      if (Range.start > Range.end)
        return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                             " has a reversed debug range");
      if (Range.end > Artifact.debugSources[Range.source].text.size())
        return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                             " debug range exceeds source length");
    }
  }

  for (unsigned Index = 0; Index < Body.instructions.size(); ++Index)
    if (!validateInstruction(Body.instructions[Index], Artifact, Body, BodyName,
                             Index, Error))
      return false;
  return true;
}

static bool validateConstants(const CDArtifact &Artifact, std::string &Error) {
  for (unsigned Index = 0; Index < Artifact.constants.size(); ++Index) {
    const CDConstant &Constant = Artifact.constants[Index];
    switch (Constant.kind) {
    case CDConstant::Nil:
      if (!Constant.text.empty())
        return fail(Error, Twine("constant c") + Twine(Index) +
                             " has unexpected nil payload");
      break;
    case CDConstant::Bool:
      if (Constant.text != "true" && Constant.text != "false")
        return fail(Error, Twine("constant c") + Twine(Index) +
                             " has an invalid boolean payload");
      break;
    case CDConstant::Number: {
      double Number = 0.0;
      if (StringRef(Constant.text).getAsDouble(Number) ||
          !std::isfinite(Number))
        return fail(Error, Twine("constant c") + Twine(Index) +
                             " is not a finite number");
      break;
    }
    case CDConstant::String:
      if (!json::isUTF8(Constant.text))
        return fail(Error, Twine("constant c") + Twine(Index) +
                             " is not valid UTF-8");
      break;
    default:
      return fail(Error, Twine("constant c") + Twine(Index) +
                           " has an unknown kind");
    }
  }
  return true;
}

static bool validateDebugSources(const CDArtifact &Artifact,
                                 std::string &Error) {
  std::set<std::string> Identities;
  for (unsigned Index = 0; Index < Artifact.debugSources.size(); ++Index) {
    const CDDebugSource &Source = Artifact.debugSources[Index];
    if (Source.module && Source.module->empty())
      return fail(Error, Twine("debug source s") + Twine(Index) +
                           " has an empty module identity");
    if (Source.path.empty())
      return fail(Error, Twine("debug source s") + Twine(Index) +
                           " has an empty path");
    if (!json::isUTF8(Source.path) || !json::isUTF8(Source.text) ||
        (Source.module && !json::isUTF8(*Source.module)))
      return fail(Error, Twine("debug source s") + Twine(Index) +
                           " is not valid UTF-8");

    std::string Identity = Source.module.value_or("");
    Identity.push_back('\0');
    Identity += Source.path;
    if (!Identities.insert(Identity).second)
      return fail(Error, Twine("debug source s") + Twine(Index) +
                           " duplicates source identity");
  }
  return true;
}

static bool validateModuleMetadata(const CDArtifact &Artifact,
                                   std::string &Error) {
  if (!Artifact.module)
    return true;

  const CDModuleMetadata &Module = *Artifact.module;
  if (Module.identity.empty())
    return fail(Error, "module identity must not be empty");
  if (Module.path.empty())
    return fail(Error, "module path must not be empty");
  if (Module.canonicalPath.empty())
    return fail(Error, "module canonical path must not be empty");
  if (!json::isUTF8(Module.identity) || !json::isUTF8(Module.path) ||
      !json::isUTF8(Module.canonicalPath))
    return fail(Error, "module metadata contains invalid UTF-8");
  if (Module.isEntry != Module.entryOrder.has_value())
    return fail(Error,
                "module entry_order must be present exactly for entry modules");

  uint64_t PreviousOffset = 0;
  for (unsigned Index = 0; Index < Module.dependencies.size(); ++Index) {
    const CDModuleDependency &Dependency = Module.dependencies[Index];
    if (Dependency.identity.empty() || Dependency.requestedPath.empty())
      return fail(Error, Twine("module dependency d") + Twine(Index) +
                           " has an empty identity or path");
    if (!json::isUTF8(Dependency.identity) ||
        !json::isUTF8(Dependency.requestedPath))
      return fail(Error, Twine("module dependency d") + Twine(Index) +
                           " contains invalid UTF-8");
    if (Dependency.instructionOffset > Artifact.main.instructions.size())
      return fail(Error, Twine("module dependency d") + Twine(Index) +
                           " instruction offset out of range");
    if (Index != 0 && Dependency.instructionOffset < PreviousOffset)
      return fail(Error, "module dependency offsets must be nondecreasing");
    PreviousOffset = Dependency.instructionOffset;
  }
  return true;
}

} // namespace

namespace llvm::cd {

CDConstant CDConstant::nil() { return {CDConstant::Nil, {}}; }

CDConstant CDConstant::number(double Value) {
  return {CDConstant::Number, numberText(Value)};
}

CDConstant CDConstant::boolean(bool Value) {
  return {CDConstant::Bool, Value ? "true" : "false"};
}

CDConstant CDConstant::string(StringRef Value) {
  return {CDConstant::String, Value.str()};
}

CDInstruction CDInstruction::constant(unsigned Destination, unsigned Constant) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Constant;
  Instruction.result = Destination;
  Instruction.reference = Constant;
  return Instruction;
}

CDInstruction CDInstruction::makeFunction(unsigned Destination,
                                           unsigned Function) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::MakeFunction;
  Instruction.result = Destination;
  Instruction.reference = Function;
  return Instruction;
}

CDInstruction CDInstruction::array(unsigned Destination,
                                    std::vector<unsigned> Elements) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Array;
  Instruction.result = Destination;
  Instruction.operands = std::move(Elements);
  return Instruction;
}

CDInstruction CDInstruction::map(unsigned Destination,
                                  std::vector<unsigned> KeyValueOperands) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Map;
  Instruction.result = Destination;
  Instruction.operands = std::move(KeyValueOperands);
  return Instruction;
}

CDInstruction CDInstruction::structValue(
    unsigned Destination, unsigned TypeName,
    std::vector<unsigned> FieldNameValueOperands) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Struct;
  Instruction.result = Destination;
  Instruction.reference = TypeName;
  Instruction.operands = std::move(FieldNameValueOperands);
  return Instruction;
}

CDInstruction CDInstruction::variant(unsigned Destination, unsigned EnumName,
                                     unsigned VariantName,
                                     std::vector<unsigned> Payload) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Variant;
  Instruction.result = Destination;
  Instruction.reference = EnumName;
  Instruction.secondaryReference = VariantName;
  Instruction.operands = std::move(Payload);
  return Instruction;
}

CDInstruction CDInstruction::variantTag(unsigned Destination, unsigned Value,
                                         unsigned EnumName,
                                         unsigned VariantName) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::VariantTag;
  Instruction.result = Destination;
  Instruction.reference = EnumName;
  Instruction.secondaryReference = VariantName;
  Instruction.operands = {Value};
  return Instruction;
}

CDInstruction CDInstruction::variantField(unsigned Destination, unsigned Value,
                                           unsigned Index) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::VariantField;
  Instruction.result = Destination;
  Instruction.payloadIndex = Index;
  Instruction.operands = {Value};
  return Instruction;
}

CDInstruction CDInstruction::field(unsigned Destination, unsigned Object,
                                   unsigned Name) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Field;
  Instruction.result = Destination;
  Instruction.reference = Name;
  Instruction.operands = {Object};
  return Instruction;
}

CDInstruction CDInstruction::assignField(unsigned Destination, unsigned Object,
                                         unsigned Name, unsigned Value) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::AssignField;
  Instruction.result = Destination;
  Instruction.reference = Name;
  Instruction.operands = {Object, Value};
  return Instruction;
}

CDInstruction CDInstruction::index(unsigned Destination, unsigned Collection,
                                    unsigned Index) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Index;
  Instruction.result = Destination;
  Instruction.operands = {Collection, Index};
  return Instruction;
}

CDInstruction CDInstruction::assignIndex(unsigned Destination,
                                         unsigned Collection, unsigned Index,
                                         unsigned Value) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::AssignIndex;
  Instruction.result = Destination;
  Instruction.operands = {Collection, Index, Value};
  return Instruction;
}

CDInstruction CDInstruction::len(unsigned Destination, unsigned Value) {
  return unary(CDOpcode::Len, Destination, Value);
}

CDInstruction CDInstruction::assertArray(unsigned Destination, unsigned Value) {
  return unary(CDOpcode::AssertArray, Destination, Value);
}

CDInstruction CDInstruction::move(unsigned Destination, unsigned Source) {
  return unary(CDOpcode::Move, Destination, Source);
}

CDInstruction CDInstruction::unary(CDOpcode Opcode, unsigned Destination,
                                   unsigned Source) {
  CDInstruction Instruction;
  Instruction.opcode = Opcode;
  Instruction.result = Destination;
  Instruction.operands.push_back(Source);
  return Instruction;
}

CDInstruction CDInstruction::binary(CDOpcode Opcode, unsigned Destination,
                                    unsigned Left, unsigned Right) {
  CDInstruction Instruction;
  Instruction.opcode = Opcode;
  Instruction.result = Destination;
  Instruction.operands = {Left, Right};
  return Instruction;
}

CDInstruction CDInstruction::loadVar(unsigned Destination, unsigned Name) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::LoadVar;
  Instruction.result = Destination;
  Instruction.reference = Name;
  return Instruction;
}

CDInstruction CDInstruction::storeVar(unsigned Name, unsigned Value) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::StoreVar;
  Instruction.reference = Name;
  Instruction.operands.push_back(Value);
  return Instruction;
}

CDInstruction CDInstruction::call(unsigned Destination, unsigned Callee,
                                  std::vector<unsigned> Arguments) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Call;
  Instruction.result = Destination;
  Instruction.callee = Callee;
  Instruction.operands = std::move(Arguments);
  return Instruction;
}

CDInstruction CDInstruction::nativeCall(unsigned Destination, unsigned Name,
                                         std::vector<unsigned> Arguments) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::NativeCall;
  Instruction.result = Destination;
  Instruction.reference = Name;
  Instruction.operands = std::move(Arguments);
  return Instruction;
}

CDInstruction CDInstruction::print(unsigned Value) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Print;
  Instruction.operands.push_back(Value);
  return Instruction;
}

CDInstruction CDInstruction::returnValue(unsigned Value) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Return;
  Instruction.operands.push_back(Value);
  return Instruction;
}

CDInstruction CDInstruction::jump(unsigned Target) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Jump;
  Instruction.target = Target;
  return Instruction;
}

CDInstruction CDInstruction::jumpIfFalse(unsigned Condition, unsigned Target) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::JumpIfFalse;
  Instruction.operands.push_back(Condition);
  Instruction.target = Target;
  return Instruction;
}

const char *opcodeName(CDOpcode Opcode) {
  switch (Opcode) {
  case CDOpcode::Constant:
    return "constant";
  case CDOpcode::MakeFunction:
    return "make_function";
  case CDOpcode::Array:
    return "array";
  case CDOpcode::Map:
    return "map";
  case CDOpcode::Struct:
    return "struct";
  case CDOpcode::Variant:
    return "variant";
  case CDOpcode::VariantTag:
    return "variant_tag";
  case CDOpcode::VariantField:
    return "variant_field";
  case CDOpcode::Field:
    return "field";
  case CDOpcode::AssignField:
    return "assign_field";
  case CDOpcode::Index:
    return "index";
  case CDOpcode::AssignIndex:
    return "assign_index";
  case CDOpcode::Len:
    return "len";
  case CDOpcode::AssertArray:
    return "assert_array";
  case CDOpcode::Move:
    return "move";
  case CDOpcode::LoadVar:
    return "load_var";
  case CDOpcode::StoreVar:
    return "store_var";
  case CDOpcode::Call:
    return "call";
  case CDOpcode::NativeCall:
    return "native_call";
  case CDOpcode::Print:
    return "print";
  case CDOpcode::Return:
    return "return";
  case CDOpcode::Negate:
    return "negate";
  case CDOpcode::Not:
    return "not";
  case CDOpcode::Add:
    return "add";
  case CDOpcode::Subtract:
    return "subtract";
  case CDOpcode::Multiply:
    return "multiply";
  case CDOpcode::Divide:
    return "divide";
  case CDOpcode::Equal:
    return "equal";
  case CDOpcode::NotEqual:
    return "not_equal";
  case CDOpcode::Greater:
    return "greater";
  case CDOpcode::GreaterEqual:
    return "greater_equal";
  case CDOpcode::Less:
    return "less";
  case CDOpcode::LessEqual:
    return "less_equal";
  case CDOpcode::Jump:
    return "jump";
  case CDOpcode::JumpIfFalse:
    return "jump_if_false";
  case CDOpcode::JumpIfTrue:
    return "jump_if_true";
  }
  return "unknown";
}

namespace {

// The LLVM target still builds a small linear instruction model.  These
// private records are the 0.2 wire model used after both emitters have
// finished lowering.  Keeping this boundary shared is important: direct and
// machine lowering must make the same decisions about slots, tables, and
// control-flow blocks.
enum class V2Opcode {
  Constant,
  MakeFunction,
  Array,
  Map,
  MakeStruct,
  StructGet,
  StructSet,
  MakeVariant,
  IsVariant,
  VariantGet,
  Move,
  LoadLocal,
  BindLocal,
  SetLocal,
  LoadUpvalue,
  SetUpvalue,
  LoadGlobal,
  InitGlobal,
  SetGlobal,
  Call,
  CallNative,
  Index,
  AssignIndex,
  Field,
  AssignField,
  Len,
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
  BlockStart,
  Br,
  BrIf,
  Return,
  ReturnNil,
  InitModule,
};

struct V2Instruction {
  V2Opcode opcode = V2Opcode::ReturnNil;
  unsigned result = InvalidIndex;
  unsigned reference = InvalidIndex;
  unsigned secondaryReference = InvalidIndex;
  unsigned payloadIndex = InvalidIndex;
  unsigned callee = InvalidIndex;
  std::vector<unsigned> operands;
  unsigned target = InvalidIndex;
};

struct V2Body {
  unsigned registers = 0;
  unsigned localCount = 0;
  std::vector<std::string> parameterNames;
  std::vector<V2Instruction> instructions;
  std::vector<std::optional<CDDebugLocation>> locations;
};

struct V2Function {
  std::string name;
  unsigned arity = 0;
  V2Body body;
};

struct V2Variant {
  std::string name;
  unsigned payloadCount = 0;
  bool payloadCountKnown = false;
};

struct V2Type {
  bool isEnum = false;
  std::string name;
  std::vector<std::string> fieldNames;
  std::vector<V2Variant> variants;
};

struct V2NativeImport {
  std::string name;
  unsigned abi = 1;
};

struct V2Artifact {
  std::vector<unsigned> globals;
  std::vector<V2Type> types;
  std::vector<V2NativeImport> nativeImports;
  V2Body main;
  std::vector<V2Function> functions;
};

struct LegacyEntry {
  const CDInstruction *instruction = nullptr;
  unsigned oldOffset = 0;
  unsigned module = InvalidIndex;
  std::optional<CDDebugLocation> location;
};

struct LegacyCFG {
  std::vector<LegacyEntry> entries;
  std::vector<unsigned> offsetToEntry;
  std::vector<unsigned> starts;
  std::vector<unsigned> entryBlocks;
  std::vector<std::vector<unsigned>> successors;
  std::vector<std::vector<unsigned>> predecessors;
};

struct VariableLayout {
  std::map<unsigned, unsigned> slotsByName;
  std::vector<unsigned> namesBySlot;
  std::set<unsigned> parameterSlots;
};

struct VariantIdentity {
  unsigned enumName = InvalidIndex;
  unsigned variantName = InvalidIndex;

  bool operator==(const VariantIdentity &Other) const {
    return enumName == Other.enumName && variantName == Other.variantName;
  }
};

struct ProvenanceState {
  std::map<unsigned, VariantIdentity> registers;
  std::map<unsigned, VariantIdentity> variables;

  bool operator==(const ProvenanceState &Other) const {
    return registers == Other.registers && variables == Other.variables;
  }
};

static bool isLegacyBranch(CDOpcode Opcode) {
  return Opcode == CDOpcode::Jump || Opcode == CDOpcode::JumpIfFalse ||
         Opcode == CDOpcode::JumpIfTrue;
}

static bool appendV2Instruction(V2Body &Body, V2Instruction Instruction,
                                std::optional<CDDebugLocation> Location) {
  Body.instructions.push_back(std::move(Instruction));
  Body.locations.push_back(std::move(Location));
  return true;
}

static std::string v2OpcodeName(V2Opcode Opcode) {
  switch (Opcode) {
  case V2Opcode::Constant:
    return "constant";
  case V2Opcode::MakeFunction:
    return "make_function";
  case V2Opcode::Array:
    return "array";
  case V2Opcode::Map:
    return "map";
  case V2Opcode::MakeStruct:
    return "make_struct";
  case V2Opcode::StructGet:
    return "struct_get";
  case V2Opcode::StructSet:
    return "struct_set";
  case V2Opcode::MakeVariant:
    return "make_variant";
  case V2Opcode::IsVariant:
    return "is_variant";
  case V2Opcode::VariantGet:
    return "variant_get";
  case V2Opcode::Move:
    return "move";
  case V2Opcode::LoadLocal:
    return "load_local";
  case V2Opcode::BindLocal:
    return "bind_local";
  case V2Opcode::SetLocal:
    return "set_local";
  case V2Opcode::LoadUpvalue:
    return "load_upvalue";
  case V2Opcode::SetUpvalue:
    return "set_upvalue";
  case V2Opcode::LoadGlobal:
    return "load_global";
  case V2Opcode::InitGlobal:
    return "init_global";
  case V2Opcode::SetGlobal:
    return "set_global";
  case V2Opcode::Call:
    return "call";
  case V2Opcode::CallNative:
    return "call_native";
  case V2Opcode::Index:
    return "index";
  case V2Opcode::AssignIndex:
    return "assign_index";
  case V2Opcode::Field:
    return "field";
  case V2Opcode::AssignField:
    return "assign_field";
  case V2Opcode::Len:
    return "len";
  case V2Opcode::Negate:
    return "negate";
  case V2Opcode::Not:
    return "not";
  case V2Opcode::Add:
    return "add";
  case V2Opcode::Subtract:
    return "subtract";
  case V2Opcode::Multiply:
    return "multiply";
  case V2Opcode::Divide:
    return "divide";
  case V2Opcode::Equal:
    return "equal";
  case V2Opcode::NotEqual:
    return "not_equal";
  case V2Opcode::Greater:
    return "greater";
  case V2Opcode::GreaterEqual:
    return "greater_equal";
  case V2Opcode::Less:
    return "less";
  case V2Opcode::LessEqual:
    return "less_equal";
  case V2Opcode::BlockStart:
    return "block";
  case V2Opcode::Br:
    return "br";
  case V2Opcode::BrIf:
    return "br_if";
  case V2Opcode::Return:
    return "return";
  case V2Opcode::ReturnNil:
    return "return_nil";
  case V2Opcode::InitModule:
    return "init_module";
  }
  return "unknown";
}

static bool buildLegacyCFG(const CDBody &Body,
                           const CDModuleMetadata *Module,
                           LegacyCFG &CFG, std::string &Error) {
  const size_t InstructionCount = Body.instructions.size();
  CFG = {};
  CFG.offsetToEntry.reserve(InstructionCount + 1);

  unsigned DependencyIndex = 0;
  for (size_t Offset = 0; Offset <= InstructionCount; ++Offset) {
    CFG.offsetToEntry.push_back(static_cast<unsigned>(CFG.entries.size()));
    while (Module && DependencyIndex < Module->dependencies.size() &&
           Module->dependencies[DependencyIndex].instructionOffset == Offset) {
      LegacyEntry Entry;
      Entry.oldOffset = static_cast<unsigned>(Offset);
      Entry.module = DependencyIndex++;
      CFG.entries.push_back(std::move(Entry));
    }
    if (Offset == InstructionCount)
      break;

    LegacyEntry Entry;
    Entry.instruction = &Body.instructions[Offset];
    Entry.oldOffset = static_cast<unsigned>(Offset);
    if (!Body.locations.empty())
      Entry.location = Body.locations[Offset];
    CFG.entries.push_back(std::move(Entry));
  }

  if (Module && DependencyIndex != Module->dependencies.size())
    return fail(Error, "module dependency offset could not be lowered");

  std::set<unsigned> StartSet;
  StartSet.insert(CFG.offsetToEntry[0]);
  for (const LegacyEntry &Entry : CFG.entries) {
    if (!Entry.instruction)
      continue;
    const CDInstruction &Instruction = *Entry.instruction;
    if (isLegacyBranch(Instruction.opcode) ||
        Instruction.opcode == CDOpcode::Return) {
      const size_t NextOffset = static_cast<size_t>(Entry.oldOffset) + 1;
      if (NextOffset < InstructionCount ||
          CFG.offsetToEntry[NextOffset] < CFG.entries.size())
        StartSet.insert(CFG.offsetToEntry[NextOffset]);
    }
    if (isLegacyBranch(Instruction.opcode)) {
      if (Instruction.target > InstructionCount)
        return fail(Error, "legacy branch target is outside the body");
      StartSet.insert(CFG.offsetToEntry[Instruction.target]);
    }
  }

  CFG.starts.assign(StartSet.begin(), StartSet.end());
  if (CFG.starts.empty() || CFG.starts.front() != 0)
    return fail(Error, "lowered body does not begin at entry block");

  CFG.entryBlocks.assign(CFG.entries.size(), InvalidIndex);
  for (unsigned Block = 0; Block < CFG.starts.size(); ++Block) {
    const unsigned Start = CFG.starts[Block];
    const unsigned End = Block + 1 < CFG.starts.size()
                             ? CFG.starts[Block + 1]
                             : static_cast<unsigned>(CFG.entries.size());
    for (unsigned Index = Start; Index < End; ++Index)
      CFG.entryBlocks[Index] = Block;
  }

  auto blockForEntry = [&](unsigned Entry) -> unsigned {
    auto It = std::lower_bound(CFG.starts.begin(), CFG.starts.end(), Entry);
    if (It == CFG.starts.end() || *It != Entry)
      return InvalidIndex;
    return static_cast<unsigned>(It - CFG.starts.begin());
  };

  CFG.successors.resize(CFG.starts.size());
  CFG.predecessors.resize(CFG.starts.size());
  for (unsigned Block = 0; Block < CFG.starts.size(); ++Block) {
    const unsigned Start = CFG.starts[Block];
    const unsigned End = Block + 1 < CFG.starts.size()
                             ? CFG.starts[Block + 1]
                             : static_cast<unsigned>(CFG.entries.size());
    const CDInstruction *LastInstruction = nullptr;
    unsigned LastOffset = 0;
    for (unsigned Index = Start; Index < End; ++Index) {
      if (CFG.entries[Index].instruction) {
        LastInstruction = CFG.entries[Index].instruction;
        LastOffset = CFG.entries[Index].oldOffset;
      }
    }
    if (!LastInstruction)
      continue;

    auto appendSuccessor = [&](size_t Offset) -> bool {
      if (Offset > InstructionCount)
        return fail(Error, "lowered branch fallthrough is outside the body");
      const unsigned Entry = CFG.offsetToEntry[Offset];
      const unsigned Target = blockForEntry(Entry);
      if (Target == InvalidIndex)
        return fail(Error, "lowered branch target is not a block boundary");
      CFG.successors[Block].push_back(Target);
      return true;
    };

    if (LastInstruction->opcode == CDOpcode::Jump) {
      if (!appendSuccessor(LastInstruction->target))
        return false;
    } else if (LastInstruction->opcode == CDOpcode::JumpIfFalse ||
               LastInstruction->opcode == CDOpcode::JumpIfTrue) {
      if (!appendSuccessor(static_cast<size_t>(LastOffset) + 1) ||
          !appendSuccessor(LastInstruction->target))
        return false;
    }
  }

  for (unsigned Block = 0; Block < CFG.successors.size(); ++Block)
    for (unsigned Successor : CFG.successors[Block])
      CFG.predecessors[Successor].push_back(Block);
  return true;
}

static VariableLayout buildVariableLayout(const CDArtifact &Artifact,
                                          const CDBody &Body,
                                          bool IsTopLevel,
                                          std::string &Error) {
  VariableLayout Layout;
  std::map<std::string, unsigned> SlotsByText;
  if (!IsTopLevel) {
    for (unsigned Index = 0; Index < Body.parameterNames.size(); ++Index) {
      const std::string &Name = Body.parameterNames[Index];
      auto [It, Inserted] = SlotsByText.emplace(Name, Index);
      if (!Inserted)
        return fail(Error, "function parameter names are not unique"), Layout;
      Layout.namesBySlot.push_back(InvalidIndex);
      Layout.parameterSlots.insert(Index);
    }
  }

  for (const CDInstruction &Instruction : Body.instructions) {
    if (Instruction.opcode != CDOpcode::LoadVar &&
        Instruction.opcode != CDOpcode::StoreVar)
      continue;
    if (Instruction.reference >= Artifact.names.size())
      return fail(Error, "variable name reference is outside the name table"),
             Layout;
    const std::string &Name = Artifact.names[Instruction.reference];
    auto It = SlotsByText.find(Name);
    if (It == SlotsByText.end()) {
      const unsigned Slot = SlotsByText.size();
      It = SlotsByText.emplace(Name, Slot).first;
      Layout.namesBySlot.push_back(InvalidIndex);
    }
    Layout.slotsByName[Instruction.reference] = It->second;
    if (It->second >= Layout.namesBySlot.size())
      Layout.namesBySlot.resize(It->second + 1, InvalidIndex);
    if (Layout.namesBySlot[It->second] == InvalidIndex)
      Layout.namesBySlot[It->second] = Instruction.reference;
  }

  if (!IsTopLevel) {
    for (unsigned Index = 0; Index < Body.parameterNames.size(); ++Index) {
      auto It = SlotsByText.find(Body.parameterNames[Index]);
      if (It == SlotsByText.end())
        return fail(Error, "function parameter is missing a local slot"), Layout;
      for (unsigned Name = 0; Name < Artifact.names.size(); ++Name)
        if (Artifact.names[Name] == Body.parameterNames[Index])
          Layout.slotsByName[Name] = It->second;
      if (Layout.namesBySlot[It->second] == InvalidIndex) {
        for (unsigned Name = 0; Name < Artifact.names.size(); ++Name) {
          if (Artifact.names[Name] == Body.parameterNames[Index]) {
            Layout.namesBySlot[It->second] = Name;
            break;
          }
        }
      }
    }
  }
  return Layout;
}

static std::vector<std::set<unsigned>>
computeBoundIn(const LegacyCFG &CFG, const VariableLayout &Layout) {
  const unsigned SlotCount = Layout.namesBySlot.size();
  std::set<unsigned> Universe;
  for (unsigned Slot = 0; Slot < SlotCount; ++Slot)
    Universe.insert(Slot);

  std::vector<std::set<unsigned>> Definitions(CFG.starts.size());
  for (unsigned Block = 0; Block < CFG.starts.size(); ++Block) {
    const unsigned Start = CFG.starts[Block];
    const unsigned End = Block + 1 < CFG.starts.size()
                             ? CFG.starts[Block + 1]
                             : static_cast<unsigned>(CFG.entries.size());
    for (unsigned Index = Start; Index < End; ++Index) {
      const LegacyEntry &Entry = CFG.entries[Index];
      if (!Entry.instruction ||
          Entry.instruction->opcode != CDOpcode::StoreVar)
        continue;
      auto It = Layout.slotsByName.find(Entry.instruction->reference);
      if (It != Layout.slotsByName.end())
        Definitions[Block].insert(It->second);
    }
  }

  std::vector<std::set<unsigned>> In(CFG.starts.size(), Universe);
  if (!In.empty())
    In[0] = Layout.parameterSlots;
  bool Changed = true;
  while (Changed) {
    Changed = false;
    std::vector<std::set<unsigned>> Out(CFG.starts.size());
    for (unsigned Block = 0; Block < CFG.starts.size(); ++Block) {
      Out[Block] = In[Block];
      Out[Block].insert(Definitions[Block].begin(), Definitions[Block].end());
    }
    for (unsigned Block = 1; Block < CFG.starts.size(); ++Block) {
      std::set<unsigned> Next = Universe;
      if (!CFG.predecessors[Block].empty()) {
        for (unsigned Pred : CFG.predecessors[Block]) {
          std::set<unsigned> Intersection;
          std::set_intersection(Next.begin(), Next.end(), Out[Pred].begin(),
                                Out[Pred].end(),
                                std::inserter(Intersection,
                                              Intersection.begin()));
          Next = std::move(Intersection);
        }
      }
      if (Next != In[Block]) {
        In[Block] = std::move(Next);
        Changed = true;
      }
    }
  }
  return In;
}

static void eraseProvenance(ProvenanceState &State, unsigned Result) {
  if (Result != InvalidIndex)
    State.registers.erase(Result);
}

static void transferProvenance(const LegacyEntry &Entry,
                               const VariableLayout &Layout,
                               ProvenanceState &State) {
  if (!Entry.instruction)
    return;
  const CDInstruction &Instruction = *Entry.instruction;
  switch (Instruction.opcode) {
  case CDOpcode::Variant:
    eraseProvenance(State, Instruction.result);
    if (Instruction.result != InvalidIndex)
      State.registers[Instruction.result] =
          {Instruction.reference, Instruction.secondaryReference};
    return;
  case CDOpcode::Move:
    eraseProvenance(State, Instruction.result);
    if (Instruction.result != InvalidIndex && Instruction.operands.size() == 1) {
      auto It = State.registers.find(Instruction.operands[0]);
      if (It != State.registers.end())
        State.registers[Instruction.result] = It->second;
    }
    return;
  case CDOpcode::LoadVar:
    eraseProvenance(State, Instruction.result);
    if (Instruction.result != InvalidIndex) {
      auto Variable = State.variables.find(Instruction.reference);
      if (Variable != State.variables.end())
        State.registers[Instruction.result] = Variable->second;
    }
    return;
  case CDOpcode::StoreVar:
    if (!Instruction.operands.empty()) {
      auto Value = State.registers.find(Instruction.operands[0]);
      if (Value != State.registers.end())
        State.variables[Instruction.reference] = Value->second;
      else
        State.variables.erase(Instruction.reference);
    }
    return;
  default:
    eraseProvenance(State, Instruction.result);
    return;
  }
}

static bool mergeProvenance(const LegacyCFG &CFG,
                            const std::vector<ProvenanceState> &Out,
                            unsigned Block, ProvenanceState &Merged) {
  Merged = {};
  if (CFG.predecessors[Block].empty())
    return true;

  const ProvenanceState &First = Out[CFG.predecessors[Block].front()];
  auto mergeMap = [&](const auto &FirstMap, auto &Result, bool Registers) {
    for (const auto &[Key, Value] : FirstMap) {
      bool PresentEverywhere = true;
      for (unsigned Pred : CFG.predecessors[Block]) {
        const auto &Map = Registers ? Out[Pred].registers : Out[Pred].variables;
        auto It = Map.find(Key);
        if (It == Map.end() || !(It->second == Value)) {
          PresentEverywhere = false;
          break;
        }
      }
      if (PresentEverywhere)
        Result[Key] = Value;
    }
  };
  mergeMap(First.registers, Merged.registers, true);
  mergeMap(First.variables, Merged.variables, false);
  return true;
}

static std::vector<ProvenanceState>
computeProvenance(const LegacyCFG &CFG, const VariableLayout &Layout) {
  (void)Layout;
  std::vector<ProvenanceState> In(CFG.starts.size());
  std::vector<ProvenanceState> Out(CFG.starts.size());
  bool Changed = true;
  while (Changed) {
    Changed = false;
    for (unsigned Block = 0; Block < CFG.starts.size(); ++Block) {
      ProvenanceState Next;
      if (Block != 0)
        mergeProvenance(CFG, Out, Block, Next);
      if (!(Next == In[Block])) {
        In[Block] = Next;
        Changed = true;
      }

      ProvenanceState State = In[Block];
      const unsigned Start = CFG.starts[Block];
      const unsigned End = Block + 1 < CFG.starts.size()
                               ? CFG.starts[Block + 1]
                               : static_cast<unsigned>(CFG.entries.size());
      for (unsigned Index = Start; Index < End; ++Index)
        transferProvenance(CFG.entries[Index], Layout, State);
      if (!(State == Out[Block])) {
        Out[Block] = std::move(State);
        Changed = true;
      }
    }
  }

  std::vector<ProvenanceState> Before(CFG.entries.size());
  for (unsigned Block = 0; Block < CFG.starts.size(); ++Block) {
    ProvenanceState State = In[Block];
    const unsigned Start = CFG.starts[Block];
    const unsigned End = Block + 1 < CFG.starts.size()
                             ? CFG.starts[Block + 1]
                             : static_cast<unsigned>(CFG.entries.size());
    for (unsigned Index = Start; Index < End; ++Index) {
      Before[Index] = State;
      transferProvenance(CFG.entries[Index], Layout, State);
    }
  }
  return Before;
}

class CDArtifactLowerer {
  const CDArtifact &Artifact;
  V2Artifact Lowered;
  std::map<std::string, unsigned> TypeIndexes;
  std::map<std::string, unsigned> NativeIndexes;
  VariableLayout GlobalLayout;
  std::string Error;

  StringRef name(unsigned Index) const { return Artifact.names[Index]; }

  unsigned ensureType(StringRef Name, bool IsEnum) {
    auto It = TypeIndexes.find(Name.str());
    if (It != TypeIndexes.end()) {
      if (Lowered.types[It->second].isEnum != IsEnum) {
        fail(Error, Twine("type `") + Name + "` is used as both struct and enum");
        return InvalidIndex;
      }
      return It->second;
    }
    const unsigned Index = Lowered.types.size();
    TypeIndexes[Name.str()] = Index;
    Lowered.types.push_back({IsEnum, Name.str(), {}, {}});
    return Index;
  }

  unsigned ensureStruct(StringRef TypeName,
                        const std::vector<std::string> &Fields) {
    const unsigned Type = ensureType(TypeName, false);
    if (Type == InvalidIndex)
      return Type;
    V2Type &Layout = Lowered.types[Type];
    if (!Layout.fieldNames.empty() || !Fields.empty()) {
      if (Layout.fieldNames.empty())
        Layout.fieldNames = Fields;
      else if (Layout.fieldNames != Fields) {
        fail(Error, Twine("struct type `") + TypeName +
                         "` has inconsistent field layout");
        return InvalidIndex;
      }
    }
    return Type;
  }

  unsigned ensureVariant(StringRef EnumName, StringRef VariantName,
                         unsigned PayloadCount, bool PayloadKnown) {
    const unsigned Type = ensureType(EnumName, true);
    if (Type == InvalidIndex)
      return Type;
    V2Type &Layout = Lowered.types[Type];
    auto It = std::find_if(
        Layout.variants.begin(), Layout.variants.end(),
        [&](const V2Variant &Variant) { return Variant.name == VariantName; });
    if (It == Layout.variants.end()) {
      Layout.variants.push_back(
          {VariantName.str(), PayloadCount, PayloadKnown});
      return static_cast<unsigned>(Layout.variants.size() - 1);
    }
    if (PayloadKnown) {
      if (It->payloadCountKnown && It->payloadCount != PayloadCount) {
        fail(Error, Twine("enum variant `") + EnumName + "." + VariantName +
                         "` has inconsistent payload count");
        return InvalidIndex;
      }
      It->payloadCount = PayloadCount;
      It->payloadCountKnown = true;
    }
    return static_cast<unsigned>(It - Layout.variants.begin());
  }

  std::string anonymousStructName(const std::vector<std::string> &Fields) const {
    std::string Result = "__cd_anonymous_struct";
    for (const std::string &Field : Fields)
      Result += "_" + std::to_string(Field.size()) + "_" + Field;
    return Result;
  }

  bool collectTypesFromBody(const CDBody &Body) {
    for (const CDInstruction &Instruction : Body.instructions) {
      if (Instruction.opcode == CDOpcode::Struct) {
        std::vector<std::string> Fields;
        for (unsigned Index = 0; Index < Instruction.operands.size(); Index += 2)
          Fields.push_back(name(Instruction.operands[Index]).str());
        const std::string TypeName =
            Instruction.reference == InvalidIndex
                ? anonymousStructName(Fields)
                : name(Instruction.reference).str();
        if (ensureStruct(TypeName, Fields) == InvalidIndex)
          return false;
      } else if (Instruction.opcode == CDOpcode::Variant) {
        if (ensureVariant(name(Instruction.reference),
                          name(Instruction.secondaryReference),
                          Instruction.operands.size(), true) == InvalidIndex)
          return false;
      } else if (Instruction.opcode == CDOpcode::VariantTag) {
        if (ensureVariant(name(Instruction.reference),
                          name(Instruction.secondaryReference), 0, false) ==
            InvalidIndex)
          return false;
      }
    }
    return true;
  }

  bool collectTypes() {
    if (!collectTypesFromBody(Artifact.main))
      return false;
    for (const CDFunction &Function : Artifact.functions)
      if (!collectTypesFromBody(Function.body))
        return false;
    return true;
  }

  unsigned ensureNative(StringRef NativeName) {
    auto It = NativeIndexes.find(NativeName.str());
    if (It != NativeIndexes.end())
      return It->second;
    if (!isSupportedNativeName(NativeName)) {
      fail(Error, Twine("unsupported native name `") + NativeName + "`");
      return InvalidIndex;
    }
    const unsigned Index = Lowered.nativeImports.size();
    NativeIndexes[NativeName.str()] = Index;
    Lowered.nativeImports.push_back({NativeName.str(), 1});
    return Index;
  }

  bool collectNativesFromBody(const CDBody &Body) {
    for (const CDInstruction &Instruction : Body.instructions) {
      if (Instruction.opcode == CDOpcode::NativeCall) {
        if (Instruction.reference >= Artifact.names.size())
          return fail(Error, "native name reference is outside the name table");
        if (ensureNative(name(Instruction.reference)) == InvalidIndex)
          return false;
      } else if (Instruction.opcode == CDOpcode::Print) {
        if (ensureNative("print") == InvalidIndex)
          return false;
      }
    }
    return true;
  }

  bool collectNatives() {
    if (!collectNativesFromBody(Artifact.main))
      return false;
    for (const CDFunction &Function : Artifact.functions)
      if (!collectNativesFromBody(Function.body))
        return false;
    return true;
  }

  bool collectGlobals() {
    GlobalLayout = buildVariableLayout(Artifact, Artifact.main,
                                       /*IsTopLevel=*/true, Error);
    if (!Error.empty())
      return false;
    Lowered.globals = GlobalLayout.namesBySlot;
    return true;
  }

  unsigned typeForStruct(const CDInstruction &Instruction,
                         std::vector<unsigned> &Elements) {
    std::vector<std::string> Fields;
    for (unsigned Index = 0; Index < Instruction.operands.size(); Index += 2)
      Fields.push_back(name(Instruction.operands[Index]).str());
    const std::string TypeName =
        Instruction.reference == InvalidIndex
            ? anonymousStructName(Fields)
            : name(Instruction.reference).str();
    auto TypeIt = TypeIndexes.find(TypeName);
    if (TypeIt == TypeIndexes.end()) {
      fail(Error, "struct type was not collected");
      return InvalidIndex;
    }
    const V2Type &Layout = Lowered.types[TypeIt->second];
    std::vector<bool> Used(Instruction.operands.size() / 2, false);
    Elements.clear();
    for (const std::string &Field : Layout.fieldNames) {
      bool Found = false;
      for (unsigned Pair = 0; Pair < Instruction.operands.size() / 2;
           ++Pair) {
        if (Used[Pair] || name(Instruction.operands[Pair * 2]) != Field)
          continue;
        Used[Pair] = true;
        Elements.push_back(Instruction.operands[Pair * 2 + 1]);
        Found = true;
        break;
      }
      if (!Found) {
        fail(Error, Twine("struct constructor for `") + TypeName +
                         "` is missing field `" + Field + "`");
        return InvalidIndex;
      }
    }
    for (bool WasUsed : Used)
      if (!WasUsed) {
        fail(Error, Twine("struct constructor for `") + TypeName +
                         "` has an unexpected field");
        return InvalidIndex;
      }
    return TypeIt->second;
  }

  unsigned typeForVariant(const VariantIdentity &Identity,
                          unsigned &Variant) {
    if (Identity.enumName >= Artifact.names.size() ||
        Identity.variantName >= Artifact.names.size()) {
      fail(Error, "variant provenance references an invalid name");
      return InvalidIndex;
    }
    auto TypeIt = TypeIndexes.find(name(Identity.enumName).str());
    if (TypeIt == TypeIndexes.end()) {
      fail(Error, "variant provenance has no enum type layout");
      return InvalidIndex;
    }
    const V2Type &Layout = Lowered.types[TypeIt->second];
    auto VariantIt = std::find_if(
        Layout.variants.begin(), Layout.variants.end(), [&](const V2Variant &V) {
          return V.name == name(Identity.variantName);
        });
    if (VariantIt == Layout.variants.end()) {
      fail(Error, "variant provenance has no variant layout");
      return InvalidIndex;
    }
    Variant = static_cast<unsigned>(VariantIt - Layout.variants.begin());
    return TypeIt->second;
  }

  bool mapVariable(const VariableLayout &Layout, unsigned NameIndex,
                   unsigned &Slot) {
    auto It = Layout.slotsByName.find(NameIndex);
    if (It == Layout.slotsByName.end()) {
      fail(Error, "variable reference has no allocated slot");
      return false;
    }
    Slot = It->second;
    return true;
  }

  std::optional<V2Instruction>
  convertInstruction(const CDInstruction &Instruction,
                     const VariableLayout &Layout,
                     const std::vector<ProvenanceState> &Provenance,
                     unsigned EntryIndex, unsigned &NextScratchRegister,
                     bool IsTopLevel, const std::vector<unsigned> &Bound,
                     std::set<unsigned> &CurrentBound) {
    V2Instruction Result;
    Result.result = Instruction.result;
    Result.operands = Instruction.operands;
    switch (Instruction.opcode) {
    case CDOpcode::Constant:
      Result.opcode = V2Opcode::Constant;
      Result.reference = Instruction.reference;
      return Result;
    case CDOpcode::MakeFunction:
      if (Instruction.reference >
          std::numeric_limits<unsigned>::max() - (Artifact.module ? 1 : 0)) {
        fail(Error, "function reference overflow during module lowering");
        return std::nullopt;
      }
      Result.opcode = V2Opcode::MakeFunction;
      Result.reference = Instruction.reference + (Artifact.module ? 1 : 0);
      return Result;
    case CDOpcode::Array:
      Result.opcode = V2Opcode::Array;
      return Result;
    case CDOpcode::Map:
      Result.opcode = V2Opcode::Map;
      return Result;
    case CDOpcode::Struct: {
      std::vector<unsigned> Elements;
      const unsigned Type = typeForStruct(Instruction, Elements);
      if (Type == InvalidIndex)
        return std::nullopt;
      Result.opcode = V2Opcode::MakeStruct;
      Result.reference = Type;
      Result.operands = std::move(Elements);
      return Result;
    }
    case CDOpcode::Variant: {
      auto TypeIt = TypeIndexes.find(name(Instruction.reference).str());
      if (TypeIt == TypeIndexes.end()) {
        fail(Error, "variant type was not collected");
        return std::nullopt;
      }
      const V2Type &Type = Lowered.types[TypeIt->second];
      auto VariantIt = std::find_if(
          Type.variants.begin(), Type.variants.end(), [&](const V2Variant &V) {
            return V.name == name(Instruction.secondaryReference);
          });
      if (VariantIt == Type.variants.end()) {
        fail(Error, "variant was not collected");
        return std::nullopt;
      }
      Result.opcode = V2Opcode::MakeVariant;
      Result.reference = TypeIt->second;
      Result.secondaryReference =
          static_cast<unsigned>(VariantIt - Type.variants.begin());
      return Result;
    }
    case CDOpcode::VariantTag: {
      auto TypeIt = TypeIndexes.find(name(Instruction.reference).str());
      if (TypeIt == TypeIndexes.end()) {
        fail(Error, "variant tag type was not collected");
        return std::nullopt;
      }
      const V2Type &Type = Lowered.types[TypeIt->second];
      auto VariantIt = std::find_if(
          Type.variants.begin(), Type.variants.end(), [&](const V2Variant &V) {
            return V.name == name(Instruction.secondaryReference);
          });
      if (VariantIt == Type.variants.end()) {
        fail(Error, "variant tag was not collected");
        return std::nullopt;
      }
      Result.opcode = V2Opcode::IsVariant;
      Result.reference = TypeIt->second;
      Result.secondaryReference =
          static_cast<unsigned>(VariantIt - Type.variants.begin());
      return Result;
    }
    case CDOpcode::VariantField: {
      if (EntryIndex >= Provenance.size() || Instruction.operands.size() != 1) {
        fail(Error, "variant field has no provenance state");
        return std::nullopt;
      }
      auto It = Provenance[EntryIndex].registers.find(Instruction.operands[0]);
      if (It == Provenance[EntryIndex].registers.end()) {
        fail(Error,
             "llvm.cd.variant.field requires a statically identifiable variant "
             "for cdbc 0.2");
        return std::nullopt;
      }
      unsigned Variant = InvalidIndex;
      const unsigned Type = typeForVariant(It->second, Variant);
      if (Type == InvalidIndex)
        return std::nullopt;
      if (Variant >= Lowered.types[Type].variants.size() ||
          Instruction.payloadIndex >=
              Lowered.types[Type].variants[Variant].payloadCount) {
        fail(Error, "variant field index is outside the inferred payload layout");
        return std::nullopt;
      }
      Result.opcode = V2Opcode::VariantGet;
      Result.reference = Type;
      Result.secondaryReference = Variant;
      Result.payloadIndex = Instruction.payloadIndex;
      return Result;
    }
    case CDOpcode::Field:
      Result.opcode = V2Opcode::Field;
      Result.reference = Instruction.reference;
      return Result;
    case CDOpcode::AssignField:
      Result.opcode = V2Opcode::AssignField;
      Result.reference = Instruction.reference;
      return Result;
    case CDOpcode::Index:
      Result.opcode = V2Opcode::Index;
      return Result;
    case CDOpcode::AssignIndex:
      Result.opcode = V2Opcode::AssignIndex;
      return Result;
    case CDOpcode::Len:
      Result.opcode = V2Opcode::Len;
      return Result;
    case CDOpcode::AssertArray:
      fail(Error,
           "llvm.cd.assert.array has no cdbc 0.2 opcode; use a typed array "
           "operation instead");
      return std::nullopt;
    case CDOpcode::Move:
      Result.opcode = V2Opcode::Move;
      return Result;
    case CDOpcode::LoadVar: {
      unsigned Slot = InvalidIndex;
      if (!mapVariable(Layout, Instruction.reference, Slot))
        return std::nullopt;
      Result.opcode = IsTopLevel ? V2Opcode::LoadGlobal : V2Opcode::LoadLocal;
      Result.reference = Slot;
      return Result;
    }
    case CDOpcode::StoreVar: {
      unsigned Slot = InvalidIndex;
      if (!mapVariable(Layout, Instruction.reference, Slot))
        return std::nullopt;
      const bool WasBound = CurrentBound.count(Slot) != 0;
      CurrentBound.insert(Slot);
      Result.opcode = IsTopLevel
                          ? (WasBound ? V2Opcode::SetGlobal
                                      : V2Opcode::InitGlobal)
                          : (WasBound ? V2Opcode::SetLocal : V2Opcode::BindLocal);
      Result.reference = Slot;
      return Result;
    }
    case CDOpcode::Call:
      Result.opcode = V2Opcode::Call;
      Result.callee = Instruction.callee;
      return Result;
    case CDOpcode::NativeCall: {
      const unsigned Native = ensureNative(name(Instruction.reference));
      if (Native == InvalidIndex)
        return std::nullopt;
      Result.opcode = V2Opcode::CallNative;
      Result.reference = Native;
      return Result;
    }
    case CDOpcode::Print: {
      const unsigned Native = ensureNative("print");
      if (Native == InvalidIndex)
        return std::nullopt;
      Result.opcode = V2Opcode::CallNative;
      Result.result = NextScratchRegister++;
      Result.reference = Native;
      return Result;
    }
    case CDOpcode::Negate:
      Result.opcode = V2Opcode::Negate;
      return Result;
    case CDOpcode::Not:
      Result.opcode = V2Opcode::Not;
      return Result;
    case CDOpcode::Add:
      Result.opcode = V2Opcode::Add;
      return Result;
    case CDOpcode::Subtract:
      Result.opcode = V2Opcode::Subtract;
      return Result;
    case CDOpcode::Multiply:
      Result.opcode = V2Opcode::Multiply;
      return Result;
    case CDOpcode::Divide:
      Result.opcode = V2Opcode::Divide;
      return Result;
    case CDOpcode::Equal:
      Result.opcode = V2Opcode::Equal;
      return Result;
    case CDOpcode::NotEqual:
      Result.opcode = V2Opcode::NotEqual;
      return Result;
    case CDOpcode::Greater:
      Result.opcode = V2Opcode::Greater;
      return Result;
    case CDOpcode::GreaterEqual:
      Result.opcode = V2Opcode::GreaterEqual;
      return Result;
    case CDOpcode::Less:
      Result.opcode = V2Opcode::Less;
      return Result;
    case CDOpcode::LessEqual:
      Result.opcode = V2Opcode::LessEqual;
      return Result;
    default:
      fail(Error, Twine("legacy opcode `") + opcodeName(Instruction.opcode) +
                       "` has no cdbc 0.2 lowering");
      return std::nullopt;
    }
  }

  bool lowerBody(const CDBody &OldBody, bool IsTopLevel,
                 bool IsModuleInitializer, V2Body &Body) {
    const CDModuleMetadata *Module =
        IsModuleInitializer && Artifact.module ? &*Artifact.module : nullptr;
    LegacyCFG CFG;
    if (!buildLegacyCFG(OldBody, Module, CFG, Error))
      return false;

    VariableLayout Layout = IsTopLevel
                                ? GlobalLayout
                                : buildVariableLayout(Artifact, OldBody, false,
                                                       Error);
    if (!Error.empty())
      return false;
    const std::vector<std::set<unsigned>> BoundIn =
        computeBoundIn(CFG, Layout);
    const std::vector<ProvenanceState> Provenance =
        computeProvenance(CFG, Layout);

    Body = {};
    Body.registers = OldBody.registers;
    Body.localCount = IsTopLevel ? 0 : Layout.namesBySlot.size();
    Body.parameterNames = IsTopLevel ? std::vector<std::string>()
                                     : OldBody.parameterNames;
    unsigned NextScratchRegister = OldBody.registers;

    auto blockForOffset = [&](unsigned Offset) -> unsigned {
      const unsigned Entry = CFG.offsetToEntry[Offset];
      auto It = std::lower_bound(CFG.starts.begin(), CFG.starts.end(), Entry);
      if (It == CFG.starts.end() || *It != Entry)
        return InvalidIndex;
      return static_cast<unsigned>(It - CFG.starts.begin());
    };

    for (unsigned Block = 0; Block < CFG.starts.size(); ++Block) {
      const unsigned Start = CFG.starts[Block];
      const unsigned End = Block + 1 < CFG.starts.size()
                               ? CFG.starts[Block + 1]
                               : static_cast<unsigned>(CFG.entries.size());
      V2Instruction BlockStart;
      BlockStart.opcode = V2Opcode::BlockStart;
      BlockStart.target = Block;
      appendV2Instruction(Body, std::move(BlockStart), std::nullopt);

      std::set<unsigned> CurrentBound =
          Block < BoundIn.size() ? BoundIn[Block] : std::set<unsigned>();
      bool HasTerminator = false;
      for (unsigned EntryIndex = Start; EntryIndex < End; ++EntryIndex) {
        const LegacyEntry &Entry = CFG.entries[EntryIndex];
        if (!Entry.instruction) {
          V2Instruction Init;
          Init.opcode = V2Opcode::InitModule;
          Init.reference = Entry.module;
          appendV2Instruction(Body, std::move(Init), Entry.location);
          continue;
        }
        const CDInstruction &Instruction = *Entry.instruction;
        if (Instruction.opcode == CDOpcode::Jump ||
            Instruction.opcode == CDOpcode::JumpIfFalse ||
            Instruction.opcode == CDOpcode::JumpIfTrue) {
          if (EntryIndex + 1 != End) {
            fail(Error, "legacy branch was not lowered at a block boundary");
            return false;
          }
          V2Instruction Branch;
          if (Instruction.opcode == CDOpcode::Jump) {
            Branch.opcode = V2Opcode::Br;
            Branch.target = blockForOffset(Instruction.target);
          } else {
            Branch.opcode = V2Opcode::BrIf;
            Branch.operands = Instruction.operands;
            const unsigned Fallthrough = blockForOffset(Entry.oldOffset + 1);
            const unsigned Target = blockForOffset(Instruction.target);
            if (Instruction.opcode == CDOpcode::JumpIfFalse) {
              Branch.target = Fallthrough;
              Branch.secondaryReference = Target;
            } else {
              Branch.target = Target;
              Branch.secondaryReference = Fallthrough;
            }
          }
          if (Branch.target == InvalidIndex ||
              (Branch.opcode == V2Opcode::BrIf &&
               Branch.secondaryReference == InvalidIndex)) {
            fail(Error, "legacy branch target is not a valid cdbc 0.2 block");
            return false;
          }
          appendV2Instruction(Body, std::move(Branch), Entry.location);
          HasTerminator = true;
          continue;
        }
        if (Instruction.opcode == CDOpcode::Return) {
          if (EntryIndex + 1 != End) {
            fail(Error, "legacy return was not lowered at a block boundary");
            return false;
          }
          V2Instruction Return;
          if (IsModuleInitializer) {
            Return.opcode = V2Opcode::ReturnNil;
          } else {
            Return.opcode = V2Opcode::Return;
            Return.operands = Instruction.operands;
          }
          appendV2Instruction(Body, std::move(Return), Entry.location);
          HasTerminator = true;
          continue;
        }

        std::optional<V2Instruction> Converted = convertInstruction(
            Instruction, Layout, Provenance, EntryIndex, NextScratchRegister,
            IsTopLevel, {}, CurrentBound);
        if (!Converted)
          return false;
        appendV2Instruction(Body, std::move(*Converted), Entry.location);
      }

      if (!HasTerminator) {
        // The legacy linear format allowed a block to fall through to the
        // next block.  cdbc 0.2 requires an explicit terminator, so preserve
        // that edge when there is a following block and only synthesize a
        // nil return for the final block.
        V2Instruction Terminator;
        if (Block + 1 < CFG.starts.size()) {
          Terminator.opcode = V2Opcode::Br;
          Terminator.target = Block + 1;
        } else {
          Terminator.opcode = V2Opcode::ReturnNil;
        }
        appendV2Instruction(Body, std::move(Terminator), std::nullopt);
      }
    }
    Body.registers = NextScratchRegister;
    return true;
  }

public:
  explicit CDArtifactLowerer(const CDArtifact &Artifact) : Artifact(Artifact) {}

  bool run(V2Artifact &Result, std::string &LoweringError) {
    if (!collectTypes() || !collectNatives() || !collectGlobals()) {
      LoweringError = Error;
      return false;
    }

    if (Artifact.module) {
      Lowered.main.registers = 0;
      V2Instruction BlockStart;
      BlockStart.opcode = V2Opcode::BlockStart;
      BlockStart.target = 0;
      appendV2Instruction(Lowered.main, std::move(BlockStart), std::nullopt);
      V2Instruction Return;
      Return.opcode = V2Opcode::ReturnNil;
      appendV2Instruction(Lowered.main, std::move(Return), std::nullopt);

      V2Function Initializer;
      Initializer.name = "__module_init";
      if (!lowerBody(Artifact.main, true, true, Initializer.body)) {
        LoweringError = Error;
        return false;
      }
      Lowered.functions.push_back(std::move(Initializer));
    } else if (!lowerBody(Artifact.main, true, false, Lowered.main)) {
      LoweringError = Error;
      return false;
    }

    for (const CDFunction &Function : Artifact.functions) {
      V2Function LoweredFunction;
      LoweredFunction.name = Function.name;
      LoweredFunction.arity = Function.arity;
      if (!lowerBody(Function.body, false, false, LoweredFunction.body)) {
        LoweringError = Error;
        return false;
      }
      Lowered.functions.push_back(std::move(LoweredFunction));
    }
    Result = std::move(Lowered);
    LoweringError.clear();
    return true;
  }
};

} // namespace

bool validateArtifact(const CDArtifact &Artifact, std::string &Error) {
  if (!validateConstants(Artifact, Error))
    return false;
  if (!validateDebugSources(Artifact, Error))
    return false;
  if (!validateModuleMetadata(Artifact, Error))
    return false;

  for (unsigned Index = 0; Index < Artifact.names.size(); ++Index)
    if (Artifact.names[Index].empty())
      return fail(Error, Twine("name n") + Twine(Index) + " is empty");

  if (!Artifact.main.parameterNames.empty())
    return fail(Error, "main body must not have parameter records");
  if (!validateBody(Artifact, Artifact.main, "main", Error))
    return false;

  for (unsigned Index = 0; Index < Artifact.functions.size(); ++Index) {
    const CDFunction &Function = Artifact.functions[Index];
    if (Function.arity != Function.body.parameterNames.size())
      return fail(Error, Twine("function f") + Twine(Index) +
                           " arity does not match parameter records");
    if (!validateBody(Artifact, Function.body, functionName(Index), Error))
      return false;
  }
  Error.clear();
  return true;
}

static void writeV2RegisterList(raw_ostream &OS,
                                ArrayRef<unsigned> Registers) {
  OS << "[";
  for (unsigned Index = 0; Index < Registers.size(); ++Index) {
    if (Index != 0)
      OS << ", ";
    OS << registerName(Registers[Index]);
  }
  OS << "]";
}

static void writeV2Instruction(raw_ostream &OS,
                              const V2Instruction &Instruction) {
  if (Instruction.opcode == V2Opcode::BlockStart) {
    OS << "block b" << Instruction.target << ":\n";
    return;
  }

  OS << "  ";
  if (Instruction.result != InvalidIndex)
    OS << registerName(Instruction.result) << " = ";

  switch (Instruction.opcode) {
  case V2Opcode::Constant:
    OS << "constant " << constantName(Instruction.reference);
    break;
  case V2Opcode::MakeFunction:
    OS << "make_function " << functionName(Instruction.reference);
    break;
  case V2Opcode::Array:
    OS << "array ";
    writeV2RegisterList(OS, Instruction.operands);
    break;
  case V2Opcode::Map:
    OS << "map [";
    for (unsigned Index = 0; Index < Instruction.operands.size(); Index += 2) {
      if (Index != 0)
        OS << ", ";
      OS << registerName(Instruction.operands[Index]) << ": "
         << registerName(Instruction.operands[Index + 1]);
    }
    OS << "]";
    break;
  case V2Opcode::MakeStruct:
    OS << "make_struct t" << Instruction.reference << " ";
    writeV2RegisterList(OS, Instruction.operands);
    break;
  case V2Opcode::StructGet:
    OS << "struct_get " << registerName(Instruction.operands[0]) << ", t"
       << Instruction.reference << ", " << Instruction.payloadIndex;
    break;
  case V2Opcode::StructSet:
    OS << "struct_set " << registerName(Instruction.operands[0]) << ", t"
       << Instruction.reference << ", " << Instruction.payloadIndex << ", "
       << registerName(Instruction.operands[1]);
    break;
  case V2Opcode::MakeVariant:
    OS << "make_variant t" << Instruction.reference << ", v"
       << Instruction.secondaryReference << " ";
    writeV2RegisterList(OS, Instruction.operands);
    break;
  case V2Opcode::IsVariant:
    OS << "is_variant " << registerName(Instruction.operands[0]) << ", t"
       << Instruction.reference << ", v" << Instruction.secondaryReference;
    break;
  case V2Opcode::VariantGet:
    OS << "variant_get " << registerName(Instruction.operands[0]) << ", t"
       << Instruction.reference << ", v" << Instruction.secondaryReference
       << ", " << Instruction.payloadIndex;
    break;
  case V2Opcode::Move:
    OS << "move " << registerName(Instruction.operands[0]);
    break;
  case V2Opcode::LoadLocal:
    OS << "load_local l" << Instruction.reference;
    break;
  case V2Opcode::BindLocal:
    OS << "bind_local l" << Instruction.reference << ", "
       << registerName(Instruction.operands[0]);
    break;
  case V2Opcode::SetLocal:
    OS << "set_local l" << Instruction.reference << ", "
       << registerName(Instruction.operands[0]);
    break;
  case V2Opcode::LoadUpvalue:
    OS << "load_upvalue u" << Instruction.reference;
    break;
  case V2Opcode::SetUpvalue:
    OS << "set_upvalue u" << Instruction.reference << ", "
       << registerName(Instruction.operands[0]);
    break;
  case V2Opcode::LoadGlobal:
    OS << "load_global g" << Instruction.reference;
    break;
  case V2Opcode::InitGlobal:
    OS << "init_global g" << Instruction.reference << ", "
       << registerName(Instruction.operands[0]);
    break;
  case V2Opcode::SetGlobal:
    OS << "set_global g" << Instruction.reference << ", "
       << registerName(Instruction.operands[0]);
    break;
  case V2Opcode::Call:
    OS << "call " << registerName(Instruction.callee) << " ";
    writeV2RegisterList(OS, Instruction.operands);
    break;
  case V2Opcode::CallNative:
    OS << "call_native i" << Instruction.reference << " ";
    writeV2RegisterList(OS, Instruction.operands);
    break;
  case V2Opcode::Index:
    OS << "index " << registerName(Instruction.operands[0]) << ", "
       << registerName(Instruction.operands[1]);
    break;
  case V2Opcode::AssignIndex:
    OS << "assign_index " << registerName(Instruction.operands[0]) << ", "
       << registerName(Instruction.operands[1]) << ", "
       << registerName(Instruction.operands[2]);
    break;
  case V2Opcode::Field:
    OS << "field " << registerName(Instruction.operands[0]) << ", "
       << nameName(Instruction.reference);
    break;
  case V2Opcode::AssignField:
    OS << "assign_field " << registerName(Instruction.operands[0]) << ", "
       << nameName(Instruction.reference) << ", "
       << registerName(Instruction.operands[1]);
    break;
  case V2Opcode::Len:
    OS << "len " << registerName(Instruction.operands[0]);
    break;
  case V2Opcode::Negate:
  case V2Opcode::Not:
    OS << v2OpcodeName(Instruction.opcode) << " "
       << registerName(Instruction.operands[0]);
    break;
  case V2Opcode::Add:
  case V2Opcode::Subtract:
  case V2Opcode::Multiply:
  case V2Opcode::Divide:
  case V2Opcode::Equal:
  case V2Opcode::NotEqual:
  case V2Opcode::Greater:
  case V2Opcode::GreaterEqual:
  case V2Opcode::Less:
  case V2Opcode::LessEqual:
    OS << v2OpcodeName(Instruction.opcode) << " "
       << registerName(Instruction.operands[0]) << ", "
       << registerName(Instruction.operands[1]);
    break;
  case V2Opcode::Br:
    OS << "br b" << Instruction.target;
    break;
  case V2Opcode::BrIf:
    OS << "br_if " << registerName(Instruction.operands[0]) << ", b"
       << Instruction.target << ", b" << Instruction.secondaryReference;
    break;
  case V2Opcode::Return:
    OS << "return " << registerName(Instruction.operands[0]);
    break;
  case V2Opcode::ReturnNil:
    OS << "return_nil";
    break;
  case V2Opcode::InitModule:
    OS << "init_module m" << Instruction.reference;
    break;
  case V2Opcode::BlockStart:
    llvm_unreachable("block starts are handled before instruction formatting");
  }
  OS << '\n';
}

static void writeV2Body(raw_ostream &OS, const V2Body &Body) {
  for (const V2Instruction &Instruction : Body.instructions)
    writeV2Instruction(OS, Instruction);
}

static void writeV2Types(raw_ostream &OS, const V2Artifact &Artifact) {
  if (Artifact.types.empty())
    return;
  OS << "\ntypes:\n";
  for (unsigned TypeIndex = 0; TypeIndex < Artifact.types.size(); ++TypeIndex) {
    const V2Type &Type = Artifact.types[TypeIndex];
    OS << "  t" << TypeIndex << " = " << (Type.isEnum ? "enum " : "struct ");
    writeQuoted(OS, Type.name);
    if (Type.isEnum) {
      for (unsigned VariantIndex = 0; VariantIndex < Type.variants.size();
           ++VariantIndex) {
        const V2Variant &Variant = Type.variants[VariantIndex];
        OS << " v" << VariantIndex << "=";
        writeQuoted(OS, Variant.name);
        OS << " payload=" << Variant.payloadCount;
      }
    } else {
      for (unsigned FieldIndex = 0; FieldIndex < Type.fieldNames.size();
           ++FieldIndex) {
        OS << " field" << FieldIndex << "=";
        writeQuoted(OS, Type.fieldNames[FieldIndex]);
      }
    }
    OS << '\n';
  }
}

static bool hasV2DebugLocations(const V2Artifact &Artifact) {
  for (const std::optional<CDDebugLocation> &Location : Artifact.main.locations)
    if (Location)
      return true;
  for (const V2Function &Function : Artifact.functions)
    for (const std::optional<CDDebugLocation> &Location : Function.body.locations)
      if (Location)
        return true;
  return false;
}

static bool hasV2DebugRanges(const V2Artifact &Artifact) {
  for (const std::optional<CDDebugLocation> &Location : Artifact.main.locations)
    if (Location && Location->range)
      return true;
  for (const V2Function &Function : Artifact.functions)
    for (const std::optional<CDDebugLocation> &Location : Function.body.locations)
      if (Location && Location->range)
        return true;
  return false;
}

static void writeV2DebugMetadata(const CDArtifact &Source,
                                 const V2Artifact &Artifact, raw_ostream &OS) {
  if (!Source.debugSources.empty()) {
    OS << "\ndebug_sources:\n";
    for (unsigned Index = 0; Index < Source.debugSources.size(); ++Index) {
      const CDDebugSource &DebugSource = Source.debugSources[Index];
      OS << "  s" << Index << " ";
      if (DebugSource.module) {
        OS << "module=";
        writeQuoted(OS, *DebugSource.module);
        OS << " ";
      }
      OS << "path=";
      writeQuoted(OS, DebugSource.path);
      OS << " text=";
      writeQuoted(OS, DebugSource.text);
      OS << '\n';
    }
  }

  if (hasV2DebugLocations(Artifact)) {
    OS << "\ndebug_locations:\n";
    for (unsigned Index = 0; Index < Artifact.main.locations.size(); ++Index) {
      const auto &Location = Artifact.main.locations[Index];
      if (!Location)
        continue;
      OS << "  main " << Index << " = s" << Location->source << ":"
         << Location->line << ":" << Location->column << '\n';
    }
    for (unsigned FunctionIndex = 0; FunctionIndex < Artifact.functions.size();
         ++FunctionIndex) {
      const V2Body &Body = Artifact.functions[FunctionIndex].body;
      for (unsigned Index = 0; Index < Body.locations.size(); ++Index) {
        const auto &Location = Body.locations[Index];
        if (!Location)
          continue;
        OS << "  function " << functionName(FunctionIndex) << " " << Index
           << " = s" << Location->source << ":" << Location->line << ":"
           << Location->column << '\n';
      }
    }
  }

  if (hasV2DebugRanges(Artifact)) {
    OS << "\ndebug_ranges:\n";
    for (unsigned Index = 0; Index < Artifact.main.locations.size(); ++Index) {
      const auto &Location = Artifact.main.locations[Index];
      if (!Location || !Location->range)
        continue;
      const CDDebugRange &Range = *Location->range;
      OS << "  main " << Index << " = s" << Range.source << ":" << Range.start
         << ":" << Range.end << '\n';
    }
    for (unsigned FunctionIndex = 0; FunctionIndex < Artifact.functions.size();
         ++FunctionIndex) {
      const V2Body &Body = Artifact.functions[FunctionIndex].body;
      for (unsigned Index = 0; Index < Body.locations.size(); ++Index) {
        const auto &Location = Body.locations[Index];
        if (!Location || !Location->range)
          continue;
        const CDDebugRange &Range = *Location->range;
        OS << "  function " << functionName(FunctionIndex) << " " << Index
           << " = s" << Range.source << ":" << Range.start << ":"
           << Range.end << '\n';
      }
    }
  }
}

void serializeArtifact(const CDArtifact &Artifact, raw_ostream &OS) {
  std::string Error;
  if (!validateArtifact(Artifact, Error))
    report_fatal_error(Twine("CD bytecode artifact is invalid: ") + Error);

  V2Artifact Lowered;
  CDArtifactLowerer Lowerer(Artifact);
  if (!Lowerer.run(Lowered, Error))
    report_fatal_error(Twine("CD bytecode 0.2 lowering failed: ") + Error);

  OS << "cdbc 0.2\n\n";
  if (Artifact.module) {
    const CDModuleMetadata &Module = *Artifact.module;
    OS << "artifact: module\n\nmodule:\n";
    OS << "  identity = ";
    writeQuoted(OS, Module.identity);
    OS << "\n  path = ";
    writeQuoted(OS, Module.path);
    OS << "\n  canonical_path = ";
    writeQuoted(OS, Module.canonicalPath);
    OS << "\n  entry = " << (Module.isEntry ? "true" : "false") << '\n';
    if (Module.entryOrder)
      OS << "  entry_order = " << *Module.entryOrder << '\n';
    OS << "  init = f0\n  dependencies:\n";
    for (unsigned Index = 0; Index < Module.dependencies.size(); ++Index) {
      const CDModuleDependency &Dependency = Module.dependencies[Index];
      OS << "    d" << Index << " target=";
      writeQuoted(OS, Dependency.identity);
      OS << " kind="
         << (Dependency.kind == CDModuleDependencyKind::Import ? "import"
                                                                 : "re_export")
         << " requested=";
      writeQuoted(OS, Dependency.requestedPath);
      OS << '\n';
    }
    OS << '\n';
  }

  OS << "constants:\n";
  for (unsigned Index = 0; Index < Artifact.constants.size(); ++Index) {
    OS << "  " << constantName(Index) << " = ";
    switch (Artifact.constants[Index].kind) {
    case CDConstant::Nil:
      OS << "nil";
      break;
    case CDConstant::Number: {
      double Number;
      StringRef(Artifact.constants[Index].text).getAsDouble(Number);
      OS << "number " << numberText(Number);
      break;
    }
    case CDConstant::Bool:
      OS << "bool " << Artifact.constants[Index].text;
      break;
    case CDConstant::String:
      OS << "string ";
      writeQuoted(OS, Artifact.constants[Index].text);
      break;
    }
    OS << '\n';
  }

  OS << "\nnames:\n";
  for (unsigned Index = 0; Index < Artifact.names.size(); ++Index) {
    OS << "  " << nameName(Index) << " = ";
    writeQuoted(OS, Artifact.names[Index]);
    OS << '\n';
  }

  if (!Lowered.globals.empty()) {
    OS << "\nglobals:\n";
    for (unsigned Index = 0; Index < Lowered.globals.size(); ++Index)
      OS << "  g" << Index << " = n" << Lowered.globals[Index] << '\n';
  }
  writeV2Types(OS, Lowered);
  if (!Lowered.nativeImports.empty()) {
    OS << "\nnative_imports:\n";
    for (unsigned Index = 0; Index < Lowered.nativeImports.size(); ++Index) {
      const V2NativeImport &Import = Lowered.nativeImports[Index];
      OS << "  i" << Index << " = ";
      writeQuoted(OS, Import.name);
      OS << " abi=" << Import.abi << '\n';
    }
  }

  OS << "\nmain registers=" << Lowered.main.registers << ":\n";
  writeV2Body(OS, Lowered.main);
  for (unsigned Index = 0; Index < Lowered.functions.size(); ++Index) {
    const V2Function &Function = Lowered.functions[Index];
    OS << "\nfunction " << functionName(Index) << " name=";
    writeQuoted(OS, Function.name);
    OS << " arity=" << Function.arity
       << " registers=" << Function.body.registers << ":\n";
    for (unsigned Parameter = 0;
         Parameter < Function.body.parameterNames.size(); ++Parameter) {
      OS << "  param " << Parameter << " = ";
      writeQuoted(OS, Function.body.parameterNames[Parameter]);
      OS << '\n';
    }
    writeV2Body(OS, Function.body);
  }
  writeV2DebugMetadata(Artifact, Lowered, OS);
}

} // namespace llvm::cd
