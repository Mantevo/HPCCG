#@HEADER
# ************************************************************************
# 
#               HPCCG: Simple Conjugate Gradient Benchmark Code
#                 Copyright (2006) Sandia Corporation
# 
# Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
# license for use of this work by or on behalf of the U.S. Government.
# 
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#  
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#  
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA
# Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
# 
# ************************************************************************
#@HEADER


# Simple hand-tuned makefile.  Modify as necessary for your environment.
# Questions? Contact Mike Heroux (maherou@sandia.gov).
#

#
# 0) Specify compiler and linker:

CXX=/usr/local/bin/g++
LINKER=/usr/local/bin/g++
#CXX=mpicxx
#LINKER=mpicxx


# 1) Build with MPI or not?
#    If you want to run the program with MPI, make sure USE_MPI is set 
#    to -DUSING_MPI

USE_MPI =
#USE_MPI = -DUSING_MPI


# 2) MPI headers:  
#    If you:
#    - Are building MPI mode (-DUSING_MPI is set above).
#    - Do not have the MPI headers installed in a default search directory and
#    - Are not using MPI compiler wrappers
#    Then specify the path to your MPI header file (include a -I)

#MPI_INC = -I/usr/MPICH/SDK.gcc/include


# 3) Specify C++ compiler optimization flags (if any)
#    Typically some reasonably high level of optimization should be used to 
#    enhance performance.

#IA32 with GCC: 
#CPP_OPT_FLAGS = -O3 -funroll-all-loops -malign-double
CPP_OPT_FLAGS = -O3 -ftree-vectorize -ftree-vectorizer-verbose=2

#
# 4) MPI library:
#    If you:
#    - Are building MPI mode (-DUSING_MPI is set above).
#    - Do not have the MPI library installed a default search directory and
#    - Are not using MPI compiler wrappers for linking
#    Then specify the path to your MPI library (include -L and -l directives)

#MPI_LIB = -L/usr/MPICH/SDK.gcc/lib -lmpich

#
# 5) Build with OpenMP or not?
#    If you want to run the program with OpenMP, make sure USING_OMP is set
#    to -DUSING_OMP

USE_OMP = 
#USE_OMP = -DUSING_OMP

#
# 6) OpenMP Compiler argument
#    GCC and Intel compilers require -fopenmp and -openmp, resp.  Other compilers may differ.

#OMP_FLAGS = -fopenmp
#OMP_FLAGS = -openmp

#
# 7) System libraries: (May need to add -lg2c before -lm)

SYS_LIB =-lm

#
# 6) Specify name if executable (optional):

TARGET = test_HPCCG

################### Derived Quantities (no modification required) ##############

CXXFLAGS= $(CPP_OPT_FLAGS) $(OMP_FLAGS) $(USE_OMP) $(USE_MPI) $(MPI_INC)

LIB_PATHS= $(SYS_LIB)

TEST_CPP = main.cpp generate_matrix.cpp read_HPC_row.cpp \
	  compute_residual.cpp mytimer.cpp dump_matlab_matrix.cpp \
          HPC_sparsemv.cpp HPCCG.cpp waxpby.cpp ddot.cpp \
          make_local_matrix.cpp exchange_externals.cpp \
          YAML_Element.cpp YAML_Doc.cpp

TEST_OBJ          = $(TEST_CPP:.cpp=.o)

$(TARGET): $(TEST_OBJ)
	$(LINKER) $(CPP_OPT_FLAGS) $(OMP_FLAGS) $(TEST_OBJ) $(LIB_PATHS) -o $(TARGET)

test:
	@echo "Not implemented yet..."

clean:
	@rm -f *.o  *~ $(TARGET) $(TARGET).exe test_HPCPCG 
