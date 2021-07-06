
define i32 @main() {
entry:
  %a = alloca i32, align 4
  store i32 0, i32* %a, align 4
  %b = alloca i32, align 4
  store i32 0, i32* %b, align 4
  %ptr = alloca i32*, align 8
  store i32* %a, i32** %ptr, align 8
  %value = alloca i32, align 4
  %loadtmp = load i32*, i32** %ptr, align 8
  %dereftmp = load i32, i32* %loadtmp, align 4
  %0 = add i32 %dereftmp, 1
  store i32 %0, i32* %value, align 4
  ret i32 0
}
