// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE


void *kvmalloc_array(unsigned long size, unsigned int n);

int foo() {
    int * mem; 
    mem = (int *) kvmalloc_array(sizeof(int), 4);
    for(int i = 0; i < 4; ++i){
        *(mem + i) = (int) i;
    }
    return 0;
}

// NOTE: Crashes, need to fix
