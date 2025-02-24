// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_PASS
// RUN: %HAKC_EVALUATE
// TODO: add the linux include for build 
// testing function that allocates variable sized memory is tagged correctly 
// #include <linux/slab.h>         // kmalloc()

void *kmalloc(unsigned long size, unsigned int);

int foo() {
    int * mem; 
    mem = (int *) kmalloc(2<<8, 0);
    for(int i = 0; i < 2<<8; ++i){
        *(mem + i) = (int) i;
    }
    return 0;
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK: call ptr @check_hakc_data_access(ptr %10, i64 1, i64 73728)
// CHECK: call ptr @kmalloc(i64 noundef 512, i32 noundef 0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %3, i64 1, i64 1, i64 13, i1 false)
