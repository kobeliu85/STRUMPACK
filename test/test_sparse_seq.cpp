/*
 * STRUMPACK -- STRUctured Matrices PACKage, Copyright (c) 2014, The Regents of
 * the University of California, through Lawrence Berkeley National Laboratory
 * (subject to receipt of any required approvals from the U.S. Dept. of Energy).
 * All rights reserved.
 *
 * If you have questions about your rights to use or distribute this software,
 * please contact Berkeley Lab's Technology Transfer Department at TTD@lbl.gov.
 *
 * NOTICE. This software is owned by the U.S. Department of Energy. As such, the
 * U.S. Government has been granted for itself and others acting on its behalf a
 * paid-up, nonexclusive, irrevocable, worldwide license in the Software to
 * reproduce, prepare derivative works, and perform publicly and display publicly.
 * Beginning five (5) years after the date permission to assert copyright is
 * obtained from the U.S. Department of Energy, and subject to any subsequent five
 * (5) year renewals, the U.S. Government is granted for itself and others acting
 * on its behalf a paid-up, nonexclusive, irrevocable, worldwide license in the
 * Software to reproduce, prepare derivative works, distribute copies to the
 * public, perform publicly and display publicly, and to permit others to do so.
 *
 * Developers: Pieter Ghysels, Francois-Henry Rouet, Xiaoye S. Li.
 *             (Lawrence Berkeley National Lab, Computational Research Division).
 *
 */
#include <iostream>
#include "StrumpackSparseSolver.hpp"
#include "CSRMatrix.hpp"
#include "CSCMatrix.hpp"

using namespace strumpack;

#define SOLVE_TOLERANCE 1e-12

template<typename scalar_t,typename integer_t> int
test(int argc, char* argv[], CSRMatrix<scalar_t,integer_t>& A) {
  StrumpackSparseSolver<scalar_t,integer_t> spss;
  spss.options().set_from_command_line(argc, argv);

  TaskTimer::t_begin = GET_TIME_NOW();

  int N = A.size();
  std::vector<scalar_t> b(N), x(N), x_exact(N, scalar_t(1.)/std::sqrt(N));
  A.omp_spmv(x_exact.data(), b.data());

  spss.set_matrix(A);
  if (spss.reorder() != ReturnCode::SUCCESS) {
    std::cout << "problem with reordering of the matrix." << std::endl;
    return 1;
  }
  if (spss.factor() != ReturnCode::SUCCESS) {
    std::cout << "problem during factorization of the matrix." << std::endl;
    return 1;
  }
  spss.solve(b.data(), x.data());

  auto comp_scal_res = A.max_scaled_residual(x.data(), b.data());
  std::cout << "# COMPONENTWISE SCALED RESIDUAL = " << comp_scal_res << std::endl;

  blas::axpy(N, scalar_t(-1.), x_exact.data(), 1, x.data(), 1);
  auto nrm_error = blas::nrm2(N, x.data(), 1);
  auto nrm_x_exact = blas::nrm2(N, x_exact.data(), 1);
  std::cout << "# RELATIVE ERROR = " << (nrm_error/nrm_x_exact) << std::endl;

  if (comp_scal_res > SOLVE_TOLERANCE) return 1;
  else return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Solve a linear system with a matrix given in matrix market format" << std::endl
	      << "using the sequential/multithreaded C++ STRUMPACK interface."
	      << std::endl << std::endl
	      << "Usage: \n\t./testMMdouble pde900.mtx" << std::endl;
    return 1;
  }
  std::string f(argv[1]);

  std::cout << "# Running with:\n# ";
#if defined(_OPENMP)
  std::cout << "OMP_NUM_THREADS=" << omp_get_max_threads() << " ";
#endif
  for (int i=0; i<argc; i++) std::cout << argv[i] << " ";
  std::cout << std::endl;

  CSRMatrix<double,int> A;
  if (A.read_matrix_market(f) == 0)
    return test<double,int>(argc, argv, A);
  else {
    CSRMatrix<std::complex<double>,int> A;
    A.read_matrix_market(f);
    return test<std::complex<double>,int>(argc, argv, A);
  }
}
