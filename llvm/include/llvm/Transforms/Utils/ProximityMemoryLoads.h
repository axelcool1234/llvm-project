#ifndef LLVM_TRANSFORMS_PROXIMITYMEMORYLOADS_H
#define LLVM_TRANSFORMS_PROXIMITYMEMORYLOADS_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class ProximityMemoryLoadsPass
    : public PassInfoMixin<ProximityMemoryLoadsPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
  void printProximityMemoryLoads(BasicBlock &BB, const DataLayout &DL,
                                 const uint64_t N);
  bool isProximity(LoadInst *LoadA, LoadInst *LoadB, const DataLayout &DL,
                   const uint64_t N);
  uint64_t getPtrConst(Value *Ptr, const DataLayout &DL);
  uint64_t calcPtrWithOffset(GetElementPtrInst *GEP, const DataLayout &DL);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_PROXIMITYMEMORYLOADS_H
