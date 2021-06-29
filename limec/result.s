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
	pushq	%rbp
	.seh_pushreg %rbp
	subq	$16, %rsp
	.seh_stackalloc 16
	leaq	16(%rsp), %rbp
	.seh_setframe %rbp, 16
	.seh_endprologue
	movl	$5, -8(%rbp)
	movl	$10, -16(%rbp)
	subq	$32, %rsp
	movl	$5, %ecx
	movl	$10, %edx
	callq	sum
	addq	$32, %rsp
	movl	%eax, -4(%rbp)
	movl	-8(%rbp), %eax
	movl	%eax, -12(%rbp)
	cmpl	$5, %eax
	jb	.LBB1_2
# %bb.1:                                # %btrue
	movl	$16, %eax
	callq	__chkstk
	subq	%rax, %rsp
	movq	%rsp, %rax
	movl	$0, (%rax)
.LBB1_2:                                # %end
	movl	-4(%rbp), %eax
	movq	%rbp, %rsp
	popq	%rbp
	retq
	.seh_endproc
                                        # -- End function
