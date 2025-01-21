// RUN: %HAKC_SETUP
// RUN: %HAKC_YAML_REPLACE_PATHS
// RUN: %HAKC_YAML_CHANGE_MODE_DAG
// RUN: %HAKC_YAML_CHANGE_MODE_COMP
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_PASS_DAG_ANALYSIS
// RUN: %HAKC_PYTHON_CREATE_DAG
// RUN: %HAKC_PYTHON_ADJUST_DAG
// RUN: %HAKC_PASS_COMPARTMENTALIZE
// RUN: %HAKC_EVALUATE

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
