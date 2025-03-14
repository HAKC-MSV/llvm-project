// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_COMP_DAG_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_RUN_DAG_PASS
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_RUN_PYTHON_DAG
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_COMP_PASS
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

// NOTE: Crashes, need to fix
