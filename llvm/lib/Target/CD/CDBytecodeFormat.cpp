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
#include "llvm/Support/raw_ostream.h"

#include <cmath>
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

static bool validateUnusedFields(const CDInstruction &Instruction,
                                 bool HasReference, bool HasCallee,
                                 bool HasTarget, StringRef BodyName,
                                 unsigned InstructionIndex,
                                 std::string &Error) {
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
  return true;
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
    default:
      return fail(Error, Twine("constant c") + Twine(Index) +
                           " has an unknown kind");
    }
  }
  return true;
}

static void writeOperands(raw_ostream &OS, ArrayRef<unsigned> Operands) {
  for (unsigned Index = 0; Index < Operands.size(); ++Index) {
    if (Index != 0)
      OS << ", ";
    OS << registerName(Operands[Index]);
  }
}

static void writeInstruction(raw_ostream &OS, const CDInstruction &Instruction) {
  OS << "  ";
  if (Instruction.result != InvalidIndex)
    OS << registerName(Instruction.result) << " = ";

  switch (Instruction.opcode) {
  case CDOpcode::Constant:
    OS << "constant " << constantName(Instruction.reference);
    break;
  case CDOpcode::MakeFunction:
    OS << "make_function " << functionName(Instruction.reference);
    break;
  case CDOpcode::Move:
    OS << "move " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::LoadVar:
    OS << "load_var " << nameName(Instruction.reference);
    break;
  case CDOpcode::StoreVar:
    OS << "store_var " << nameName(Instruction.reference) << ", "
       << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::Call:
    OS << "call " << registerName(Instruction.callee) << " [";
    writeOperands(OS, Instruction.operands);
    OS << "]";
    break;
  case CDOpcode::Print:
    OS << "print " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::Return:
    OS << "return " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::Negate:
    OS << "negate " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::Not:
    OS << "not " << registerName(Instruction.operands[0]);
    break;
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
    OS << opcodeName(Instruction.opcode) << " ";
    writeOperands(OS, Instruction.operands);
    break;
  case CDOpcode::Jump:
    OS << "jump " << Instruction.target;
    break;
  case CDOpcode::JumpIfFalse:
  case CDOpcode::JumpIfTrue:
    OS << opcodeName(Instruction.opcode) << " "
       << registerName(Instruction.operands[0]) << ", " << Instruction.target;
    break;
  }
  OS << '\n';
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
  case CDOpcode::Move:
    return "move";
  case CDOpcode::LoadVar:
    return "load_var";
  case CDOpcode::StoreVar:
    return "store_var";
  case CDOpcode::Call:
    return "call";
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

bool validateArtifact(const CDArtifact &Artifact, std::string &Error) {
  if (!validateConstants(Artifact, Error))
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

void serializeArtifact(const CDArtifact &Artifact, raw_ostream &OS) {
  std::string Error;
  if (!validateArtifact(Artifact, Error))
    report_fatal_error(Twine("CD bytecode artifact is invalid: ") + Error);

  OS << "cdbc 0.1\n\nconstants:\n";
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
    }
    OS << '\n';
  }

  OS << "\nnames:\n";
  for (unsigned Index = 0; Index < Artifact.names.size(); ++Index) {
    OS << "  " << nameName(Index) << " = ";
    writeQuoted(OS, Artifact.names[Index]);
    OS << '\n';
  }

  OS << "\nmain registers=" << Artifact.main.registers << ":\n";
  for (const CDInstruction &Instruction : Artifact.main.instructions)
    writeInstruction(OS, Instruction);

  for (unsigned Index = 0; Index < Artifact.functions.size(); ++Index) {
    const CDFunction &Function = Artifact.functions[Index];
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
    for (const CDInstruction &Instruction : Function.body.instructions)
      writeInstruction(OS, Instruction);
  }
}

} // namespace llvm::cd
