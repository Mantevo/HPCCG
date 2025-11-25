
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

/////////////////////////////////////////////////////////////////////////

// Routine to compute matrix vector product y = Ax where:
// First call exchange_externals to get off-processor values of x

// A - known matrix 
// x - known vector
// y - On exit contains Ax.

/////////////////////////////////////////////////////////////////////////

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
#include <filesystem>
#include "HPC_sparsemv.hpp"

void HPC_sparsemv( HPC_Sparse_Matrix *A, 
		 const float * const x, float * const y)
{
  // CSR
  // const int nrow = (const int) A->nrow;

  // for (int i=0; i< nrow; i++)
  //   {
  //     float sum = 0.0;
  //     const int begin = A->nnzs[i];
  //     const int end = A->nnzs[i+1];
  //     for (int j=begin; j< end; j++)
  //         sum += A->vals[j]*x[A->inds[j]];
  //     y[i] = sum;
  //   }

  // ELLPACK
  const int nrow = (const int) A->nrow;
  const int ncol_per_row = (const int) A->ellpack_cols;
  for (int i=0; i< nrow; i++)
    {
      float sum = 0.0f;
      for (int j=0; j < ncol_per_row; j++) {
        sum += A->ellpack_vals[i * ncol_per_row + j] * x[A->ellpack_inds[i * ncol_per_row + j]];
      }
      y[i] = sum;
    }
}
