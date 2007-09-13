//===-- MipsISelDAGToDAG.cpp - A dag to dag inst selector for Mips --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by Bruno Cardoso Lopes is distributed under the 
// University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the MIPS target.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "mips-isel"

#include "Mips.h"
#include "MipsISelLowering.h"
#include "MipsRegisterInfo.h"
#include "MipsSubtarget.h"
#include "MipsTargetMachine.h"
#include "llvm/GlobalValue.h"
#include "llvm/Instructions.h"
#include "llvm/Intrinsics.h"
#include "llvm/Support/CFG.h"
#include "llvm/Type.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include <queue>
#include <set>

using namespace llvm;

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// MipsDAGToDAGISel - MIPS specific code to select MIPS machine
// instructions for SelectionDAG operations.
//===----------------------------------------------------------------------===//
namespace {

class VISIBILITY_HIDDEN MipsDAGToDAGISel : public SelectionDAGISel {

  /// TM - Keep a reference to MipsTargetMachine.
  MipsTargetMachine &TM;

  /// MipsLowering - This object fully describes how to lower LLVM code to an
  /// Mips-specific SelectionDAG.
  MipsTargetLowering MipsLowering;

  /// Subtarget - Keep a pointer to the MipsSubtarget around so that we can
  /// make the right decision when generating code for different targets.
  //TODO: add initialization on constructor
  //const MipsSubtarget *Subtarget;
 
public:
  MipsDAGToDAGISel(MipsTargetMachine &tm) : 
        SelectionDAGISel(MipsLowering),
        TM(tm), MipsLowering(*TM.getTargetLowering()) {}
  
  virtual void InstructionSelectBasicBlock(SelectionDAG &SD);

  // Pass Name
  virtual const char *getPassName() const {
    return "MIPS DAG->DAG Pattern Instruction Selection";
  } 
  

private:  
  // Include the pieces autogenerated from the target description.
  #include "MipsGenDAGISel.inc"

  SDNode *Select(SDOperand N);

  // Complex Pattern.
  bool SelectAddr(SDOperand Op, SDOperand N, 
                  SDOperand &Base, SDOperand &Offset);


  // getI32Imm - Return a target constant with the specified
  // value, of type i32.
  inline SDOperand getI32Imm(unsigned Imm) {
    return CurDAG->getTargetConstant(Imm, MVT::i32);
  }


  #ifndef NDEBUG
  unsigned Indent;
  #endif
};

}

/// InstructionSelectBasicBlock - This callback is invoked by
/// SelectionDAGISel when it has created a SelectionDAG for us to codegen.
void MipsDAGToDAGISel::
InstructionSelectBasicBlock(SelectionDAG &SD) 
{
  DEBUG(BB->dump());
  // Codegen the basic block.
  #ifndef NDEBUG
  DOUT << "===== Instruction selection begins:\n";
  Indent = 0;
  #endif

  // Select target instructions for the DAG.
  SD.setRoot(SelectRoot(SD.getRoot()));

  #ifndef NDEBUG
  DOUT << "===== Instruction selection ends:\n";
  #endif

  SD.RemoveDeadNodes();
  
  // Emit machine code to BB. 
  ScheduleAndEmitDAG(SD);
}

/// ComplexPattern used on MipsInstrInfo
/// Used on Mips Load/Store instructions
bool MipsDAGToDAGISel::
SelectAddr(SDOperand Op, SDOperand Addr, SDOperand &Offset, SDOperand &Base)
{
  // if Address is FI, get the TargetFrameIndex.
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base   = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
    Offset = CurDAG->getTargetConstant(0, MVT::i32);
    return true;
  }
    
  // TargetExternalSymbol and TargetGlobalAddress are
  // lowered and their addresses go into registers, so
  // they should not be touched here.
  if ((Addr.getOpcode() == ISD::TargetExternalSymbol ||
       Addr.getOpcode() == ISD::TargetGlobalAddress))
    return false;
  
  // Operand is a result from an ADD.
  if (Addr.getOpcode() == ISD::ADD) 
  {
    if (ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1))) 
    {
      if (Predicate_immSExt16(CN)) 
      {
        // If the first operand is a FI, get the TargetFI Node
        if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>
                                    (Addr.getOperand(0))) {
          Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
        } else {
          Base = Addr.getOperand(0);
        }

        Offset = CurDAG->getTargetConstant(CN->getValue(), MVT::i32);
        return true;
      }
    }
  }

  Base   = Addr;
  Offset = CurDAG->getTargetConstant(0, MVT::i32);
  return true;
}

/// Select instructions not customized! Used for
/// expanded, promoted and normal instructions
SDNode* MipsDAGToDAGISel::
Select(SDOperand N) 
{
  SDNode *Node = N.Val;
  unsigned Opcode = Node->getOpcode();

  // Dump information about the Node being selected
  #ifndef NDEBUG
  DOUT << std::string(Indent, ' ') << "Selecting: ";
  DEBUG(Node->dump(CurDAG));
  DOUT << "\n";
  Indent += 2;
  #endif

  // If we have a custom node, we already have selected!
  if (Opcode >= ISD::BUILTIN_OP_END && Opcode < MipsISD::FIRST_NUMBER) {
    #ifndef NDEBUG
    DOUT << std::string(Indent-2, ' ') << "== ";
    DEBUG(Node->dump(CurDAG));
    DOUT << "\n";
    Indent -= 2;
    #endif
    return NULL;
  }

  ///
  // Instruction Selection not handled by custom or by the 
  // auto-generated tablegen selection should be handled here
  /// 
  switch(Opcode) {

    default: break;

    /// Special Mul operations
    case ISD::MULHS:
    case ISD::MULHU: {
      SDOperand MulOp1 = Node->getOperand(0);
      SDOperand MulOp2 = Node->getOperand(1);
      AddToISelQueue(MulOp1);
      AddToISelQueue(MulOp2);

      unsigned MulOp  = (Opcode == ISD::MULHU ? Mips::MULTu : Mips::MULT);
      SDNode *MulNode = CurDAG->getTargetNode(MulOp, MVT::Flag, MulOp1, MulOp2);

      SDOperand MFInFlag = SDOperand(MulNode, 0);
      return CurDAG->getTargetNode(Mips::MFHI, MVT::i32, MFInFlag);
    }

    /// Div operations
    case ISD::SDIV: 
    case ISD::UDIV: {
      SDOperand DivOp1 = Node->getOperand(0);
      SDOperand DivOp2 = Node->getOperand(1);
      AddToISelQueue(DivOp1);
      AddToISelQueue(DivOp2);

      unsigned DivOp  = (Opcode == ISD::SDIV ? Mips::DIV : Mips::DIVu);
      SDNode *DivNode = CurDAG->getTargetNode(DivOp, MVT::Flag, DivOp1, DivOp2);

      SDOperand MFInFlag = SDOperand(DivNode, 0);
      return CurDAG->getTargetNode(Mips::MFLO, MVT::i32, MFInFlag);
    }

    /// Rem operations
    case ISD::SREM: 
    case ISD::UREM: {
      SDOperand RemOp1 = Node->getOperand(0);
      SDOperand RemOp2 = Node->getOperand(1);
      AddToISelQueue(RemOp1);
      AddToISelQueue(RemOp2);
      
      unsigned RemOp  = (Opcode == ISD::SREM ? Mips::DIV : Mips::DIVu);
      SDNode *RemNode = CurDAG->getTargetNode(RemOp, MVT::Flag, RemOp1, RemOp2);

      SDOperand MFInFlag = SDOperand(RemNode, 0);
      return CurDAG->getTargetNode(Mips::MFHI, MVT::i32, MFInFlag);
    }
  }

  // Select the default instruction
  SDNode *ResNode = SelectCode(N);

  #ifndef NDEBUG
  DOUT << std::string(Indent-2, ' ') << "=> ";
  if (ResNode == NULL || ResNode == N.Val)
    DEBUG(N.Val->dump(CurDAG));
  else
    DEBUG(ResNode->dump(CurDAG));
  DOUT << "\n";
  Indent -= 2;
  #endif

  return ResNode;
}

/// createMipsISelDag - This pass converts a legalized DAG into a 
/// MIPS-specific DAG, ready for instruction scheduling.
FunctionPass *llvm::createMipsISelDag(MipsTargetMachine &TM) {
  return new MipsDAGToDAGISel(TM);
}
