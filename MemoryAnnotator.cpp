#include "MemoryAnnotator.h"

using namespace llvm;
using namespace cs380c;

bool MemoryAnnotator::runOnModule(Module& M)
{
	setEnv(M);

	// Remove the following line and before you write down your own codes
	for (Module::iterator I = M.begin(), E = M.end(); 
 			I != E; ++I) {
		annotateFunction(*I);
	}

	return false;
}

void MemoryAnnotator::setEnv(Module& M)
{
	module = &M;

	VoidTy = Type::getVoidTy(module->getContext());
	VoidPtrTy = PointerType::getUnqual(Type::getInt8Ty(module->getContext()));
	SizeTy = Type::getInt64Ty(module->getContext());
	Int32Ty = Type::getInt32Ty(module->getContext());

	Type *types[] = {VoidPtrTy, SizeTy, SizeTy, SizeTy};
	FunctionType *trackStackAllocaTy = FunctionType::get( VoidTy, types, false );
	allocaStackTrack = Function::Create( trackStackAllocaTy, Function::ExternalLinkage, "__track_stack_allocation_integer_after", module );
	// module->getOrInsertFunction("__track_stack_allocation", allocaTrack);

	Type *types2[] = {VoidPtrTy, SizeTy};
	FunctionType *trackHeapAllocaTy = FunctionType::get( VoidTy, types2, false );
	mallocTrack = Function::Create( trackHeapAllocaTy, Function::ExternalLinkage, "__track_heap_allocation_after", module );

	Type *types3[] = {VoidPtrTy};

	FunctionType *trackHeapFreeTy = FunctionType::get( VoidTy, types3, false );
	freeTrack = Function::Create( trackHeapFreeTy, Function::ExternalLinkage, "__track_heap_free_before", module );

	FunctionType *trackStackSaveTy = FunctionType::get( VoidTy, types3, false );
	saveStackTrack = Function::Create( trackStackSaveTy, Function::ExternalLinkage, "__track_stack_save_after", module );

	FunctionType *trackStackRestoreTy = FunctionType::get( VoidTy, types3, false );
	restoreStackTrack = Function::Create( trackStackRestoreTy, Function::ExternalLinkage, "__track_stack_restore_before", module );

	Type *types4[] = {VoidPtrTy, SizeTy, SizeTy, SizeTy};

	FunctionType *trackLoadTy = FunctionType::get( VoidTy, types4, false );
	loadTrack = Function::Create( trackLoadTy, Function::ExternalLinkage, "__track_load_integer_after", module );

	FunctionType *trackStoreTy = FunctionType::get( VoidTy, types4, false );
	storeTrack = Function::Create( trackStoreTy, Function::ExternalLinkage, "__track_store_integer_before", module );

}

void MemoryAnnotator::annotateFunction(Function& F)
{
	// Annotate Function
	if (F.getName() == "main")
		cloneFunction(&F);
	for (Function::iterator I = F.begin(), E = F.end(); 
			I != E; ++I) {
		annotateBasicBlock(*I);
	}
}

void MemoryAnnotator::annotateBasicBlock(BasicBlock& BB)
{
	// Annotate Basic Block
	for (BasicBlock::iterator I = BB.begin(), E = BB.end(); 
 			I != E; ++I) {
		annotateInstruction(*I);
	}
}

unsigned MemoryAnnotator::getStrideWidth(Type *type) 
{
	if (type->isArrayTy()) {
		ArrayType *arrayTy = dyn_cast<ArrayType>(type);
		return getStrideWidth( arrayTy->getElementType() );
	}

	if (isa<IntegerType>(type)) {
		IntegerType *int_type = dyn_cast<IntegerType>(type);
		return int_type->getBitWidth();
	}

	outs() << "WARNING: non-integer types require handling of struct/floating types\n";

	if (type->isHalfTy()) 
		return 16;

	if (type->isFloatTy()) 
		return 32;

	if (type->isDoubleTy()) 
		return 64;

	return 0;
}

uint64_t MemoryAnnotator::getArraySize(Type *type) 
{
	if (type->isArrayTy()) {
		ArrayType *arrayTy = dyn_cast<ArrayType>(type);
		return arrayTy->getNumElements();
	} else
		return 1;
}

void MemoryAnnotator::annotateAllocaInst(AllocaInst *inst)
{
	outs() << "----" << *inst << "----\n";

	// alignment
	unsigned align = inst->getAlignment();
	Constant *alignment = ConstantInt::get( SizeTy, align );

	// Array Size and Stride Size
	Type *allocatedTy = inst->getAllocatedType();
	Value *arraySize = nullptr;
	Constant *stride = ConstantInt::get( SizeTy, getStrideWidth(allocatedTy) );

	if (inst->isArrayAllocation()) {
		outs() << "WARNING: this is array allocation for AllocaInst\n";
		arraySize = inst->getArraySize();
		if ( !arraySize->getType()->isIntegerTy(64) ) {
			CastInst *castArg2 = CastInst::CreateIntegerCast (arraySize, SizeTy, false);
			castArg2->insertBefore(inst);
			arraySize = castArg2;
		}
	} else {
		// if this is a array or not
		uint64_t array = getArraySize(allocatedTy);
		arraySize = ConstantInt::get( SizeTy, array );
	}

	if (!inst->isStaticAlloca()) {
		outs() << "WARNING: non-static alloca require handling of stack save/restore\n";
	}

	// Allocated Ptr 
	CastInst *cast = CastInst::CreatePointerCast (inst, VoidPtrTy);
	cast->insertAfter(inst);
	
	Value *args[4] = {cast, arraySize, stride, alignment};
	ArrayRef<Value *> argArray(args, 4);
	outs() << *cast << ", array=[" << *arraySize << "], stride=" << *stride << ", align" << *alignment << "\n";
	CallInst *tracker = CallInst::Create( module->getFunction("__track_stack_allocation_integer_after"), argArray );
	tracker->insertAfter(cast);
	outs() << "----------------------------\n";
}

void MemoryAnnotator::annotateMalloc(CallInst *inst)
{
	outs() << "--------" << *inst << "--------\n";

	Value *mallocSize = inst->getArgOperand(0);
	
	Value *args[2] = {inst, mallocSize};
	ArrayRef<Value *> argArray(args, 2);
	outs() << *inst << " " << *mallocSize << "\n";
	CallInst *tracker = CallInst::Create( module->getFunction("__track_heap_allocation_after"), argArray );
	tracker->insertAfter(inst);
	outs() << "----------------------------\n";
}

void MemoryAnnotator::annotateFree(CallInst *inst)
{
	outs() << "--------" << *inst << "--------\n";

	Value *freePtr = inst->getArgOperand(0);
	
	Value *args[] = {freePtr};
	ArrayRef<Value *> argArray(args, 1);
	outs() << *inst << " " << *freePtr << "\n";
	CallInst *tracker = CallInst::Create( module->getFunction("__track_heap_free_before"), argArray );
	tracker->insertBefore(inst);
	outs() << "----------------------------\n";
}

void MemoryAnnotator::annotateStackSave(CallInst *inst)
{
	outs() << "--------" << *inst << "--------\n";

	Value *stackSavePtr = inst;
	
	Value *args[] = {stackSavePtr};
	ArrayRef<Value *> argArray(args, 1);
	outs() << *inst << " " << *stackSavePtr << "\n";
	CallInst *tracker = CallInst::Create( module->getFunction("__track_stack_save_after"), argArray );
	tracker->insertAfter(inst);
	outs() << "----------------------------\n";
}

void MemoryAnnotator::annotateStackRestore(CallInst *inst)
{
	outs() << "--------" << *inst << "--------\n";

	Value *stackRestorePtr = inst->getArgOperand(0);
	
	Value *args[] = {stackRestorePtr};
	ArrayRef<Value *> argArray(args, 1);
	outs() << *inst << " " << *stackRestorePtr << "\n";
	CallInst *tracker = CallInst::Create( module->getFunction("__track_stack_restore_before"), argArray );
	tracker->insertBefore(inst);
	outs() << "----------------------------\n";
}

CastInst *MemoryAnnotator::cast2SizeTy(Value *value)
{
	if ( !CastInst::isCastable(value->getType(), SizeTy) )
		outs() << "ERROR: non-castable types in load...\n";

	if ( !CastInst::isBitCastable(value->getType(), SizeTy) ) {
		outs() << "WARNING: non-bit castable, maybe a pointer...\n";
		if (value->getType()->isPointerTy()) {
			return CastInst::CreatePointerCast(value, SizeTy);
		}

	}

	return CastInst::CreateZExtOrBitCast(value, SizeTy);

}

void MemoryAnnotator::annotateLoad(LoadInst *inst)
{
	outs() << "--------" << *inst << "--------\n";

	// alignment
	unsigned align = inst->getAlignment();
	Constant *alignment = ConstantInt::get( SizeTy, align );

	Type *loadedTy = inst->getType();
	llvm::DataLayout DL(module);
	Constant *stride = ConstantInt::get( SizeTy, DL.getTypeStoreSize(loadedTy) * 8 );

	Value *loadPtr = inst->getPointerOperand();
	if (loadPtr->getType() != VoidPtrTy) {
		CastInst *castPtr = CastInst::CreatePointerCast (loadPtr, VoidPtrTy);
		castPtr->insertBefore(inst);
		loadPtr = castPtr;
	}

	// value
	outs() << *inst << ", loadPtr=" << *loadPtr << ", stride=" << *stride << "\n";
	Instruction *pos = inst;
	Value *loadee = inst;
	if (!loadedTy->isIntegerTy(64)) {
		CastInst *castLoad = cast2SizeTy(inst);
		outs() << "yyy";
		castLoad->insertAfter(inst);
		pos = castLoad;
		loadee = castLoad;
	}
	
	Value *args[] = {loadPtr, stride, alignment, loadee};
	ArrayRef<Value *> argArray(args, 4);
	CallInst *tracker = CallInst::Create( module->getFunction("__track_load_integer_after"), argArray );
	tracker->insertAfter(pos);
	outs() << "----------------------------\n";
}

void MemoryAnnotator::annotateStore(StoreInst *inst)
{
	outs() << "--------" << *inst << "--------\n";

	// alignment
	unsigned align = inst->getAlignment();
	Constant *alignment = ConstantInt::get( SizeTy, align );

	// pointer
	Value *storePtr = inst->getPointerOperand();
	if (storePtr->getType() != VoidPtrTy) {
		CastInst *castPtr = CastInst::CreatePointerCast (storePtr, VoidPtrTy);
		castPtr->insertBefore(inst);
		storePtr = castPtr;
	}

	// value
	Value *storee = inst->getValueOperand();
	Type *storedTy = storee->getType();
	llvm::DataLayout DL(module);
	Constant *stride = ConstantInt::get( SizeTy, DL.getTypeStoreSize(storedTy) * 8 );

	if (!storedTy->isIntegerTy(64)) {
		CastInst *castStore = cast2SizeTy(storee);
		castStore->insertBefore(inst);
		storee = castStore;
	}

	Value *args[] = {storePtr, stride, alignment, storee};
	ArrayRef<Value *> argArray(args, 4);
	CallInst *tracker = CallInst::Create( module->getFunction("__track_store_integer_before"), argArray );
	tracker->insertBefore(inst);
	outs() << "----------------------------\n";
}

void MemoryAnnotator::annotateInstruction(Instruction& I)
{
	Instruction *inst = &I;
	if (isa<AllocaInst>(inst)) {
		AllocaInst *alloca = dyn_cast<AllocaInst> (inst);
		annotateAllocaInst(alloca);
	}

	if (isa<CallInst>(inst)) {
		CallInst *call = dyn_cast<CallInst> (inst);
		Function *callee = call->getCalledFunction();
		if (callee == nullptr) {
			outs() << "WARNING: An indirect call may require special care\n";
			return;
		}

		if (callee->getName() == "malloc") {
			annotateMalloc(call);
		}

		if (callee->getName() == "free") {
			annotateFree(call);
		}

		if (callee->getName() == "llvm.stacksave") {
			annotateStackSave(call);
		}

		if (callee->getName() == "llvm.stackrestore") {
			annotateStackRestore(call);
		}
	}

	if (isa<LoadInst>(inst)) {
		LoadInst *load = dyn_cast<LoadInst>(inst);
		annotateLoad(load);
	}

	if (isa<StoreInst>(inst)) {
		// not done yet...
		StoreInst *store = dyn_cast<StoreInst>(inst);
		annotateStore(store);
	}

}

void MemoryAnnotator::getAnalysisUsage(AnalysisUsage &AU) const
{
	AU.setPreservesCFG();
}

// LLVM uses the address of this static member to identify the pass, so the
// initialization value is unimportant.
char MemoryAnnotator::ID = 0;

// Register the pass name to allow it to be called with opt:
// See http://llvm.org/releases/3.5.1/docs/WritingAnLLVMPass.html#running-a-pass-with-opt for more info.
static RegisterPass<MemoryAnnotator> X("mannotate", "Memory + Source line number annotator");
