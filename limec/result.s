	.text
	.def	 @feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
.set @feat.00, 0
	.file	"result.ll"
	.def	 sum;
	.scl	2;
	.type	32;
	.endef
	.globl	sum                             # -- Begin function sum
	.p2align	4, 0x90
sum:                                    # @sum
.seh_proc sum
# %bb.0:                                # %entry
	pushq	%rax
	.seh_stackalloc 8
	.seh_endprologue
	movl	4(%rsp), %eax
	addl	(%rsp), %eax
	popq	%rcx
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
	subq	$56, %rsp
	.seh_stackalloc 56
	.seh_endprologue
	movl	$5, 52(%rsp)
	movl	$10, 48(%rsp)
	movl	$5, %ecx
	movl	$10, %edx
	callq	sum
	movl	%eax, 44(%rsp)
	addq	$56, %rsp
	retq
	.seh_endproc
                                        # -- End function
