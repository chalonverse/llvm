; RUN: llvm-upgrade < %s | llvm-as | llc -march=arm
int %test(int %a1, int %a2, int %a3, int %a4) {
  ret int %a4
}
