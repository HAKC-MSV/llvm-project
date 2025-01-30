// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_PASS
// RUN: %HAKC_EVALUATE
// TODO: add the linux include for build 
// testing function that allocates variable sized memory is tagged correctly 
#include <linux/slab.h>         // kmalloc()

inline void *kmalloc(size_t size, gfp_t gfp);

int foo() {
    int * mem; 
    mem = (int *) kmalloc(2<<8, GFP_KERNEL);
    for(int i = 0; i < 2<<8; ++i){
        *(mem + i) = (int) i;
    }
    return 0;
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: foo
// CHECK: %1 = tail call i8* @kmalloc(i64 512, i32 3264) #3
// CHECK: %2 = icmp ugt i8* %1, inttoptr (i64 281474976710655 to i8*)
