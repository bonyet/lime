	.text
	.def	 @feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
.set @feat.00, 0
	.file	"result.ll"
	.def	 main;
	.scl	2;
	.type	32;
	.endef
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
main:                                   # @main
.seh_proc main
# %bb.0:                                # %entry
	subq	$24, %rsp
	.seh_stackalloc 24
	.seh_endprologue
	movl	$0, 4(%rsp)
	movl	$0, 12(%rsp)
	leaq	4(%rsp), %rax
	movq	%rax, 16(%rsp)
	movl	$1, 8(%rsp)
	xorl	%eax, %eax
	addq	$24, %rsp
	retq
	.seh_endproc
                                        # -- End function
