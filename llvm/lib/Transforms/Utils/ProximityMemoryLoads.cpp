#include "llvm/Transforms/Utils/ProximityMemoryLoads.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

static cl::opt<int> MaxProximity("max-proximity",
                                 cl::desc("Set max proximity between loads."),
                                 cl::init(-1));

PreservedAnalyses ProximityMemoryLoadsPass::run(Function &F,
                                                FunctionAnalysisManager &AM) {
  const DataLayout &DL = F.getDataLayout();
  const TargetTransformInfo &TTI = AM.getResult<TargetIRAnalysis>(F);
  const uint64_t N = (MaxProximity < 0) ? TTI.getCacheLineSize() : MaxProximity;
  for (BasicBlock &BB : F)
    printProximityMemoryLoads(BB, DL, N);
  return PreservedAnalyses::all();
}

void ProximityMemoryLoadsPass::printProximityMemoryLoads(BasicBlock &BB,
                                                         const DataLayout &DL,
                                                         const uint64_t N) {
  std::vector<LoadInst *> Loads;
  for (Instruction &I : BB)
    if (LoadInst *LoadI = dyn_cast<LoadInst>(&I))
      Loads.push_back(LoadI);

  for (auto OuterIt = Loads.begin(), OuterEnd = Loads.end();
       OuterIt != OuterEnd; ++OuterIt) {
    for (auto InnerIt = std::next(OuterIt); InnerIt != Loads.end(); ++InnerIt) {
      if (isProximity(*OuterIt, *InnerIt, DL, N))
        errs() << "Pair of memory loads are within proximity: \n"
               << **OuterIt << "\n and \n"
               << **InnerIt << ".\n";
    }
  }
}

bool ProximityMemoryLoadsPass::isProximity(LoadInst *LoadA, LoadInst *LoadB,
                                           const DataLayout &DL,
                                           const uint64_t N) {
  Value *PtrA = LoadA->getPointerOperand();
  Value *PtrB = LoadB->getPointerOperand();

  unsigned ASA = PtrA->getType()->getPointerAddressSpace();
  unsigned ASB = PtrB->getType()->getPointerAddressSpace();

  if (ASA != ASB)
    return false;

  uint64_t StartAddrA = getPtrConst(PtrA, DL);
  uint64_t StartAddrB = getPtrConst(PtrB, DL);

  if (StartAddrA == 0 || StartAddrB == 0)
    return false;

  uint64_t EndAddrA = StartAddrA + DL.getTypeStoreSize(LoadA->getType());
  uint64_t EndAddrB = StartAddrB + DL.getTypeStoreSize(LoadB->getType());

  uint64_t OffsetAtoB = (StartAddrB > EndAddrA) ? StartAddrB - EndAddrA : 0;
  uint64_t OffsetBtoA = (StartAddrA > EndAddrB) ? StartAddrA - EndAddrB : 0;
  uint64_t MinOffset = std::max(OffsetAtoB, OffsetBtoA);

  return MinOffset <= N;
}

uint64_t ProximityMemoryLoadsPass::getPtrConst(Value *Ptr,
                                               const DataLayout &DL) {
  // GEP indexing/struct accessing
  if (auto *GEP = dyn_cast<GetElementPtrInst>(Ptr))
    return calcPtrWithOffset(GEP, DL);

  // Const address accesses
  if (auto *Const = dyn_cast<ConstantInt>(Ptr))
    return Const->getZExtValue();

  // Const expression address accesses
  if (auto *ConstExpr = dyn_cast<ConstantExpr>(Ptr)) {
    if (ConstExpr->getOpcode() == Instruction::IntToPtr) {
      if (auto *Const = dyn_cast<ConstantInt>(ConstExpr->getOperand(0)))
        return Const->getZExtValue();
    }
    // TODO: Missing a couple here I believe
  }

  // TODO: Missing non-const address accesses (needs alias analysis)
  // This'd be in a seperate function (getPtrNonConst)

  return 0;
}

uint64_t ProximityMemoryLoadsPass::calcPtrWithOffset(GetElementPtrInst *GEP,
                                                     const DataLayout &DL) {
  uint64_t BaseAddr = 0;
  if (auto *BasePtr = dyn_cast<ConstantInt>(GEP->getPointerOperand()))
    BaseAddr = BasePtr->getZExtValue();
  else
    return 0; // TODO: This fails to cover non-const pointers (needs alias
              // analysis)

  // TODO: This fails to cover situations where the offset isn't constant!
  APInt Offset;
  if (GEP->accumulateConstantOffset(DL, Offset))
    return BaseAddr + Offset.getZExtValue();

  return 0;
}
