
define i32 @sumi32i32(i32 %a_, i32 %b_) {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %loadtmp = load i32, i32* %a, align 4
  %loadtmp1 = load i32, i32* %b, align 4
  %0 = add i32 %loadtmp, %loadtmp1
  ret i32 %0
}

define float @sumfloatfloat(float %a_, float %b_) {
entry:
  %a = alloca float, align 4
  %b = alloca float, align 4
  %loadtmp = load float, float* %a, align 4
  %loadtmp1 = load float, float* %b, align 4
  %0 = fadd float %loadtmp, %loadtmp1
  ret float %0
}

define void @main() {
entry:
  %a = alloca i32, align 4
  store i32 0, i32* %a, align 4
  %loadtmp = load i32, i32* %a, align 4
  %0 = call i32 @sumi32i32(i32 %loadtmp, i32 5)
  store volatile i32 %0, i32* %a, align 4
  %b = alloca float, align 4
  store float 0.000000e+00, float* %b, align 4
  %loadtmp1 = load float, float* %b, align 4
  %1 = call float @sumfloatfloat(float %loadtmp1, float 5.000000e+00)
  store volatile float %1, float* %b, align 4
  ret void
}
