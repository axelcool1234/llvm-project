#include "llvm/Transforms/Utils/ProximityMemoryLoads.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DataLayout.h"

using namespace llvm;

PreservedAnalyses ProximityMemoryLoadsPass::run(Function &F,
                                                FunctionAnalysisManager &AM) { 
    const DataLayout &DL = F.getDataLayout();
    const uint64_t N = 12;
    for (BasicBlock &BB : F) {
        printProximityMemoryLoads(BB, DL, N);
    }
    return PreservedAnalyses::all();
}

void ProximityMemoryLoadsPass::printProximityMemoryLoads(BasicBlock &BB, 
                                                         const DataLayout &DL, 
                                                         const uint64_t N) {
    std::vector<std::pair<LoadInst*, Value*>> Loads;
    for (Instruction &I : BB) {
        if(LoadInst *LoadI = dyn_cast<LoadInst>(&I)) {
            Value* Ptr = LoadI->getPointerOperand();
            if (Ptr->getType()->isPointerTy()) {
                Loads.push_back({LoadI, Ptr});
            }
        }
    }
    std::sort(Loads.begin(), Loads.end(), [](const auto& Lhs, const auto& Rhs) {
        return Lhs.second < Rhs.second;
    });
    for (auto OuterIt = Loads.begin(); OuterIt != Loads.end(); ++OuterIt) {
        LoadInst *OuterLoad = OuterIt->first;
        Value *OuterPtr = OuterIt->second;
        uint64_t OuterSize = DL.getTypeStoreSize(OuterLoad->getType());


        for (auto InnerIt = std::next(OuterIt); InnerIt != Loads.end(); ++InnerIt) {
            LoadInst *InnerLoad = InnerIt->first;
            Value* InnerPtr = InnerIt->second;
            uint64_t InnerSize = DL.getTypeStoreSize(InnerLoad->getType());

            if (isProximity(OuterSize, OuterPtr, InnerSize, InnerPtr, N)) {
                errs() << "Pair of memory loads are within proximity: " 
                       << *OuterLoad << " and " << *InnerLoad << ".\n";
            }
            /*getPointersDiff*/
        }
    }
}

bool ProximityMemoryLoadsPass::isProximity(const uint64_t ElemSizeA, Value *PtrA, 
                                           const uint64_t ElemSizeB, Value *PtrB, 
                                           const uint64_t N) {
    unsigned ASA = PtrA->getType()->getPointerAddressSpace();
    unsigned ASB = PtrB->getType()->getPointerAddressSpace();

    if (ASA != ASB) return false;
    bool SameBaseAddr = PtrA->getValueID() == PtrB->getValueID();

    errs() << "Same Base Address: " << SameBaseAddr << "\n";

    uint64_t StartAddrA = getPtrConst(ElemSizeA, PtrA, SameBaseAddr);
    uint64_t StartAddrB = getPtrConst(ElemSizeB, PtrB, SameBaseAddr);    

    errs() << "N: " << N << "\n";
    errs() << "StartA: "<< StartAddrA << "\nStartB: " << StartAddrB <<"\n";

    if (StartAddrA == 0 || StartAddrB == 0) return false;

    uint64_t EndAddrA = StartAddrA + ElemSizeA;
    uint64_t EndAddrB = StartAddrB + ElemSizeB;

    errs() << "EndsA: " << EndAddrA << "\nEndsB: " << EndAddrB <<"\n";
    errs() << "Min Offset: " << ((StartAddrA > EndAddrB) ? (StartAddrA - EndAddrB) : (StartAddrB - EndAddrA)) << "\n";

    return (StartAddrA > EndAddrB) ? (StartAddrA - EndAddrB) <= N : (StartAddrB - EndAddrA) <= N;
}

uint64_t ProximityMemoryLoadsPass::getPtrConst(const uint64_t ElemSize, Value *Ptr, const bool SameBaseAddr) {
    if (auto *GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
        errs() << "GEP Detected!\n";
        return calcPtrWithOffset(ElemSize, GEP, SameBaseAddr);
    } 

    if (auto *Const = dyn_cast<ConstantInt>(Ptr)) {
        errs() << "Const Detected!\n";
        return Const->getZExtValue();
    } 
    errs() << "None Detected! Type: " << Ptr->getType() << "\n";

    return 0; 
}

uint64_t ProximityMemoryLoadsPass::calcPtrWithOffset(const uint64_t ElemSize, GetElementPtrInst *GEP, const bool SameBaseAddr) {
    uint64_t BaseAddr = 0;
    if (auto *BasePtr = dyn_cast<ConstantInt>(GEP->getPointerOperand())) {
        BaseAddr = BasePtr->getZExtValue(); 
    } else if (!SameBaseAddr) return 0;

    uint64_t Offset = 0;
    for (unsigned OpIndex = 1; OpIndex < GEP->getNumOperands(); ++OpIndex) {
        Value *Index = GEP->getOperand(OpIndex);
        if (auto *ConstIndex = dyn_cast<ConstantInt>(Index)) {
            Offset += ConstIndex->getZExtValue() * ElemSize; 
        } else {
            return 0;
        }
    }
    return BaseAddr + Offset;
}
