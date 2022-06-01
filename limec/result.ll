
@strtmp = private unnamed_addr constant [28 x i8] c"morbius made %d morbcoin\\0A\00", align 1
@strtmp.1 = private unnamed_addr constant [6 x i8] c"%d\\0A\00", align 1

declare i32 @printf(i8*, i32, ...)

define i8* @get_message() {
entry:
  ret i8* getelementptr inbounds ([28 x i8], [28 x i8]* @strtmp, i32 0, i32 0)
}

define i32 @do_math(i32 %a_) {
entry:
  %a = alloca i32, align 4
  store i32 %a_, i32* %a, align 4
  %loadtmp = load i32, i32* %a, align 4
  %0 = mul i32 %loadtmp, 5000
  ret i32 %0
}

define i32 @main() {
entry:
  %msg = alloca i8*, align 8
  store i8* getelementptr inbounds ([6 x i8], [6 x i8]* @strtmp.1, i32 0, i32 0), i8** %msg, align 8
  %val = alloca i64, align 8
  store i64 2, i64* %val, align 4
  %pval = alloca i64*, align 8
  %loadtmp = load i64, i64* %val, align 4
  store i64* %val, i64** %pval, align 8
  %loadtmp1 = load i8*, i8** %msg, align 8
  %loadtmp2 = load i64*, i64** %pval, align 8
  %loadtmp3 = load i64, i64* %loadtmp2, align 4
  %trunctmp = trunc i64 %loadtmp3 to i32
  %0 = call i32 (i8*, i32, ...) @printf(i8* %loadtmp1, i32 %trunctmp)
  store i64 3, i64* %val, align 4
  %loadtmp4 = load i8*, i8** %msg, align 8
  %loadtmp5 = load i64*, i64** %pval, align 8
  %loadtmp6 = load i64, i64* %loadtmp5, align 4
  %trunctmp7 = trunc i64 %loadtmp6 to i32
  %1 = call i32 (i8*, i32, ...) @printf(i8* %loadtmp4, i32 %trunctmp7)
  %loadtmp8 = load i64*, i64** %pval, align 8
  store i64 4, i64* %loadtmp8, align 4
  store i64 6, i64* %val, align 4
  %loadtmp9 = load i8*, i8** %msg, align 8
  %loadtmp10 = load i64*, i64** %pval, align 8
  %loadtmp11 = load i64, i64* %loadtmp10, align 4
  %trunctmp12 = trunc i64 %loadtmp11 to i32
  %2 = call i32 (i8*, i32, ...) @printf(i8* %loadtmp9, i32 %trunctmp12)
  ret i32 0
}
