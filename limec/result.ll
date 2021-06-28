
define i32 @sum(i32 %a_, i32 %b_) {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %loadtmp = load i32, i32* %a, align 4
  %loadtmp1 = load i32, i32* %b, align 4
  %0 = add i32 %loadtmp, %loadtmp1
  ret i32 %0
}

define i32 @main() {
entry:
  %a = alloca i32, align 4
  store i32 5, i32* %a, align 4
  %b = alloca i32, align 4
  store i32 10, i32* %b, align 4
  %c = alloca i32, align 4
  %loadtmp = load i32, i32* %a, align 4
  %loadtmp1 = load i32, i32* %b, align 4
  %0 = call i32 @sum(i32 %loadtmp, i32 %loadtmp1)
  store i32 %0, i32* %c, align 4
  %loadtmp2 = load i32, i32* %c, align 4
  ret i32 %loadtmp2
}
