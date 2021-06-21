	.text
	.def	 @feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
.set @feat.00, 0
	.file	"result.ll"
	.def	 test;
	.scl	2;
	.type	32;
	.endef
	.globl	test                            # -- Begin function test
	.p2align	4, 0x90
test:                                   # @test
.seh_proc test
# %bb.0:                                # %entry
	pushq	%rax
	.seh_stackalloc 8
	.seh_endprologue
	popq	%rax
	retq
	.seh_endproc
                                        # -- End function
	.def	 main;
	.scl	2;
	.type	32;
	.endef
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
main:                                   # @main
.seh_proc main
# %bb.0:                                # %entry
	subq	$40, %rsp
	.seh_stackalloc 40
	.seh_endprologue
	movl	$15, 32(%rsp)
	movl	$15, 36(%rsp)
	movl	$15, %ecx
	callq	test
	movl	$0, 32(%rsp)
	movl	$10, 36(%rsp)
	movl	$5, %eax
	addq	$40, %rsp
	retq
	.seh_endproc
                                        # -- End function
