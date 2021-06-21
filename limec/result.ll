
%Dog = type { i32 }

define void @test(i32 %eyeballs_) {
entry:
  %eyeballs = alloca i32, align 4
  ret void
}

define i32 @main() {
entry:
  %dog = alloca %Dog, align 8
  %geptmp = getelementptr %Dog, %Dog* %dog, i32 0, i32 0
  store i32 15, i32* %geptmp, align 4
  %i = alloca i32, align 4
  %geptmp1 = getelementptr %Dog, %Dog* %dog, i32 0, i32 0
  %loadtmp = load i32, i32* %geptmp1, align 4
  store i32 %loadtmp, i32* %i, align 4
  %geptmp2 = getelementptr %Dog, %Dog* %dog, i32 0, i32 0
  %loadtmp3 = load i32, i32* %geptmp2, align 4
  call void @test(i32 %loadtmp3)
  %geptmp4 = getelementptr %Dog, %Dog* %dog, i32 0, i32 0
  store i32 0, i32* %geptmp4, align 4
  store volatile i32 10, i32* %i, align 4
  ret i32 5
}
