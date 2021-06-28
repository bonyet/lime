#pragma once

#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/IR/Mangler.h>
#include <llvm/Target/TargetMachine.h>

class JIT
{
public:
	JIT();
private:
	llvm::orc::ExecutionSession session;
	std::unique_ptr<llvm::DataLayout> dataLayout;
	std::unique_ptr<llvm::orc::RTDyldObjectLinkingLayer> objectLayer;
	std::unique_ptr<llvm::orc::IRCompileLayer> compileLayer;
	std::unique_ptr<llvm::TargetMachine> targetMachine;
};