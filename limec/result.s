	.text
	.def	 @feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
.set @feat.00, 0
	.file	"result.ll"
	.def	 sumi32i32;
	.scl	2;
	.type	32;
	.endef
	.globl	sumi32i32                       # -- Begin function sumi32i32
	.p2align	4, 0x90
sumi32i32:                              # @sumi32i32
.seh_proc sumi32i32
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
	.def	 sumfloatfloat;
	.scl	2;
	.type	32;
	.endef
	.globl	sumfloatfloat                   # -- Begin function sumfloatfloat
	.p2align	4, 0x90
sumfloatfloat:                          # @sumfloatfloat
.seh_proc sumfloatfloat
# %bb.0:                                # %entry
	pushq	%rax
	.seh_stackalloc 8
	.seh_endprologue
	movss	4(%rsp), %xmm0                  # xmm0 = mem[0],zero,zero,zero
	addss	(%rsp), %xmm0
	popq	%rax
	retq
	.seh_endproc
                                        # -- End function
	.def	 main;
	.scl	2;
	.type	32;
	.endef
	.globl	__real@40a00000                 # -- Begin function main
	.section	.rdata,"dr",discard,__real@40a00000
	.p2align	2
__real@40a00000:
	.long	0x40a00000                      # float 5
	.text
	.globl	main
	.p2align	4, 0x90
main:                                   # @main
.seh_proc main
# %bb.0:                                # %entry
	subq	$40, %rsp
	.seh_stackalloc 40
	.seh_endprologue
	movl	$0, 36(%rsp)
	xorl	%ecx, %ecx
	movl	$5, %edx
	callq	sumi32i32
	movl	%eax, 36(%rsp)
	movl	$0, 32(%rsp)
	movss	__real@40a00000(%rip), %xmm1    # xmm1 = mem[0],zero,zero,zero
	xorps	%xmm0, %xmm0
	callq	sumfloatfloat
	movss	%xmm0, 32(%rsp)
	addq	$40, %rsp
	retq
	.seh_endproc
                                        # -- End function
	.globl	_fltused
