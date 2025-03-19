// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER & sleep 1
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE


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
