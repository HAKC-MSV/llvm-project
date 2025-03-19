// RUN: source %HAKC_PYTHON_VENV/bin/activate
// RUN: %HAKC_START_POLICY_SERVER & sleep 1
// RUN: %hakc_clang -g -S -emit-llvm -mllvm --enable-hakc -mllvm --hakc-config=%HAKC_CONFIG -o %t %s
// RUN: deactivate
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
