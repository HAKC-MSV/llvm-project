# -*- Python -*-

import subprocess
import os

# Setup config name.
config.name = "HAKC" + config.name_suffix

# Setup source root.
config.test_source_root = os.path.dirname(__file__)

# `pip install -r $(realpath ../../../../python/requirements.txt)`

# %HAKC_ROOT =          /home/al32163/hakc/HAKC
# test_source_root =    /home/al32163/hakc/HAKC/llvm-project/compiler-rt/test/hakc
# %s =                  /home/al32163/hakc/HAKC/llvm-project/compiler-rt/test/hakc/TestCases/Posix/hakc_testn/hakc-testn.c
# CONFIG_NAME =         /home/al32163/hakc/HAKC/llvm-project/compiler-rt/test/hakc/TestCases/Posix/hakc_testn/hakc-testn_config.yml
# pwd =                 /home/al32163/hakc/HAKC/cmake-build-hakc-llvm/llvm-project/llvm/projects/compiler-rt/test/hakc/X86_64LinuxConfig/TestCases/Posix
# clangxx_hakc =        /home/al32163/hakc/HAKC/cmake-build-hakc-llvm/llvm-project/llvm/./bin/clang 

# test file structure:
# hakc_testn/
#   existing files:
#       hakc_testn.c
#       hakc_test0_adjustments.yml
#       hakc_test0_config.yml.in
#   build files: 
#       build/*
#       hakc-db/*
#       dag_analysis/*

# todo: are relative paths here ok? 
config.substitutions.append(("%clang_hakc ", "%HAKC_ROOT/install/bin/clang"))
config.substitutions.append(("%clangxx_hakc", "%HAKC_ROOT/install/bin/clang++"))

# run these commands in each test file 
# -save-temps is an option for clang, might be useful for debugging 
config.substitutions.append(("%HAKC_SETUP",                 "mkdir -p build"))
config.substitutions.append(("%HAKC_YAML_REPLACE_PATHS",    "cat %HAKC_YAML_CONFIG.in | sed 's,@PWD@,'%HAKC_TEST_PATH',g' > %HAKC_YAML_CONFIG.tmp"))
config.substitutions.append(("%HAKC_YAML_CHANGE_MODE_DAG",  "cat %HAKC_YAML_CONFIG.tmp | sed 's,@PASS_MODE@,RunDataAccessGraphAnalysis,g' > %HAKC_YAML_CONFIG_DAG"))
config.substitutions.append(("%HAKC_YAML_CHANGE_MODE_COMP", "cat %HAKC_YAML_CONFIG.tmp | sed 's,@PASS_MODE@,RunCompartmentalization,g' > %HAKC_YAML_CONFIG_COMP"))
config.substitutions.append(("%HAKC_PYTHON_VENV",           "python3 -m venv %HAKC_ROOT/python/venv && source %HAKC_ROOT/python/venv/bin/activate"))
config.substitutions.append(("%HAKC_PASS_DAG_ANALYSIS",     "%clangxx_hakc -fpass-plugin=%HAKC_TEST_PASS -Xclang -load -Xclang %HAKC_TEST_PASS -mllvm -HAKC_CONFIG=%HAKC_YAML_CONFIG_DAG -g -S -emit-llvm -O2 -o %t.dag.ll -c %s"))
config.substitutions.append(("%HAKC_PYTHON_CREATE_DAG",     "env PYTHONPATH=%HAKC_PYTHON_PATH python %HAKC_ROOT/python/analysis/hakc-dag.py --log-level INFO --dag-files-root %HAKC_DAG_ROOT_PATH --db-dir %HAKC_DB_PATH --create-dag --single-thread"))
config.substitutions.append(("%HAKC_PYTHON_ADJUST_DAG",     "env PYTHONPATH=%HAKC_PYTHON_PATH python %HAKC_ROOT/python/analysis/hakc-dag.py --log-level INFO --db-dir %HAKC_DB_PATH --adjust --adjust-path %HAKC_YAML_ADJUSTMENT"))
config.substitutions.append(("%HAKC_PASS_COMPARTMENTALIZE", "%clangxx_hakc -fpass-plugin=%HAKC_TEST_PASS -Xclang -load -Xclang %HAKC_TEST_PASS -mllvm -HAKC_CONFIG=%HAKC_YAML_CONFIG_COMP -g -S -emit-llvm -O2 -o %t.comp.ll -c %s"))
config.substitutions.append(("%HAKC_EVALUATE", "cat %t.comp.ll | FileCheck %s || exit 1"))
# recursive substitutions
config.substitutions.append(("%HAKC_YAML_CONFIG",       "%HAKC_TEST_PATH/%FNAME_config.yml"))
config.substitutions.append(("%HAKC_YAML_ADJUSTMENT",   "%HAKC_TEST_PATH/%FNAME_adjustments.yml"))
config.substitutions.append(("%HAKC_YAML_CONFIG_DAG",   "%HAKC_TEST_PATH/%FNAME_config_dag.yml"))
config.substitutions.append(("%HAKC_YAML_CONFIG_COMP",  "%HAKC_TEST_PATH/%FNAME_config_comp.yml"))
config.substitutions.append(("%HAKC_DB_PATH",           "%HAKC_TEST_PATH/hakc-db"))
config.substitutions.append(("%HAKC_DAG_ROOT_PATH",     "%HAKC_TEST_PATH/dag_analysis/_HAKC_SOURCE_PATH_"))
config.substitutions.append(("%HAKC_TEST_PASS",         "%HAKC_ROOT/install/lib/HAKC-Compartmentalizer.so"))
config.substitutions.append(("%HAKC_PYTHON_PATH",       "%HAKC_ROOT/kuzu/tools/python_api/build"))

config.substitutions.append(("%FNAME", "$(basename %s .c)"))
config.substitutions.append(("%HAKC_TEST_PATH", "$(dirname %s)"))
config.substitutions.append(("%HAKC_ROOT", "$(realpath " + config.test_source_root + "/../../../../)"))

# Default test suffixes.
config.suffixes = [".c", ".cpp"]
