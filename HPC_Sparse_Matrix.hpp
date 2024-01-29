
//@HEADER
// ************************************************************************
// 
//               HPCCG: Simple Conjugate Gradient Benchmark Code
//                 Copyright (2006) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// BSD 3-Clause License
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ************************************************************************
//@HEADER

#ifndef HPC_SPARSE_MATRIX_H
#define HPC_SPARSE_MATRIX_H

// These constants are upper bounds that might need to be changes for 
// pathological matrices, e.g., those with nearly dense rows/columns.

const int max_external = 100000;
const int max_num_messages = 500;
const int max_num_neighbors = max_num_messages;

/**
 * A structure representing a sparse matrix stored in compressed-sparse column
 * (CSC) form.
 */
struct HPC_Sparse_Matrix_STRUCT {
  char *title;  /**< The title of the sparse matrix. */
  int start_row;  /**< The start row within the entire matrix (always `0` in serial mode). */
  int stop_row;  /**< The start row within the entire matrix (always `total_nrow-1` in serial mode). */
  int total_nrow; /**< The number of rows of the entire matrix. */
  long long total_nnz;  /**< The number of non-zeroes in the entire matrix. */
  int local_nrow;  /**< The number of rows in the local slice of the matrix (always `total_nrow` in serial mode). */
  int local_ncol; /**< Must be defined in make_local_matrix (unused and defaults to `local_nrow` in serial mode). */
  int local_nnz;  /**< The number of non-zeroes in the local slice of the matrix (always `total_nnz` in serial mode). */
  int *nnz_in_row;  /**< An array of length `local_nrow` containing the number of non-zeroes in each row. */
  double **ptr_to_vals_in_row;  /**< An array of pointers to the starts of slices in `list_of_vals` for each row. */
  int **ptr_to_inds_in_row;  /**< An array of pointers to the starts of slices in `list_of_inds` for each row. */
  double **ptr_to_diags;    /**< An array of pointers to the starts of slices in `list_of_vals`. */

#ifdef USING_MPI
  int num_external;  /**< The number of neighbouring. */
  int num_send_neighbors;  /**< The number of neighbouring MPI ranks to send data to. */
  int *external_index;  /**< A list of external local indices. */
  int *external_local_index;  /**< A list of external local indices. */
  int total_to_be_sent;  /**< The total data to be sent to other MPI ranks. */
  int *elements_to_send;  /**< A list of data elements to send to other MPI ranks. */
  int *neighbors;  /**< A list of adjacent rank numbers. */
  int *recv_length;   /**< A list of number of elements to receive from each neighbouring rank. */
  int *send_length;  /**< A list of number of elements to send to each neighbouring rank. */
  double *send_buffer;  /**< A buffer containing the data to send to other MPI ranks. */
#endif

  // needed for cleaning up memory
  double *list_of_vals;  /**< An array of non-zero values in the sparse matrix. */
  int *list_of_inds;  /**< An array of the indices in rows of non-zero values in the sparse matrix.*/
};
typedef struct HPC_Sparse_Matrix_STRUCT HPC_Sparse_Matrix;


void destroyMatrix(HPC_Sparse_Matrix * &A);

#ifdef USING_SHAREDMEM_MPI
#ifndef SHAREDMEM_ALTERNATIVE
void destroySharedMemMatrix(HPC_Sparse_Matrix * &A);
#endif
#endif

#endif
