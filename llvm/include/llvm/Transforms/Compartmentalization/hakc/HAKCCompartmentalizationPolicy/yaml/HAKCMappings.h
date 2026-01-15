//
// Created by de29664 on 8/7/24.
//

#ifndef HAKC_HAKCMAPPINGS_H
#define HAKC_HAKCMAPPINGS_H

LLVM_YAML_IS_SEQUENCE_VECTOR(llvm::hakc::HAKCYamlCompartment)
LLVM_YAML_IS_SEQUENCE_VECTOR(llvm::hakc::HAKCYamlClique)
LLVM_YAML_IS_SEQUENCE_VECTOR(llvm::hakc::HAKCYamlSymbol)

template <> struct yaml::ScalarEnumerationTraits<hakc::hakc_scope_t> {
  static void enumeration(yaml::IO &io, hakc::hakc_scope_t &value) {
    io.enumCase(value, "global", hakc::hakc_global_scope);
    io.enumCase(value, "local", hakc::hakc_local_scope);
  }
};

template <> struct llvm::yaml::MappingTraits<llvm::hakc::HAKCYamlType> {
  static void mapping(yaml::IO &io, hakc::HAKCYamlType &Type) {
    io.mapOptional("debug_type", Type.DebugType);
    io.mapOptional("llvm_type", Type.LLVMType);
  }
};

template <> struct llvm::yaml::MappingTraits<llvm::hakc::HAKCYamlScope> {
  static void mapping(yaml::IO &io, hakc::HAKCYamlScope &Scope) {
    io.mapOptional("local_scope_name", Scope.LocalScope, "");
    io.mapRequired("scope", Scope.Scope);
  }
};

template <> struct yaml::MappingTraits<llvm::hakc::HAKCYamlClique> {
  static void mapping(yaml::IO &io, hakc::HAKCYamlClique &Clique) {
    io.mapRequired("access_token", Clique.AccessToken);
    io.mapRequired("division_id", Clique.DivisionID);
  }
};

template <> struct yaml::MappingTraits<llvm::hakc::HAKCYamlCompartment> {
  static void mapping(yaml::IO &io, hakc::HAKCYamlCompartment &Compartment) {
    io.mapRequired("divisions", Compartment.Cliques);
    io.mapRequired("compartment_id", Compartment.CompartmentID);
    io.mapRequired("targets", Compartment.Targets);
    io.mapRequired("entry_token", Compartment.EntryToken);
  }
};

template <>
struct yaml::MappingTraits<llvm::hakc::HAKCYamlCompartmentalizationPolicy> {
  static void mapping(yaml::IO &io,
                      hakc::HAKCYamlCompartmentalizationPolicy &YamlPolicy) {
    io.mapRequired("COMPARTMENTS", YamlPolicy.Compartments);
    io.mapRequired("SYMBOLS", YamlPolicy.Symbols);
  }
};

template <> struct yaml::MappingTraits<llvm::hakc::HAKCYamlSymbol> {
  static void mapping(yaml::IO &io, hakc::HAKCYamlSymbol &Symbol) {
    io.mapRequired("compartment_id", Symbol.CompartmentID);
    io.mapRequired("division_id", Symbol.DivisionID);
    io.mapRequired("definition", Symbol.Definition);
    io.mapRequired("name", Symbol.Name);
    io.mapRequired("scope", Symbol.Scope);
    io.mapRequired("type", Symbol.Type);
    io.mapRequired("compilation_units", Symbol.CompilationUnits);
  }
};

#endif // HAKC_HAKCMAPPINGS_H
