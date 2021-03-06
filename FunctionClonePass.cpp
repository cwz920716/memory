#include "FunctionClonePass.h"

using namespace llvm;
using namespace std;

void FunctionDB::init(Module &M)
{
	module = &M;
	num_funct = 0;

	for (auto const& f: M) {
		runOnFunction(f);
	}
	
}

void FunctionDB::runOnFunction(const Function& F)
{
	// Remove the following two lines before you write down your own codes
	unsigned num_load = 0, num_store = 0, num_call = 0;

	for (auto const& bb: F) {
		for (auto const& i: bb) {
			if (const LoadInst *ci = dyn_cast<LoadInst>(&i)) {
				num_load++;
			}
			if (const LoadInst *ci = dyn_cast<LoadInst>(&i)) {
				num_store++;
			}
			if (const CallInst *ci = dyn_cast<CallInst>(&i)) {
				num_call++;
			}
		}
	}

	cdb[&F] = num_load + num_store * 2 + num_call * 10;

	
	if (cdb[&F] > getThreshold())
		outs() << cdb[&F] << "\n";
	
	return;
}

void FunctionClonePass::setEnv(Module& M)
{
	module = &M;
	db.init(M);
}

void FunctionClonePass::buildSCC(CallGraph &cg)
{

	V = 0;
	for (auto i = cg.begin(), e = cg.end(); i != e; ++i) {
		CallGraphNode *n = i->second;
		if (!n->getFunction())
			continue;

		int vi = V;
		indexes[n] = vi;
		names[vi] = n;
		V++;
	}

	// initialization
	count = 0;
	pre = 0;
	for (int v = 0; v < V; v++) {
		marked.push_back(false);
		id.push_back(0);
		low.push_back(0);
	}

	for (int v = 0; v < V; v++) {
        	if (!marked[v]) 
			dfs(cg, v);
        }


	cg.dump();
	for (int v = 0; v < V; v++) {
		if (names[v]->getFunction())
			outs() << names[v]->getFunction()->getName() << " belongs to SCC id=" << id[v] << ", index=" << v << "\n";
	}

}

void FunctionClonePass::dfs(CallGraph &cg, int v)
{
	
	marked[v] = true;
	low[v] = pre++;
	int min = low[v];
	stack.push(v);

	std::unordered_set<int> adj;
	CallGraphNode *node = names[v];
	for (int i = 0; i < node->size(); i++) {
		adj.insert( indexes[ (*node)[i] ] );
		outs() << v << "->" << indexes[ (*node)[i] ] << "\n";
	}

	for (int w : adj) {
		if (!marked[w]) 
			dfs(cg, w);

		if (low[w] < min) 
			min = low[w];
        }

        if (min < low[v]) { 
		low[v] = min; 
		return; 
	}

        int w;
        do {
		w = stack.top();
		stack.pop();
		id[w] = count;
		low[w] = V;
        } while (w != v);

        count++;

}

FunctionClonePass::Context FunctionClonePass::stitch(FunctionClonePass::Context parent, int myId, int myCnt)
{
	Context r = parent;
	r.push_back(myId);
	r.push_back(myCnt);
	return r;
}

std::string FunctionClonePass::context2Str(FunctionClonePass::Context ctx)
{
	std::string r = "_";
	int c = 0;
	for (auto i : ctx) {
		if (c % 2)
			r += std::to_string(i);
		else
			r += std::to_string(i);
		r += "_";
		c++;
	}
	return r;
}

Function *FunctionClonePass::getClonedFunction(Function *funct, CallGraph &cg, FunctionClonePass::Context parent)
{
	int index = indexes[ cg[funct] ];
	int sccId = id[ indexes[ cg[funct] ] ];
	string nameSuff = funct->getName().str() + context2Str(parent); 

	Function *clone = module->getFunction(nameSuff);
	if (clone)
		return clone;

	// initiate a new clone
	outs() << "new clone " << nameSuff << "\n";
	clone = Function::Create( funct->getFunctionType(), funct->getLinkage(), nameSuff, module );

	ValueMap<const Value *, WeakVH> ValueMap;
	auto oldArgs = funct->arg_begin();
	auto clonedArgs = clone->arg_begin();
	for (size_t i = 0; i < funct->arg_size() ;i++) {
		WeakVH nvh(&(*clonedArgs));
		ValueMap[&(*oldArgs)] = nvh;
		oldArgs++;
		clonedArgs++;
	}

	typedef SmallVector<ReturnInst *, 8> ReturnSet; 

	ReturnSet returns;
	llvm::CloneFunctionInto(clone, funct,
				ValueMap,
				false,
				returns,
				"_clone", nullptr,
				nullptr,
				nullptr);

	int ic = 0;
	for(inst_iterator i = inst_begin(clone), e = inst_end(clone); 
				i != e; ++i) {
		Instruction* inst = &*i;
		if (CallInst *call_inst = dyn_cast<CallInst>(inst)) {
			Function *callee = call_inst->getCalledFunction();
			int calleeSccId = id[ indexes[ cg[callee] ] ];
			FunctionClonePass::Context now = parent;
			if ( calleeSccId != sccId )
				now = stitch(parent, index, ic);

			if (!callee || callee->isDeclaration() || db.skip(callee, now))
				continue;

			// start clone...
			Function *clone2 = getClonedFunction(callee, cg, now);
			call_inst->setCalledFunction(clone2);
			
		}
		ic++;
	}

	return clone;

}

bool FunctionClonePass::cloneMain(Function *funct, CallGraph &cg)
{
	FunctionClonePass::Context ctx;
	int index = indexes[ cg[funct] ];
	ctx.push_back(index);

	int ic = 0;
	for(inst_iterator i = inst_begin(funct), e = inst_end(funct); 
				i != e; ++i) {
		Instruction* inst = &*i;
		if (CallInst *call_inst = dyn_cast<CallInst>(inst)) {
			Function *callee = call_inst->getCalledFunction();
			if (!callee || callee->isDeclaration() || db.skip(callee))
				continue;

			// start clone...
			FunctionClonePass::Context now = ctx;
			now.push_back(ic);
			Function *clone = getClonedFunction(callee, cg, now);
			call_inst->setCalledFunction(clone);
		}
		ic++;
	}
}

bool FunctionClonePass::runOnModule(Module& M)
{
	setEnv(M);

	CallGraph cg(M);
	buildSCC(cg);

	Function *mainfp = module->getFunction("main");
	if (mainfp == nullptr)
		return false;

	cloneMain(mainfp, cg);
	return true;
}

void FunctionClonePass::getAnalysisUsage(AnalysisUsage &AU) const
{
	AU.setPreservesCFG();
}

// LLVM uses the address of this static member to identify the pass, so the
// initialization value is unimportant.
char FunctionClonePass::ID = 0;

// Register the pass name to allow it to be called with opt:
// See http://llvm.org/releases/3.5.1/docs/WritingAnLLVMPass.html#running-a-pass-with-opt for more info.
static RegisterPass<FunctionClonePass> X("fclone", "Function Clone Pass For Context Sensitivity");
