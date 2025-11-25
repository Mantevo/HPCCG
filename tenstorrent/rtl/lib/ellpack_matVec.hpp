#pragma once

#include <cstdint>
#include <memory>

namespace tt {

namespace tt_metal {
    class Buffer;
} // namespace tt_metal

namespace daisy {

enum class EllpackHwImpl {
    None = 0,
    FPU = 1,
    SFPU = 2
};

/**
 * @brief Parameters for ELLPACK matrix-vector multiplication
 */
struct ELLPACKMatVecParams {
    // Data pointers
    const float * vals;
    const int * inds;

    // Matrix dimensions and properties
    int nrow;
    int ncol;
    int ellpack_nnz;
    int ellpack_cols;

    // Input vector properties
    const uint32_t* row_min_cols;
    const uint32_t* row_max_cols;

    // Hardware implementation to use
    EllpackHwImpl hw_impl = EllpackHwImpl::FPU;
};

/**
 * @brief Perform ELLPACK sparse matrix-vector multiplication: y = A * x
 * 
 * @param params Parameters for the ELLPACK matrix-vector multiplication
 * @param x Input vector
 * @param y Output vector
 */
void tt_spmv_ellpack(const ELLPACKMatVecParams& params, const float * x, float * y);

}   // namespace daisy
}   // namespace tt

/***** Functions for Linker Opt ******/

extern "C" void _ZN2tt5daisy23tt_ellpack_matVec(
    const float * vals,
    const int * inds,
    int nrow,
    int ncol,
    int ellpack_nnz,
    int ellpack_cols,
    const uint32_t* row_min_cols,
    const uint32_t* row_max_cols,
    const float * x,
    float * y
);

extern "C" void * _ZN2tt5daisy23tt_ellpack_matVec_in_0(
    const float * vals,
    const int * inds,
    int nrow,
    int ncol,
    int ellpack_nnz,
    int ellpack_cols,
    const uint32_t* row_min_cols,
    const uint32_t* row_max_cols,
    const float * x,
    float * y
);

extern "C" void * _ZN2tt5daisy23tt_ellpack_matVec_in_1(
    const float * vals,
    const int * inds,
    int nrow,
    int ncol,
    int ellpack_nnz,
    int ellpack_cols,
    const uint32_t* row_min_cols,
    const uint32_t* row_max_cols,
    const float * x,
    float * y
);

extern "C" void * _ZN2tt5daisy23tt_ellpack_matVec_in_8(
    const float * vals,
    const int * inds,
    int nrow,
    int ncol,
    int ellpack_nnz,
    int ellpack_cols,
    const uint32_t* row_min_cols,
    const uint32_t* row_max_cols,
    const float * x,
    float * y
);

extern "C" void * _ZN2tt5daisy23tt_ellpack_matVec_in_9(
    const float * vals,
    const int * inds,
    int nrow,
    int ncol,
    int ellpack_nnz,
    int ellpack_cols,
    const uint32_t* row_min_cols,
    const uint32_t* row_max_cols,
    const float * x,
    float * y
);

extern "C" void _ZN2tt5daisy23tt_ellpack_matVec_kernel(
    const float * vals,
    const int * inds,
    int nrow,
    int ncol,
    int ellpack_nnz,
    int ellpack_cols,
    const uint32_t* row_min_cols,
    const uint32_t* row_max_cols,
    const float * x,
    float * y,
    void * d_ellpack_vals,
    void * d_ellpack_addrs,
    void * d_inVec,
    void * d_resVec
);

extern "C" void _ZN2tt5daisy23tt_ellpack_matVec_out_0(
    const float * vals,
    const int * inds,
    int nrow,
    int ncol,
    int ellpack_nnz,
    int ellpack_cols,
    const uint32_t* row_min_cols,
    const uint32_t* row_max_cols,
    const float * x,
    float * y,
    void * d_ellpack_vals
);

extern "C" void _ZN2tt5daisy23tt_ellpack_matVec_out_1(
    const float * vals,
    const int * inds,
    int nrow,
    int ncol,
    int ellpack_nnz,
    int ellpack_cols,
    const uint32_t* row_min_cols,
    const uint32_t* row_max_cols,
    const float * x,
    float * y,
    void * d_ellpack_addrs
);

extern "C" void _ZN2tt5daisy23tt_ellpack_matVec_out_8(
    const float * vals,
    const int * inds,
    int nrow,
    int ncol,
    int ellpack_nnz,
    int ellpack_cols,
    const uint32_t* row_min_cols,
    const uint32_t* row_max_cols,
    const float * x,
    float * y,
    void * d_inVec
);

extern "C" void _ZN2tt5daisy23tt_ellpack_matVec_out_9(
    const float * vals,
    const int * inds,
    int nrow,
    int ncol,
    int ellpack_nnz,
    int ellpack_cols,
    const uint32_t* row_min_cols,
    const uint32_t* row_max_cols,
    const float * x,
    float * y,
    void * d_resVec
);
