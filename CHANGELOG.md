
# DASH 0.2.0 (2016-02-28)

## Build System

Features:

- Added support for ScaLAPACK.
- Added support for MKL.
- Added support for numalib.
- Added support for IPM.
- Added support for PLASMA.
- Added support for hwloc.

- Enabled reporting of compiler warnings (`-Wall -pedantic`).

- New compiler flags:

    - `DASH_ENABLE_SCALAPACK`: Whether DASH has been compiled with ScaLAPACK
      support.
    - `DASH_ENABLE_MKL`: Whether DASH has been compiled with Intel MKL
      support.
    - `DASH_ENABLE_NUMA`: Whether DASH has been compiled with numalib support.
    - `DASH_ENABLE_IPM`: Whether DASH has been compiled with support for
      run-time calls of IPM functions.
    - `DASH_ENABLE_PLASMA`: Whether DASH has been compiled with support for
      the PLASMA linear algebra library.

  New compiler flags specific to DART-MPI, also available in DASH when built
  with DART-MPI backend:

    - `DART_MPI_IMPL_ID`: Identification string of the linked MPI
      implementation.
    - `DART_MPI_IS_INTEL`: Defined if DART-MPI uses Intel MPI.
    - `DART_MPI_IS_MPICH`: Defined if DART-MPI uses MPICH, including IBM MPI.
    - `DART_MPI_IS_MVAPICH`: Defined if DART-MPI uses MVAPICH.
    - `DART_MPI_IS_OPENMPI`: Defined if DART-MPI uses OpenMPI.

Bugfixes:

- Fixed compiler flags for Cray compiler.

## DART Interface

Features:

- Added type `dart_operation_t` for specification of reduce operations.
- Added type `dart_datatype_t` for portable specification of data types.

Bugfixes:

- Fixed compiler warnings.

## DART-MPI

Features:

- Added logging.

Bugfixes:

- Fixed missing deallocation of shared memory segments when MPI shared windows
  are disabled.
- Reduced MPI calls in rank queries.
- Numerous stability improvements, added checks of parameters and return
  values of MPI calls.
- Fixed compiler warnings.

## DASH

- Added documentation of container concepts.

Features:

- Added `dash::make_team_spec` providing automatic cartesian arrangement of
  units optimizing communication performance based on pattern properties.
- Added `dash::ShiftTilePattern` (previously `dash::TilePattern`).
- Added `dash::TilePattern`.
- Added algorithms `dash::fill` and `dash::generate`.
- Added `dash::GlobViewIter` for specifying and iterating multi-dimensional
  views e.g. on matrices.
- Added benchmark application `bench.10.summa` for evaluation of
  `dash::multiply<dash::Matrix>`.
- Added benchmark application `bench.07.local-copy` for evaluation of
  `dash::copy` (global-to-local copy operations).
- Extended GUPS benchmark application `bench.03.gups`.
- Extended micro-benchmark `bench.09.multi-reader`.
- Added automatic balancing of process grid extents in `dash::TeamSpec`,
  optimizing for surface-to-volume ratio.
- Added tool `dash::tools::PatternVisualizer` to visualize data distributions
  in SVG graphics.
- Added log helper `print_pattern_mapping` to visualize data distributions in
  log output.
- Significantly improved performance of `dash::summa`.
- Added support for irregular blocked distributions (underfilled blocks) for
  `dash::Array`.
- Added `dash::util::Locality`, a minimalistic helper class for obtaining
  basic locality information like NUMA domains and CPU pinning.
- Added utility class `dash::util::BenchmarkParams` for printing benchmark
  parameters, DASH configuration and environment settings in measurements
  preamble.
- Improved efficiency of `dash::copy` for local-to-local copying.

Bugfixes:

- Fixed compiler errors for ICC 15.
- Fixed order of coordinate indices in patterns and `dash::Matrix`.
- Fixed definition of assertion macros when configured with runtime
  assertions disabled.
- Added barrier in `dash::Array.deallocate()` to prevent partitions of a
  global array to be freed when going out of scope while other units might
  still attempt to access it.
- Fixed compiler warnings.
- Fixed `dash::copy` for huge ranges.
