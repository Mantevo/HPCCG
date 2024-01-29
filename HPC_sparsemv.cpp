
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

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <string>
#include <cmath>
#include "HPC_sparsemv.hpp"

/**
 * A method to compute the matrix-vector product `y = Ax`.
 *
 * @note In MPI, first call exchange_externals to get off-processor values of x.
 *
 * The input matrix to the function is stored in compressed-sparse column form.
 * Hence, `cur_vals` is a pointer to a slice of the `list_of_vals` array, which
 * then contains an ordered array of the non-zero values in the current row.
 * `cur_inds` equivalently is an ordered array of the indices of those values in
 * the current row. `cur_nnz` is the number of non-zeroes in the array (hence
 * the length of both `cur_vals` and `cur_inds` array slices, as the
 * `list_of_vals/inds` arrays are non-homogenous in sub-array length, and don't
 * have termination to split the sub-arrays up).
 *
 * Hence, the matrix-vector multiplication is an array containing the products
 * of those non-zero values with their pairs in the input vector, for each rown
 * in the input matrix and vector.
 *
 * @param A The input sparse matrix.
 * @param x The input vector.
 * @param y A pointer to a vector, which is updated to contain the calculated
 *          product `Ax`.
 * @return An exit code of zero on success.
 */
int HPC_sparsemv( HPC_Sparse_Matrix *A, 
		 const double * const x, double * const y)
{

  const int nrow = (const int) A->local_nrow;

#ifdef USING_OMP
#pragma omp parallel for
#endif
  for (int i=0; i< nrow; i++)
    {
      double sum = 0.0;
      const double * const cur_vals = 
     (const double * const) A->ptr_to_vals_in_row[i];

      const int    * const cur_inds = 
     (const int    * const) A->ptr_to_inds_in_row[i];

      const int cur_nnz = (const int) A->nnz_in_row[i];

      for (int j=0; j< cur_nnz; j++)
          sum += cur_vals[j]*x[cur_inds[j]];
      y[i] = sum;
    }
  return(0);
}
