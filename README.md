# Case Study: HPCCG mini-app

This repository demonstrates the Daisytuner Optimizing Compiler Collection (DOCC) applied to the High-Performance Computing Conjugate Gradient (HPCCG) mini-app, showcasing advanced auto-parallelization (OpenMP) and auto-offloading capabilities (Tenstorrent/CUDA). DOCC automatically analyzes computational kernels and generates optimized code for each target. The repository includes [Daisy workflows](https://github.com/daisytuner/HPCCG/blob/master/.daisy/hpccg.yml) for comprehensive performance analysis through our [dashboard](https://app.daisytuner.com/daisytuner/HPCCG/runs).

The case study focuses on optimizing three critical conjugate gradient kernels: **SPMV** (Sparse Matrix-Vector Multiplication), **WAXPBY** (vector operations), and **DDOT** (dot product), which are executed iteratively as part of the solver. Each kernel has been optimized by DOCC to exploit parallelism, optimize memory access patterns, and minimize data transfers for offloading targets with dedicated memory.

To ease analysis, we have made the following changes to the original code:

- Existing MPI calls and OpenMP pragmas have been removed.
- The matrix data layout has been changed to ELLPACK, which is more suitable for accelerators.
- The precision has been downgraded from FP64 to FP32.
- Minor code improvements such as removal of unused return types.

## Port to Tenstorrent Wormhole and Blackhole

The Tenstorrent Wormhole and Blackhole are PCIe-based AI accelerator cards.
Each processor contains multiple **tensix cores**, and each tensix core comprises several RISC-V cores that execute instruction pipelines.
In addition, every tensix core integrates both a matrix unit and a vector unit to deliver high-throughput computation.
Floating-point precision varies with the specific hardware units:

- FP32 on the RISC-V cores with soft-float.
- FP32 on the vector unit with special handling of subnormals.
- TensorFloat-32 on the matrix unit.

Since the conjugate gradient method is dominated by sparse matrix–vector multiplication (SPMV), the goal is to evaluate the accelerator’s capabilities for sparse linear algebra while maintaining adequate numerical stability (convergence of solver / residual).
DOCC relies on four main components to port the application:

- A **hand-tuned SPMV implementation** for Tenstorrent using the matrix unit, compiled into a runtime library consumed by the HPCCG application.
- **Einsum (dot-product) detection** and automatic mapping to the vector unit via DOCC.
- **Detection of data-parallel loops (e.g., waxpby)** with naïve code generation targeting the RISC-V cores.
- **Thin-LTO and linker-based optimizations** performed by DOCC to minimize data movement by automatically hoisting transfers outside the HPCCG solver loop.
