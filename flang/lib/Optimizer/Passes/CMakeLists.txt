get_property(dialect_libs GLOBAL PROPERTY MLIR_DIALECT_LIBS)
get_property(extension_libs GLOBAL PROPERTY MLIR_EXTENSION_LIBS)

add_flang_library(flangPasses
  CommandLineOpts.cpp
  Pipelines.cpp

  DEPENDS
  FortranCommon

  LINK_LIBS
  FIRCodeGen
  FIRTransforms
  FlangOpenMPTransforms
  FortranCommon
  HLFIRTransforms

  LINK_COMPONENTS
  Passes

  MLIR_LIBS
  ${dialect_libs}
  ${extension_libs}
  MLIRPass
  MLIRReconcileUnrealizedCasts
  MLIRSCFToControlFlow
  MLIRSupport
  MLIRTransforms
)
