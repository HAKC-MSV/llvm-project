// Make sure llvm ir contains the line '%1 = alloca i32, align 4'

// RUN: %clangxx_hakc %s -S -emit-llvm -o %t.ll
// RUN: cat %t.ll | FileCheck %s || exit 1

int x, y, z;
int main() { return 0; }
// CHECK-LABEL: main
// CHECK: %1 = alloca i32, align 4
