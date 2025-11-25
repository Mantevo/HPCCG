#include "ellpack_matVec.hpp"
#include "hostdevcommon/kernel_structs.h"
#include "tt-metalium/buffer.hpp"
#include "tt-metalium/tt_backend_api_types.hpp"

#include <cstdint>
#include <cstdlib>
#include <tt-metalium/host_api.hpp>
#include <tt-metalium/work_split.hpp>
#include <filesystem>
#include <tt-metalium/kernel_types.hpp>
#include <tt-metalium/tensor_accessor_args.hpp>
#include <tt-metalium/tt_metal.hpp>

#define PAGE_SIZE 1024u

namespace tt::daisy {

// Global device pointer
static tt::tt_metal::IDevice* g_device = nullptr;

// Initialize the global device
static tt::tt_metal::IDevice* get_device() {
    if (g_device == nullptr) {
        g_device = tt_metal::CreateDevice(0);
        // Register cleanup function to run at program exit
        std::atexit([]() {
            if (g_device != nullptr) {
                tt_metal::CloseDevice(g_device);
                g_device = nullptr;
            }
        });
    }
    return g_device;
}

template<typename T>
void tilize_buffer(std::vector<T>& out, const T* in, int rows, int cols, int ellpack_cols, T pad_value) {
    // ellpack_cols is the width of the input matrix (in elements)
    // We assume the input is (rows x ellpack_cols)
    
    int num_tiles_r = (rows + 31) / 32;
    int num_tiles_c = (ellpack_cols + 31) / 32;
    
    out.assign(num_tiles_r * num_tiles_c * 32 * 32, pad_value);
    
    for (int tr = 0; tr < num_tiles_r; tr++) {
        for (int tc = 0; tc < num_tiles_c; tc++) {
            T* tile_start = &out[(tr * num_tiles_c + tc) * 1024];
            
            // Face 0
            for (int r = 0; r < 16; r++) {
                for (int c = 0; c < 16; c++) {
                    int global_r = tr * 32 + r;
                    int global_c = tc * 32 + c;
                    if (global_r < rows && global_c < ellpack_cols) {
                        tile_start[r * 16 + c] = in[global_r * ellpack_cols + global_c];
                    }
                }
            }
            
            // Face 1
            for (int r = 0; r < 16; r++) {
                for (int c = 0; c < 16; c++) {
                    int global_r = tr * 32 + r;
                    int global_c = tc * 32 + 16 + c;
                    if (global_r < rows && global_c < ellpack_cols) {
                        tile_start[256 + r * 16 + c] = in[global_r * ellpack_cols + global_c];
                    }
                }
            }
            
            // Face 2
            for (int r = 0; r < 16; r++) {
                for (int c = 0; c < 16; c++) {
                    int global_r = tr * 32 + 16 + r;
                    int global_c = tc * 32 + c;
                    if (global_r < rows && global_c < ellpack_cols) {
                        tile_start[512 + r * 16 + c] = in[global_r * ellpack_cols + global_c];
                    }
                }
            }
            
            // Face 3
            for (int r = 0; r < 16; r++) {
                for (int c = 0; c < 16; c++) {
                    int global_r = tr * 32 + 16 + r;
                    int global_c = tc * 32 + 16 + c;
                    if (global_r < rows && global_c < ellpack_cols) {
                        tile_start[768 + r * 16 + c] = in[global_r * ellpack_cols + global_c];
                    }
                }
            }
        }
    }
}

#define TT_DEBUG 0

void tt_launch_ellpack_matVecOp(
    tt::tt_metal::IDevice* device,
    uint32_t cell_count,
    uint32_t ellpack_cols,
    std::shared_ptr<tt::tt_metal::Buffer> d_ellpack_vals,
    std::shared_ptr<tt::tt_metal::Buffer> d_ellpack_addrs,
    tt::tt_metal::Buffer& d_inVec,
    tt::tt_metal::Buffer& d_resVec,
    const std::filesystem::path& kernel_dir,
    EllpackHwImpl hwImpl,
    const std::vector<std::pair<uint32_t, uint32_t>>& row_tile_min_max
) {
    // static int invocation = 0;

    // std::cout << "Launching Ellpack MatVec Op (invocation " << invocation++ << ") with hwImpl " << static_cast<int>(hwImpl) << std::endl;

    const bool diag_wb = hwImpl == EllpackHwImpl::FPU;

    tt::tt_metal::Program program;
    // assume 1 tile wide ellpack (in allocation)
    auto ell_used_cols = ellpack_cols;
    auto ell_tiles_total = (cell_count + 31u) / 32u;
    auto data_format = tt::DataFormat::Float32;
    uint32_t ell_tile_page_size = tt::tt_metal::detail::TileSize(data_format);
    int vector_page_size = 1024;
    auto vec_entries_per_chunk = vector_page_size / 4u;
    auto vec_tile_h_per_chunk = vec_entries_per_chunk / 32u;
    auto vec_chunks_total = (ell_tiles_total + vec_tile_h_per_chunk -1) / vec_tile_h_per_chunk; // 1024 byte pages -> 256 floats -> 8 tiles for each vec-chunk

    uint32_t batch_size = diag_wb? 4u : 8u;

    auto batches_total = (ell_tiles_total + batch_size - 1) / batch_size;

    auto avail_cores = device->compute_with_storage_grid_size();

    auto [num_cores, used_cores, core_group_1, core_group_2, work_per_core1, work_per_core2] =
        tt::tt_metal::split_work_to_cores(avail_cores, batches_total);

    #if TT_DEBUG > 0
    std::cout << "Using " << num_cores << " cores to process " << batches_total << " batches, " << batch_size << " tiles each ("
              << work_per_core1 << " on " << core_group_1.num_cores() << ", " << work_per_core2 << " on " << core_group_2.num_cores() << "; " << vec_chunks_total << " vec chunks (" << vec_entries_per_chunk << " floats/chunk))" << std::endl;
    #endif

    auto input_tile_count = batch_size * 2;
    auto vector_chunk_count = 32u; // at least
    auto result_page_count = 4;

    size_t vector_size = vector_page_size * 2;

    // c0 output (vector)
    // c1 input (mat - ellpack data)
    // c2 input (mat - ellpack addr)
    // c3 input (vector)
    // c4 temp (mat - mulmat of vector matched to ellpack tiles)
    tt_metal::CreateCircularBuffer(
        program,
        used_cores,  // create on all cores
        tt_metal::CircularBufferConfig(
            ell_tile_page_size * input_tile_count,
            {
                {CBIndex::c_1, data_format},
            }
        )
        .set_page_size(CBIndex::c_1, ell_tile_page_size)
    );

    tt_metal::CreateCircularBuffer(
        program,
        used_cores,  // create on all cores
        tt_metal::CircularBufferConfig(
            ell_tile_page_size * input_tile_count,
            {
                {CBIndex::c_4, data_format},
            }
        )
        .set_page_size(CBIndex::c_4, ell_tile_page_size)
    );

    tt_metal::CreateCircularBuffer(
        program,
        used_cores,  // create on all cores
        tt_metal::CircularBufferConfig(
            ell_tile_page_size * input_tile_count,
            {
                {CBIndex::c_2, tt::DataFormat::Float32} // we MUST lie, because only then will tt-runtime unlock TF32 support (the host runtime maps the "dest" type, which is used for DST and SrcA/SrcB regs, Tf32 cannot be manually set, but must be chosed as Dest type to not loose precision)
                // tensix driver will use the dest type as is for unpack byte size. And will use it as is for SrcA/SrcB input type (which if FP32 will expect FP16 and therefore misinterpret the unpacked data)
                // CBs throw, if we try to use them with TF32 explicitly, because there is a switch case that does not define a byte-size
                // default mapping will either map FP16 as dest type (by default) or FP32 (if UnpackToDestFp32 is set. No other effects on the host side)
            }
        )
        .set_page_size(CBIndex::c_2, ell_tile_page_size));

    auto res_buf_page_size = (diag_wb? tt_metal::detail::TileSize(data_format) : vector_page_size);
    tt_metal::CreateCircularBuffer(
        program,
        used_cores,  // create on all cores
        tt_metal::CircularBufferConfig(
            res_buf_page_size * result_page_count,
            {
                {CBIndex::c_0, data_format},
            }
        )
        .set_page_size(CBIndex::c_0, res_buf_page_size)
    );

    tt_metal::CreateCircularBuffer(
        program,
        used_cores,  // create on all cores
        tt_metal::CircularBufferConfig(
            vector_page_size * vector_chunk_count,
            {
                {CBIndex::c_3, data_format}
            }
        )
        .set_page_size(CBIndex::c_3, vector_page_size)
    );

    std::vector<uint32_t> rd_compile_args, rd_common_args;
    tt_metal::TensorAccessorArgs(d_ellpack_vals).append_to(rd_compile_args, rd_common_args);
    tt_metal::TensorAccessorArgs(d_ellpack_addrs).append_to(rd_compile_args, rd_common_args);
    tt_metal::TensorAccessorArgs(d_inVec).append_to(rd_compile_args, rd_common_args);
    auto kernel_rd_0 = tt_metal::CreateKernel(
        program,
        kernel_dir / (0? "mat_vec_reader_collection.cpp" : "mat_vec_reader_naive.cpp"),
        used_cores,
        tt_metal::ReaderDataMovementConfig(
            rd_compile_args
        )
    );

    std::vector<uint32_t> wr_compile_args, wr_common_args;
    tt_metal::TensorAccessorArgs(d_resVec).append_to(wr_compile_args, wr_common_args);
    auto kernel_wr_0 = tt_metal::CreateKernel(
        program,
        kernel_dir / (diag_wb? "vec_diag_result_wb.cpp" : "vec_bare_result_wb.cpp"),
        used_cores,
        tt_metal::WriterDataMovementConfig(
            wr_compile_args
        )
    );

    std::vector<UnpackToDestMode> unpack_modes(NUM_CIRCULAR_BUFFERS, UnpackToDestMode::Default);
    // unpack_modes[CBIndex::c_1] = UnpackToDestMode::UnpackToDestFp32;
    // unpack_modes[CBIndex::c_4] = UnpackToDestMode::UnpackToDestFp32;

    auto kernel_comp_0 = tt_metal::CreateKernel(
        program,
        kernel_dir / (diag_wb? "mat_vec_compute_matmul.cpp" : "mat_vec_compute_naive.cpp"),
        used_cores,
        tt_metal::ComputeConfig {
            .math_fidelity = MathFidelity::HiFi4,
            .fp32_dest_acc_en = true,
            // .dst_full_sync_en = false,
            // .unpack_to_dest_mode = unpack_modes,
            // .math_approx_mode = false,
            .compile_args = {},
        }
    );

    rd_common_args.insert(
        rd_common_args.begin(),
        {
            d_ellpack_vals->address(),
            d_ellpack_addrs->address(),
            d_inVec.address(),
            vec_chunks_total,
            1, // vec_chunk_batch_size
        }
    );

    tt_metal::SetCommonRuntimeArgs(
        program,
        kernel_rd_0,
        rd_common_args
    );

    tt_metal::SetCommonRuntimeArgs(
        program,
        kernel_comp_0,
        {
            vec_chunks_total,
            1, // vec_chunk_batch_size
            1 // stream vec
        }
    );

    wr_common_args.insert(
        wr_common_args.begin(),
        {
            d_resVec.address(),
            batch_size,
        }
    );

    tt_metal::SetCommonRuntimeArgs(
        program,
        kernel_wr_0,
        wr_common_args
    );


    uint32_t start_batch = 0;
    uint32_t end_tile = ell_tiles_total; // ex

    for (auto& range : used_cores.ranges()) {

        for (auto& core : range) {
            uint32_t units;
            if (core_group_1.contains(core)) {
                units = work_per_core1;
            } else if (core_group_2.contains(core)) {
                units = work_per_core2;
            } else {
                units = 0;
            }

            auto tiles = units * batch_size;
            auto start_tile = start_batch * batch_size;
            if (start_tile + tiles > end_tile) {
                tiles = end_tile - start_tile;
            }

            uint32_t min_col = UINT32_MAX;
            uint32_t max_col = 0;
            for (uint32_t t = 0; t < tiles; ++t) {
                uint32_t tile_idx = start_tile + t;
                if (tile_idx < row_tile_min_max.size()) {
                    auto [t_min, t_max] = row_tile_min_max[tile_idx];
                    if (t_min < min_col) min_col = t_min;
                    if (t_max > max_col) max_col = t_max;
                }
            }

            uint32_t start_chunk = 0;
            uint32_t num_chunks = vec_chunks_total;

            if (min_col <= max_col) {
                start_chunk = min_col / vec_entries_per_chunk;
                uint32_t end_chunk = max_col / vec_entries_per_chunk;
                num_chunks = end_chunk - start_chunk + 1;
            } else {
                num_chunks = 0;
            }

            tt::tt_metal::SetRuntimeArgs(
                program,
                kernel_rd_0,
                core,
                {
                    start_tile,
                    tiles,
                    units,
                    batch_size,
                    start_chunk,
                    num_chunks
                }
            );

            tt::tt_metal::SetRuntimeArgs(
                program,
                kernel_comp_0,
                core,
                {
                    units,
                    batch_size,
                    tiles,
                    start_chunk,
                    num_chunks
                }
            );

            tt::tt_metal::SetRuntimeArgs(
                program,
                kernel_wr_0,
                core,
                {
                    start_batch,
                    units,
                    start_tile,
                    tiles,
                }
            );

            start_batch += units;
        }
    }

    tt_metal::EnqueueProgram(device->command_queue(0), program, false);
}

void tt_spmv_ellpack(const ELLPACKMatVecParams& params, const float * x, float * y)
{
    _ZN2tt5daisy23tt_ellpack_matVec(
        params.vals,
        params.inds,
        params.nrow,
        params.ncol,
        params.ellpack_nnz,
        params.ellpack_cols,
        params.row_min_cols,
        params.row_max_cols,
        x,
        y
    );
}

}   // namespace tt::daisy

void _ZN2tt5daisy23tt_ellpack_matVec(
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
)
{
    tt::tt_metal::IDevice* device = tt::daisy::get_device();

    // Copy-in vals
    void * d_ellpack_vals = _ZN2tt5daisy23tt_ellpack_matVec_in_0(
        vals,
        inds,
        nrow,
        ncol,
        ellpack_nnz,
        ellpack_cols,
        row_min_cols,
        row_max_cols,
        x,
        y
    );

    // Copy-in inds
    void * d_ellpack_addrs = _ZN2tt5daisy23tt_ellpack_matVec_in_1(
        vals,
        inds,
        nrow,
        ncol,
        ellpack_nnz,
        ellpack_cols,
        row_min_cols,
        row_max_cols,
        x,
        y
    );

    // Copy-in x
    void * d_inVec = _ZN2tt5daisy23tt_ellpack_matVec_in_8(
        vals,
        inds,
        nrow,
        ncol,
        ellpack_nnz,
        ellpack_cols,
        row_min_cols,
        row_max_cols,
        x,
        y
    );

    // Allocate output y
    void * d_resVec = _ZN2tt5daisy23tt_ellpack_matVec_in_9(
        vals,
        inds,
        nrow,
        ncol,
        ellpack_nnz,
        ellpack_cols,
        row_min_cols,
        row_max_cols,
        x,
        y
    );

    // Launch kernel
    _ZN2tt5daisy23tt_ellpack_matVec_kernel(
        vals,
        inds,
        nrow,
        ncol,
        ellpack_nnz,
        ellpack_cols,
        row_min_cols,
        row_max_cols,
        x,
        y,
        d_ellpack_vals,
        d_ellpack_addrs,
        d_inVec,
        d_resVec
    );

    // Copy-out y
    _ZN2tt5daisy23tt_ellpack_matVec_out_9(
        vals,
        inds,
        nrow,
        ncol,
        ellpack_nnz,
        ellpack_cols,
        row_min_cols,
        row_max_cols,
        x,
        y,
        d_resVec
    );

    delete static_cast<std::shared_ptr<tt::tt_metal::Buffer>*>(d_ellpack_vals);
    delete static_cast<std::shared_ptr<tt::tt_metal::Buffer>*>(d_ellpack_addrs);
    delete static_cast<std::shared_ptr<tt::tt_metal::Buffer>*>(d_inVec);
    delete static_cast<std::shared_ptr<tt::tt_metal::Buffer>*>(d_resVec);
}

std::shared_ptr<tt::tt_metal::Buffer> _ZN2tt5daisy23tt_ellpack_matVec_in_0_impl(
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
)
{
  tt::tt_metal::IDevice* device = tt::daisy::get_device();
  
  std::vector<float> tilized_vals;
  tt::daisy::tilize_buffer(tilized_vals, vals, nrow, ellpack_cols, ellpack_cols, 0.0f);
  
  size_t vals_buffer_size = tilized_vals.size() * sizeof(float);
  size_t vals_tile_size = tt::tt_metal::detail::TileSize(tt::DataFormat::Float32);
  
  std::shared_ptr<tt::tt_metal::Buffer> d_ellpack_vals;
  if (vals_buffer_size > vals_tile_size) {
    size_t aligned_vals_size = ((vals_buffer_size + vals_tile_size - 1) / vals_tile_size) * vals_tile_size;
    d_ellpack_vals = tt::tt_metal::CreateBuffer(tt::tt_metal::InterleavedBufferConfig{
      .device = device,
      .size = aligned_vals_size,
      .page_size = vals_tile_size,
      .buffer_type = tt::tt_metal::BufferType::DRAM
    });
  } else {
    d_ellpack_vals = tt::tt_metal::CreateBuffer(tt::tt_metal::BufferConfig{
      .device = device,
      .size = vals_buffer_size,
      .page_size = vals_buffer_size,
      .buffer_type = tt::tt_metal::BufferType::DRAM
    });
  }

  tt::tt_metal::EnqueueWriteBuffer(
    device->command_queue(0),
    d_ellpack_vals,
    tilized_vals.data(),
    true
  );

  return d_ellpack_vals;
}

void * _ZN2tt5daisy23tt_ellpack_matVec_in_0(
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
)
{
    return new std::shared_ptr<tt::tt_metal::Buffer>(_ZN2tt5daisy23tt_ellpack_matVec_in_0_impl(
        vals,
        inds,
        nrow,
        ncol,
        ellpack_nnz,
        ellpack_cols,
        row_min_cols,
        row_max_cols,
        x,
        y
    ));
}

std::shared_ptr<tt::tt_metal::Buffer> _ZN2tt5daisy23tt_ellpack_matVec_in_1_impl(
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
)
{
  tt::tt_metal::IDevice* device = tt::daisy::get_device();
  
  std::vector<uint32_t> tilized_inds;
  tt::daisy::tilize_buffer(tilized_inds, (const uint32_t*)inds, nrow, ellpack_cols, ellpack_cols, (uint32_t)UINT32_MAX);
  
  size_t addrs_buffer_size = tilized_inds.size() * sizeof(uint32_t);
  size_t addrs_tile_size = tt::tt_metal::detail::TileSize(tt::DataFormat::UInt32);
  
  std::shared_ptr<tt::tt_metal::Buffer> d_ellpack_addrs;
  if (addrs_buffer_size > addrs_tile_size) {
    size_t aligned_addrs_size = ((addrs_buffer_size + addrs_tile_size - 1) / addrs_tile_size) * addrs_tile_size;
    d_ellpack_addrs = tt::tt_metal::CreateBuffer(tt::tt_metal::InterleavedBufferConfig{
      .device = device,
      .size = aligned_addrs_size,
      .page_size = addrs_tile_size,
      .buffer_type = tt::tt_metal::BufferType::DRAM
    });
  } else {
    d_ellpack_addrs = tt::tt_metal::CreateBuffer(tt::tt_metal::BufferConfig{
      .device = device,
      .size = addrs_buffer_size,
      .page_size = addrs_buffer_size,
      .buffer_type = tt::tt_metal::BufferType::DRAM
    });
  }

  tt::tt_metal::EnqueueWriteBuffer(
    device->command_queue(0),
    d_ellpack_addrs,
    tilized_inds.data(),
    false
  );

  return d_ellpack_addrs;
}

void * _ZN2tt5daisy23tt_ellpack_matVec_in_1(
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
)
{
    return new std::shared_ptr<tt::tt_metal::Buffer>(_ZN2tt5daisy23tt_ellpack_matVec_in_1_impl(
        vals,
        inds,
        nrow,
        ncol,
        ellpack_nnz,
        ellpack_cols,
        row_min_cols,
        row_max_cols,
        x,
        y
    ));
}

std::shared_ptr<tt::tt_metal::Buffer> _ZN2tt5daisy23tt_ellpack_matVec_in_8_impl(
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
)
{
  tt::tt_metal::IDevice* device = tt::daisy::get_device();
  
  // Create buffers for input and output vectors
  size_t inVec_buffer_size = sizeof(float) * ncol;
  
  std::shared_ptr<tt::tt_metal::Buffer> d_inVec;
  if (inVec_buffer_size > PAGE_SIZE) {
    // Align buffer size to be divisible by page size for interleaved buffers
    size_t aligned_inVec_size = ((inVec_buffer_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    d_inVec = tt::tt_metal::CreateBuffer(tt::tt_metal::InterleavedBufferConfig{
      .device = device,
      .size = aligned_inVec_size,
      .page_size = PAGE_SIZE,
      .buffer_type = tt::tt_metal::BufferType::DRAM
    });
  } else {
    d_inVec = tt::tt_metal::CreateBuffer(tt::tt_metal::BufferConfig{
      .device = device,
      .size = inVec_buffer_size,
      .page_size = inVec_buffer_size,
      .buffer_type = tt::tt_metal::BufferType::DRAM
    });
  }

  tt::tt_metal::EnqueueWriteBuffer(
    device->command_queue(0),
    d_inVec,
    x,
    false
  );

  return d_inVec;
}

void * _ZN2tt5daisy23tt_ellpack_matVec_in_8(
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
)
{
    return new std::shared_ptr<tt::tt_metal::Buffer>(_ZN2tt5daisy23tt_ellpack_matVec_in_8_impl(
        vals,
        inds,
        nrow,
        ncol,
        ellpack_nnz,
        ellpack_cols,
        row_min_cols,
        row_max_cols,
        x,
        y
    ));
}

std::shared_ptr<tt::tt_metal::Buffer> _ZN2tt5daisy23tt_ellpack_matVec_in_9_impl(
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
)
{
  tt::tt_metal::IDevice* device = tt::daisy::get_device();
  
  size_t resVec_buffer_size = sizeof(float) * nrow;
  
  std::shared_ptr<tt::tt_metal::Buffer> d_resVec;
  if (resVec_buffer_size > PAGE_SIZE) {
    // Align buffer size to be divisible by page size for interleaved buffers
    size_t aligned_resVec_size = ((resVec_buffer_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    d_resVec = tt::tt_metal::CreateBuffer(tt::tt_metal::InterleavedBufferConfig{
      .device = device,
      .size = aligned_resVec_size,
      .page_size = PAGE_SIZE,
      .buffer_type = tt::tt_metal::BufferType::DRAM
    });
  } else {
    d_resVec = tt::tt_metal::CreateBuffer(tt::tt_metal::BufferConfig{
      .device = device,
      .size = resVec_buffer_size,
      .page_size = resVec_buffer_size,
      .buffer_type = tt::tt_metal::BufferType::DRAM
    });
  } 

  return d_resVec;
}

void * _ZN2tt5daisy23tt_ellpack_matVec_in_9(
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
)
{
    return new std::shared_ptr<tt::tt_metal::Buffer>(_ZN2tt5daisy23tt_ellpack_matVec_in_9_impl(
        vals,
        inds,
        nrow,
        ncol,
        ellpack_nnz,
        ellpack_cols,
        row_min_cols,
        row_max_cols,
        x,
        y
    ));
}

void _ZN2tt5daisy23tt_ellpack_matVec_kernel(
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
    void * d_ellpack_vals_ptr,
    void * d_ellpack_addrs_ptr,
    void * d_inVec_ptr,
    void * d_resVec_ptr
)
{
    auto d_ellpack_vals = *static_cast<std::shared_ptr<tt::tt_metal::Buffer>*>(d_ellpack_vals_ptr);
    auto d_ellpack_addrs = *static_cast<std::shared_ptr<tt::tt_metal::Buffer>*>(d_ellpack_addrs_ptr);
    auto d_inVec = *static_cast<std::shared_ptr<tt::tt_metal::Buffer>*>(d_inVec_ptr);
    auto d_resVec = *static_cast<std::shared_ptr<tt::tt_metal::Buffer>*>(d_resVec_ptr);

    tt::tt_metal::IDevice* device = tt::daisy::get_device();
  
    int num_tiles_r = (nrow + 31) / 32;
    std::vector<std::pair<uint32_t, uint32_t>> row_tile_min_max(num_tiles_r);

    for (int tr = 0; tr < num_tiles_r; ++tr) {
        uint32_t min_val = UINT32_MAX;
        uint32_t max_val = 0;
        for (int r = 0; r < 32; ++r) {
            int global_r = tr * 32 + r;
            if (global_r >= nrow) break;
            uint32_t row_min = row_min_cols[global_r];
            uint32_t row_max = row_max_cols[global_r];
            if (row_min < min_val) min_val = row_min;
            if (row_max > max_val) max_val = row_max;
        }
        row_tile_min_max[tr] = {min_val, max_val};
    }

    // Launch the matrix-vector multiplication kernel
    auto e = std::getenv("TT_HPCCG_KERNEL_DIR");
    std::filesystem::path kernel_dir = std::filesystem::path(e);
    tt::daisy::tt_launch_ellpack_matVecOp(
        device,
        nrow,  // cell_count should be number of rows
        ellpack_cols,   // ellpack_cols passed from caller
        d_ellpack_vals,
        d_ellpack_addrs,
        *d_inVec,
        *d_resVec,
        kernel_dir,  // kernel directory
        tt::daisy::EllpackHwImpl::FPU,  // hardware implementation type
        row_tile_min_max
    );

}

void _ZN2tt5daisy23tt_ellpack_matVec_out_9(
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
    void * d_resVec_ptr
)
{
    auto d_resVec = *static_cast<std::shared_ptr<tt::tt_metal::Buffer>*>(d_resVec_ptr);
    tt::tt_metal::IDevice* device = tt::daisy::get_device();

    tt::tt_metal::EnqueueReadBuffer(
        device->command_queue(0),
        d_resVec,
        y,
        false
    );

    tt::tt_metal::Finish(device->command_queue(0)); 
}

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
) {};

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
) {};

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
) {};
