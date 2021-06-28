#include "JIT.h"

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>

JIT::JIT()
{
	// Global LLVM init
	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllAsmPrinters();

	// Setup JIT
	auto target = llvm::EngineBuilder().selectTarget();
	targetMachine.reset(target);
	dataLayout = std::make_unique<llvm::DataLayout>(targetMachine->createDataLayout());
	objectLayer = std::make_unique<llvm::orc::RTDyldObjectLinkingLayer>(session, []() { return std::make_unique<llvm::SectionMemoryManager>(); });
	
	auto simpleCompiler = std::make_unique<llvm::orc::SimpleCompiler>(*targetMachine);
	compileLayer = std::make_unique<llvm::orc::IRCompileLayer>(session, *objectLayer, std::move(simpleCompiler));

	// Load exported symbols of host process
	llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}