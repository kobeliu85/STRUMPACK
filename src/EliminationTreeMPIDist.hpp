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
#ifndef ELIMINATION_TREE_MPI_DIST_HPP
#define ELIMINATION_TREE_MPI_DIST_HPP

#include <iostream>
#include <algorithm>
#include <random>
#include "blas_lapack_wrapper.hpp"
#include "CompressedSparseMatrix.hpp"
#include "ProportionallyDistributedSparseMatrix.hpp"
#include "CSRMatrixMPI.hpp"
#include "MatrixReorderingMPI.hpp"
#include "FrontalMatrixHSS.hpp"
#include "FrontalMatrixDense.hpp"
#include "FrontalMatrixDenseMPI.hpp"
#include "strumpack_parameters.hpp"
#include "Redistribute.hpp"
#include "HSS/HSSPartitionTree.hpp"

namespace strumpack {

  template<typename scalar_t,typename integer_t>
  class EliminationTreeMPIDist : public EliminationTreeMPI<scalar_t,integer_t> {
  public:
    EliminationTreeMPIDist(const SPOptions<scalar_t>& opts, CSRMatrixMPI<scalar_t,integer_t>* A,
			   MatrixReorderingMPI<scalar_t,integer_t>* nd, MPI_Comm comm);
    virtual ~EliminationTreeMPIDist();
    void multifrontal_solve_dist(scalar_t* x, std::vector<integer_t>& dist);

    std::tuple<int,int,int> get_sparse_mapped_destination(integer_t i, integer_t j, bool duplicate_fronts);

  private:
    using F_t = FrontalMatrix<scalar_t,integer_t>;
    using FD_t = FrontalMatrixDense<scalar_t,integer_t>;
    using FHSS_t = FrontalMatrixHSS<scalar_t,integer_t>;
    using FMPI_t = FrontalMatrixMPI<scalar_t,integer_t>;
    using FDMPI_t = FrontalMatrixDenseMPI<scalar_t,integer_t>;
    using FHSSMPI_t = FrontalMatrixHSSMPI<scalar_t,integer_t>;
    using DistM_t = DistributedMatrix<scalar_t>;

    CSRMatrixMPI<scalar_t,integer_t>* _A = nullptr;
    MatrixReorderingMPI<scalar_t,integer_t>* _nd = nullptr;
    ProportionallyDistributedSparseMatrix<scalar_t,integer_t> _Aprop;

    using SepRange = std::pair<integer_t,integer_t>;
    SepRange _local_range;

    /**
     * vector with _A->local_rows() elements, storing for each row
     * which process has the corresponding separator entry
     */
    std::vector<int> _row_owner;
    void get_all_pfronts();
    void find_row_owner();

    struct ParFrontMaster {
      ParFrontMaster() {}
      ParFrontMaster(integer_t _sep_begin, integer_t _sep_end, int _P0, int _P)
	: sep_begin(_sep_begin), sep_end(_sep_end), P0(_P0), P(_P) {}
      integer_t sep_begin; integer_t sep_end; int P0; int P;
    };
    /** _local_pfronts_master stores info on all the fronts for which
	this process is the master */
    std::vector<ParFrontMaster> _local_pfronts_master;

    /** list with all parallel fronts */
    std::vector<ParFrontMaster> _all_pfronts;

    struct ParFrontLocal {
      ParFrontLocal() {}
      ParFrontLocal(FMPI_t* _f, int _P0, int _P) : f(_f), P0(_P0), P(_P) {}
      FMPI_t* f; int P0; int P;
    };
    /** _local_pfronts stores info on all the fronts on which this
	process is active. */
    std::vector<ParFrontLocal> _local_pfronts;

    void symbolic_factorization(std::vector<integer_t>* upd, std::vector<integer_t>& dist_upd,
				float* subtree_work, float& dsep_work);
    void symbolic_factorization_local(integer_t sep, std::vector<integer_t>* upd, float* subtree_work, int depth);

    F_t* proportional_mapping(const SPOptions<scalar_t>& opts, std::vector<integer_t>* upd,
			      std::vector<integer_t>& dist_upd, float* subtree_work, float* dist_subtree_work,
			      integer_t dsep, int P0, int P, int P0_sibling, int P_sibling,
			      MPI_Comm front_comm, bool hss_parent, int level);

    F_t* proportional_mapping_sub_graphs(const SPOptions<scalar_t>& opts, RedistSubTree<integer_t>& tree,
					 integer_t dsep, integer_t sep, int P0, int P, int P0_sibling,
					 int P_sibling, MPI_Comm front_comm, bool hss_parent, int level);

    void communicate_distributed_separator(integer_t dsep, std::vector<integer_t>& dist_upd,
					   integer_t& dsep_begin, integer_t& dsep_end,
					   std::vector<integer_t>& dsep_upd, int P0, int P,
					   int P0_sibling, int P_sibling, int owner, bool use_hss);
    void communicate_distributed_separator_HSS_tree(HSS::HSSPartitionTree& tree, integer_t dsep,
						    int P0, int P, int owner);
  };

  template<typename scalar_t,typename integer_t>
  EliminationTreeMPIDist<scalar_t,integer_t>::EliminationTreeMPIDist
  (const SPOptions<scalar_t>& opts, CSRMatrixMPI<scalar_t,integer_t>* A,
   MatrixReorderingMPI<scalar_t,integer_t>* nd, MPI_Comm comm)
    : EliminationTreeMPI<scalar_t,integer_t>(comm), _A(A), _nd(nd) {
    auto rank = mpi_rank(this->_comm);
    auto P = mpi_nprocs(this->_comm);
    auto local_upd = new std::vector<integer_t>[_nd->local_sep_tree->separators()];
    // every process is responsible for 1 distributed separator, so store only 1 dist_upd
    std::vector<integer_t> dist_upd;
    auto local_subtree_work = new float[_nd->local_sep_tree->separators() + _nd->sep_tree->separators()];
    auto dist_subtree_work = local_subtree_work + _nd->local_sep_tree->separators();

    float dsep_work;
    MPI_Pcontrol(1, "symbolic_factorization");
    symbolic_factorization(local_upd, dist_upd, local_subtree_work, dsep_work);
    MPI_Pcontrol(-1, "symbolic_factorization");

    // communicate dist_subtree_work to everyone
    auto sbuf = new float[2*P];
    // initialize buffer or valgrind will complain about MPI sending uninitialized data
    sbuf[2*rank] = sbuf[2*rank+1] = 0.0;
    for (integer_t dsep=0; dsep<_nd->sep_tree->separators(); dsep++)
      if (rank == _nd->proc_dist_sep[dsep]) {
    	if (_nd->sep_tree->lch()[dsep] == -1) sbuf[2*rank] = local_subtree_work[_nd->local_sep_tree->root()];
	else sbuf[2*rank+1] = dsep_work;
      }
    MPI_Allgather(MPI_IN_PLACE, 2, MPI_FLOAT, sbuf, 2, MPI_FLOAT, this->_comm);
    for (integer_t dsep=0; dsep<_nd->sep_tree->separators(); dsep++)
      dist_subtree_work[dsep] = (_nd->sep_tree->lch()[dsep] == -1) ? sbuf[2*_nd->proc_dist_sep[dsep]]
	: sbuf[2*_nd->proc_dist_sep[dsep]+1];
    delete[] sbuf;

    _local_range = std::make_pair(_A->size(), 0);
    MPI_Pcontrol(1, "proportional_mapping");
    MPI_Comm tree_comm;
    if (P>1) MPI_Comm_dup(this->_comm, &tree_comm);
    else tree_comm = this->_comm;
    this->_etree_root = proportional_mapping(opts, local_upd, dist_upd, local_subtree_work, dist_subtree_work,
					     _nd->sep_tree->root(), 0, P, 0, 0, tree_comm, true, 0);
    MPI_Pcontrol(-1, "proportional_mapping");

    MPI_Pcontrol(1, "block_row_A_to_prop_A");
    if (_local_range.first > _local_range.second)
      _local_range.first = _local_range.second = 0;
    this->subtree_ranges.resize(P);
    MPI_Allgather(&_local_range, sizeof(SepRange), MPI_BYTE,
		  this->subtree_ranges.data(), sizeof(SepRange), MPI_BYTE, this->_comm);
    get_all_pfronts();
    find_row_owner();
    _Aprop.setup(_A, _nd, this, opts.use_HSS());
    MPI_Pcontrol(-1, "block_row_A_to_prop_A");

    delete[] local_upd;
    delete[] local_subtree_work;
    _nd->clear_tree_data();
    _all_pfronts.clear();
    _all_pfronts.shrink_to_fit();
  }

  template<typename scalar_t,typename integer_t>
  EliminationTreeMPIDist<scalar_t,integer_t>::~EliminationTreeMPIDist() {
  }

  /**
   * Figure out on which processor element i,j of the sparse matrix
   * (after symmetric nested dissection permutation) is mapped.  Since
   * we do not have upd[] for all the fronts, it is impossible to
   * figure out the exact rank when the element is part of the F12 or
   * F21 block of a distributed front. So we do some duplication over
   * a column or row of processors in a blacs grid. The return tuple
   * contains <P0, P, dP>, so the value should be send to each (p=P0;
   * p<P0+P; p+=dP)
   */
  template<typename scalar_t,typename integer_t> std::tuple<int,int,int>
  EliminationTreeMPIDist<scalar_t,integer_t>::get_sparse_mapped_destination
  (integer_t i, integer_t j, bool duplicate_fronts) {
    auto P = mpi_nprocs(this->_comm);     // TODO avoid doing this over and over
    for (int p=0; p<P; p++) // local separators
      if ((i >= this->subtree_ranges[p].first && i < this->subtree_ranges[p].second) ||
	  (j >= this->subtree_ranges[p].first && j < this->subtree_ranges[p].second))
	return std::make_tuple(p, 1, 1);

    const auto mb = DistM_t::default_MB;
    const auto nb = DistM_t::default_NB;
    for (auto& f : _all_pfronts) { // distributed separators
      if (i < f.sep_begin || j < f.sep_begin) continue;
      if (i < f.sep_end) {
	if (j < f.sep_end) { // F11
	  if (duplicate_fronts) return std::make_tuple(f.P0, f.P, 1);
	  else {
	    int proc_rows, proc_cols;
	    FDMPI_t::processor_grid(f.P, proc_rows, proc_cols);
	    auto p = f.P0 + (((i-f.sep_begin) / mb) % proc_rows)
	      + (((j-f.sep_begin) / nb) % proc_cols) * proc_rows;
	    return std::make_tuple(p, 1, 1);
	  }
	} else { // F12
	  if (duplicate_fronts) return std::make_tuple(f.P0, f.P, 1);
	  else {
	    int proc_rows, proc_cols;
	    FDMPI_t::processor_grid(f.P, proc_rows, proc_cols);
	    auto p_row = (((i-f.sep_begin) / mb) % proc_rows);
	    return std::make_tuple(f.P0+p_row, proc_rows*proc_cols, proc_rows);
	  }
	}
      } else {
	if (j < f.sep_end) { // F21
	  if (duplicate_fronts) return std::make_tuple(f.P0, f.P, 1);
	  else {
	    int proc_rows, proc_cols;
	    FDMPI_t::processor_grid(f.P, proc_rows, proc_cols);
	    auto p_col = (((j-f.sep_begin) / nb) % proc_cols);
	    return std::make_tuple(f.P0+p_col*proc_rows, proc_rows, 1);
	  }
	}
      }
    }
    assert(false);
    return std::make_tuple(0, P, 1);
  }

  template<typename scalar_t,typename integer_t> void
  EliminationTreeMPIDist<scalar_t,integer_t>::get_all_pfronts() {
    auto P = mpi_nprocs(this->_comm);
    auto nr_pfronts = new integer_t[P];
    integer_t tmp = _local_pfronts_master.size();
    MPI_Allgather(&tmp, 1, mpi_type<integer_t>(), nr_pfronts, 1, mpi_type<integer_t>(), this->_comm);
    integer_t total_pfronts = std::accumulate(nr_pfronts, nr_pfronts+P, 0);
    _all_pfronts.resize(total_pfronts);
    auto rcnts = new int[2*P];
    auto rdispls = rcnts + P;
    rdispls[0] = 0;
    for (int p=0; p<P; p++) rcnts[p] = nr_pfronts[p]*sizeof(ParFrontMaster);
    for (int p=1; p<P; p++) rdispls[p] = rdispls[p-1] + rcnts[p-1];
    delete[] nr_pfronts;
    MPI_Allgatherv(_local_pfronts_master.data(), _local_pfronts_master.size()*sizeof(ParFrontMaster), MPI_BYTE,
		   _all_pfronts.data(), rcnts, rdispls, MPI_BYTE, this->_comm);
    delete[] rcnts;
  }

  /**
   * Every row of the matrix is mapped to one specific proces according
   * to the proportional mapping. This function finds out which process
   * and stores that info in a vector<integer_t> _row_owner of size
   * _A->local_rows().
   *
   * First gather a list of ParFrontMaster structs for all parallel
   * fronts on every processor, by gathering the data stored in
   * local_pfront_master (which keeps only the fronts for which this
   * process is the master).  Then loop over all elements in
   * [dist[rank],dist[rank+1]), and figure out to which process that
   * element belongs, by looking for the rank using the ParFrontMaster
   * list.
   */
  template<typename scalar_t,typename integer_t> void
  EliminationTreeMPIDist<scalar_t,integer_t>::find_row_owner() {
    auto P = mpi_nprocs(this->_comm);
    auto lo = _A->begin_row();
    auto n = _A->end_row() - lo;
    _row_owner.resize(n);
    // TODO is this slow? O(rP)? loops can be reordered
#pragma omp parallel for
    for (integer_t r=0; r<n; r++) {
      auto pr = _nd->perm[r+lo];
      _row_owner[r] = -1;
      for (int p=0; p<P; p++) // local separators
	if (pr >= this->subtree_ranges[p].first && pr < this->subtree_ranges[p].second) {
	  _row_owner[r] = p;
	  break;
	}
      if (_row_owner[r] != -1) continue;
      const auto mb = DistM_t::default_MB;
      for (auto& f : _all_pfronts) { // distributed separators
	if (pr >= f.sep_begin && pr < f.sep_end) {
	  int proc_rows, proc_cols;
	  FDMPI_t::processor_grid(f.P, proc_rows, proc_cols);
	  _row_owner[r] = f.P0 + (((pr-f.sep_begin) / mb) % proc_rows);
	  break;
	}
      }
    }
  }

  template<typename scalar_t,typename integer_t> void
  EliminationTreeMPIDist<scalar_t,integer_t>::multifrontal_solve_dist
  (scalar_t* x, std::vector<integer_t>& dist) {
    auto rank = mpi_rank(this->_comm);
    auto P = mpi_nprocs(this->_comm);
    auto lo = dist[rank];
    auto n = dist[rank+1] - lo;
    auto ibuf = new int[4*P];
    auto scnts = ibuf;
    auto rcnts = ibuf + P;
    auto sdispls = ibuf + 2*P;
    auto rdispls = ibuf + 3*P;
    struct IdxVal { integer_t idx; scalar_t val; };
    auto sbuf = new IdxVal[n];
    // since some C++ pad the struct IdxVal must zero the array or will get valgrind warnings
    // about MPI sending uninitialized data
    memset(sbuf,0,n*sizeof(IdxVal));
    auto pp = new IdxVal*[P];
    std::fill(scnts, scnts+P, 0);
    for (integer_t i=0; i<n; i++) scnts[_row_owner[i]]++;
    sdispls[0] = 0;
    pp[0] = sbuf;
    for (integer_t p=1; p<P; p++) {
      sdispls[p] = sdispls[p-1] + scnts[p-1];
      pp[p] = sbuf + sdispls[p];
    }
    for (integer_t i=0; i<n; i++) {
      auto p = _row_owner[i];
      pp[p]->idx = _nd->perm[i+lo];
      pp[p]->val = x[i];
      pp[p]++;
    }
    for (integer_t p=0; p<P; p++) {
      sdispls[p] *= sizeof(IdxVal); // convert to bytes
      scnts[p] *= sizeof(IdxVal);
    }
    MPI_Alltoall(scnts, 1, mpi_type<int>(), rcnts, 1, mpi_type<int>(), this->_comm);
    rdispls[0] = 0;
    size_t rsize = rcnts[0];
    for (int p=1; p<P; p++) {
      rdispls[p] = rdispls[p-1] + rcnts[p-1];
      rsize += rcnts[p];
    }
    rsize /= sizeof(IdxVal);
    auto rbuf = new IdxVal[rsize];
    MPI_Alltoallv(sbuf, scnts, sdispls, MPI_BYTE, rbuf, rcnts, rdispls, MPI_BYTE, this->_comm);
    auto x_loc = new scalar_t[_local_range.second - _local_range.first];
    auto x_dist = new DistM_t[_local_pfronts.size()];
    for (size_t f=0; f<_local_pfronts.size(); f++)
      x_dist[f] = DistM_t(_local_pfronts[f].f->blacs_context(), _local_pfronts[f].f->dim_sep, 1);
#pragma omp parallel for
    for (size_t i=0; i<rsize; i++) {
      auto r = rbuf[i].idx;
      if (r >= _local_range.first && r < _local_range.second)
	x_loc[r - _local_range.first] = rbuf[i].val;
      else {
	for (size_t f=0; f<_local_pfronts.size(); f++)
	  if (r >= _local_pfronts[f].f->sep_begin && r < _local_pfronts[f].f->sep_end) {
	    x_dist[f].global(r-_local_pfronts[f].f->sep_begin, 0) = rbuf[i].val;
	    break;
	  }
      }
    }

    this->_etree_root->multifrontal_solve(x_loc-_local_range.first, x_dist, this->_wmem);

    std::swap(rbuf, sbuf);
    rcnts = ibuf;
    scnts = ibuf + P;
    rdispls = ibuf + 2*P;
    sdispls = ibuf + 3*P;
    for (integer_t p=0; p<P; p++) pp[p] = sbuf + sdispls[p] / sizeof(IdxVal);
    for (integer_t r=_local_range.first; r<_local_range.second; r++) {
      auto dest = std::upper_bound(dist.begin(), dist.end(), _nd->iperm[r])-dist.begin()-1;
      pp[dest]->idx = _nd->iperm[r];
      pp[dest]->val = x_loc[r-_local_range.first];
      pp[dest]++;
    }
    for (size_t i=0; i<_local_pfronts.size(); i++) {
      if (x_dist[i].lcols() == 0) continue;
      auto parf = _local_pfronts[i];
      for (int r=0; r<x_dist[i].lrows(); r++) {
	auto gr = x_dist[i].rowl2g(r) + parf.f->sep_begin;
	auto dest = std::upper_bound(dist.begin(), dist.end(), _nd->iperm[gr])-dist.begin()-1;
	pp[dest]->idx = _nd->iperm[gr];
	pp[dest]->val = x_dist[i](r, 0);
	pp[dest]++;
      }
    }
    delete[] pp;
    MPI_Alltoallv(sbuf, scnts, sdispls, MPI_BYTE, rbuf, rcnts, rdispls, MPI_BYTE, this->_comm);
    delete[] sbuf;
    delete[] ibuf;
#pragma omp parallel for
    for (integer_t i=0; i<n; i++)
      x[rbuf[i].idx-lo] = rbuf[i].val;
    delete[] rbuf;
    delete[] x_loc;
    delete[] x_dist;
  }

  template<typename scalar_t,typename integer_t> void
  EliminationTreeMPIDist<scalar_t,integer_t>::symbolic_factorization_local
  (integer_t sep, std::vector<integer_t>* upd, float* subtree_work, int depth) {
    auto chl = _nd->local_sep_tree->lch()[sep];
    auto chr = _nd->local_sep_tree->rch()[sep];
    if (depth < params::task_recursion_cutoff_level) {
      if (chl != -1)
#pragma omp task untied default(shared) final(depth >= params::task_recursion_cutoff_level-1) mergeable
	symbolic_factorization_local(chl, upd, subtree_work, depth+1);
      if (chr != -1)
#pragma omp task untied default(shared) final(depth >= params::task_recursion_cutoff_level-1) mergeable
	symbolic_factorization_local(chr, upd, subtree_work, depth+1);
#pragma omp taskwait
    } else {
      if (chl != -1) symbolic_factorization_local(chl, upd, subtree_work, depth);
      if (chr != -1) symbolic_factorization_local(chr, upd, subtree_work, depth);
    }
    auto sep_begin = _nd->local_sep_tree->sizes()[sep]+_nd->sub_graph_range.first;
    auto sep_end = _nd->local_sep_tree->sizes()[sep+1]+_nd->sub_graph_range.first;
    for (integer_t r=_nd->local_sep_tree->sizes()[sep]; r<_nd->local_sep_tree->sizes()[sep+1]; r++) {
      auto ice = _nd->my_sub_graph->get_ind()+_nd->my_sub_graph->get_ptr()[r+1];
      auto icb = std::lower_bound(_nd->my_sub_graph->get_ind()+_nd->my_sub_graph->get_ptr()[r], ice, sep_end);
      auto mid = upd[sep].size();
      std::copy(icb, ice, std::back_inserter(upd[sep]));
      std::inplace_merge(upd[sep].begin(), upd[sep].begin() + mid, upd[sep].end());
      upd[sep].erase(std::unique(upd[sep].begin(), upd[sep].end()), upd[sep].end());
    }
    if (chl != -1) {
      auto icb = std::lower_bound(upd[chl].begin(), upd[chl].end(), sep_end);
      auto mid = upd[sep].size();
      std::copy(icb, upd[chl].end(), std::back_inserter(upd[sep]));
      std::inplace_merge(upd[sep].begin(), upd[sep].begin() + mid, upd[sep].end());
      upd[sep].erase(std::unique(upd[sep].begin(), upd[sep].end()), upd[sep].end());
    }
    if (chr != -1) {
      auto icb = std::lower_bound(upd[chr].begin(), upd[chr].end(), sep_end);
      auto mid = upd[sep].size();
      std::copy(icb, upd[chr].end(), std::back_inserter(upd[sep]));
      std::inplace_merge(upd[sep].begin(), upd[sep].begin() + mid, upd[sep].end());
      upd[sep].erase(std::unique(upd[sep].begin(), upd[sep].end()), upd[sep].end());
    }
    upd[sep].shrink_to_fit();
    integer_t dim_blk = (sep_end - sep_begin) + upd[sep].size();
    // assume amount of work per front is N^3, work per subtree is work on front plus children
    float wl = (chl != -1) ? subtree_work[chl] : 0.;
    float wr = (chr != -1) ? subtree_work[chr] : 0.;
    subtree_work[sep] = (float(dim_blk)*dim_blk*dim_blk) + wl + wr;
  }

  /**
   * Symbolic factorization:
   *   bottom-up merging of upd indices and work estimate for each subtree
   *     - first do the symbolic factorization for the local subgraph,
   *        this does not require communication
   *     - then symbolic factorization for the distributed separator assigned to this process
   *        receive upd from left and right childs, merge with upd for local distributed separator
   *        send upd to parent
   *        receive work estimate from left and right subtrees
   *        work estimate for distributed separator subtree is dim_blk^3 + left_tree + right_tree
   *        send work estimate for this distributed separator / subtree to parent
   */
  template<typename scalar_t,typename integer_t> void
  EliminationTreeMPIDist<scalar_t,integer_t>::symbolic_factorization
  (std::vector<integer_t>* local_upd, std::vector<integer_t>& dist_upd, float* local_subtree_work, float& dsep_work) {
    _nd->my_sub_graph->sort_rows();
    if (_nd->local_sep_tree->separators() > 0) {
#pragma omp parallel
#pragma omp single
      symbolic_factorization_local(_nd->local_sep_tree->root(), local_upd, local_subtree_work, 0);
    }

    /* initialize dsep_work so valgrind does not complain, as it is not always set below */
    dsep_work = 0.0;
    _nd->my_dist_sep->sort_rows();
    std::vector<MPI_Request> send_req;
    for (integer_t dsep=0; dsep<_nd->sep_tree->separators(); dsep++) {
      // only consider the distributed separator owned by this process: 1 leaf and 1 non-leaf
      if (_nd->proc_dist_sep[dsep] != mpi_rank(this->_comm)) continue;
      auto pa = _nd->sep_tree->pa()[dsep];
      if (pa == -1) continue; // skip the root separator
      auto pa_rank = _nd->proc_dist_sep[pa];
      if (_nd->sep_tree->lch()[dsep] == -1) {
	// leaf of distributed tree is local subgraph for process proc_dist_sep[dsep]
	// local_upd[dsep] was computed above, send it to the parent process proc_dist_sep[_nd->sep_tree->pa()[dsep]]
	// dist_upd is local_upd of the root of the local tree, which is local_upd[this->nbsep-1], or local_upd.back()
	if (_nd->sep_tree->pa()[pa] == -1) continue; // do not send to parent if parent is root
	send_req.emplace_back();
	int tag = (dsep == _nd->sep_tree->lch()[pa]) ? 1 : 2;
	MPI_Isend(local_upd[_nd->local_sep_tree->root()].data(), local_upd[_nd->local_sep_tree->root()].size(), mpi_type<integer_t>(), pa_rank, tag, this->_comm, &send_req.back());
	dsep_work = local_subtree_work[_nd->local_sep_tree->root()];
	send_req.emplace_back();
	MPI_Isend(&dsep_work, 1, MPI_FLOAT, pa_rank, tag+2, this->_comm, &send_req.back());
      } else {
	auto sep_begin = _nd->dist_sep_range.first;
	auto sep_end = _nd->dist_sep_range.second;
	for (integer_t r=0; r<sep_end-sep_begin; r++) {
	  auto ice = _nd->my_dist_sep->get_ind()+_nd->my_dist_sep->get_ptr()[r+1];
	  auto icb = std::lower_bound(_nd->my_dist_sep->get_ind()+_nd->my_dist_sep->get_ptr()[r], ice, sep_end);
	  auto mid = dist_upd.size();
	  std::copy(icb, ice, std::back_inserter(dist_upd));
	  std::inplace_merge(dist_upd.begin(), dist_upd.begin() + mid, dist_upd.end());
	  dist_upd.erase(std::unique(dist_upd.begin(), dist_upd.end()), dist_upd.end());
	}

	auto chl = _nd->proc_dist_sep[_nd->sep_tree->lch()[dsep]];
	auto chr = _nd->proc_dist_sep[_nd->sep_tree->rch()[dsep]];
	// receive dist_upd from left child
	MPI_Status stat;
	int msg_size;
	// TODO probe both left and right, take the first
	MPI_Probe(chl, 1, this->_comm, &stat);
	MPI_Get_count(&stat, mpi_type<integer_t>(), &msg_size);
	std::vector<integer_t> dist_upd_lch(msg_size);
	MPI_Recv(dist_upd_lch.data(), msg_size, mpi_type<integer_t>(), chl, 1, this->_comm, &stat);
	// merge dist_upd from left child into dist_upd
	auto icb = std::lower_bound(dist_upd_lch.begin(), dist_upd_lch.end(), sep_end);
	auto mid = dist_upd.size();
	std::copy(icb, dist_upd_lch.end(), std::back_inserter(dist_upd));
	std::inplace_merge(dist_upd.begin(), dist_upd.begin() + mid, dist_upd.end());
	dist_upd.erase(std::unique(dist_upd.begin(), dist_upd.end()), dist_upd.end());

	// receive dist_upd from right child
	MPI_Probe(chr, 2, this->_comm, &stat);
	MPI_Get_count(&stat, mpi_type<integer_t>(), &msg_size);
	std::vector<integer_t> dist_upd_rch(msg_size);
	MPI_Recv(dist_upd_rch.data(), msg_size, mpi_type<integer_t>(), chr, 2, this->_comm, &stat);
	// merge dist_upd from right child into dist_upd
	icb = std::lower_bound(dist_upd_rch.begin(), dist_upd_rch.end(), sep_end);
	mid = dist_upd.size();
	std::copy(icb, dist_upd_rch.end(), std::back_inserter(dist_upd));
	std::inplace_merge(dist_upd.begin(), dist_upd.begin() + mid, dist_upd.end());
	dist_upd.erase(std::unique(dist_upd.begin(), dist_upd.end()), dist_upd.end());

	// receive work estimates for left and right subtrees
	float dsep_left_work, dsep_right_work;
	MPI_Recv(&dsep_left_work,  1, MPI_FLOAT, chl, 3, this->_comm, &stat);
	MPI_Recv(&dsep_right_work, 1, MPI_FLOAT, chr, 4, this->_comm, &stat);
	integer_t dim_blk = (sep_end - sep_begin) + dist_upd.size();
	dsep_work = (float(dim_blk)*dim_blk*dim_blk) + dsep_left_work + dsep_right_work;

	// send dist_upd and work estimate to parent
	if (_nd->sep_tree->pa()[pa] != -1) { // do not send to parent if parent is root
	  send_req.emplace_back();
	  int tag = (dsep == _nd->sep_tree->lch()[pa]) ? 1 : 2;
	  MPI_Isend(dist_upd.data(), dist_upd.size(), mpi_type<integer_t>(), pa_rank, tag, this->_comm, &send_req.back());
	  send_req.emplace_back();
	  MPI_Isend(&dsep_work, 1, MPI_FLOAT, pa_rank, tag+2, this->_comm, &send_req.back());
	}
      }
    }
    MPI_Waitall(send_req.size(), send_req.data(), MPI_STATUSES_IGNORE);
  }

  /**
   * Send the distributed separator from the process responsible for it,
   * to all the processes working on the frontal matrix corresponding to
   * it, and to all processes working on the sibling front (needed for
   * extend-add).  Hence this routine only needs to be called by those
   * processes (or simply by everyone in this->_comm).
   */
  template<typename scalar_t,typename integer_t> void
  EliminationTreeMPIDist<scalar_t,integer_t>::communicate_distributed_separator
  (integer_t dsep, std::vector<integer_t>& dist_upd, integer_t& dsep_begin, integer_t& dsep_end,
   std::vector<integer_t>& dsep_upd, int P0, int P, int P0_sibling, int P_sibling, int owner, bool use_hss) {
    auto rank = mpi_rank(this->_comm);
    integer_t* sbuf = NULL;
    std::vector<MPI_Request> sreq;
    int dest0 = std::min(P0, P0_sibling);
    int dest1 = std::max(P0+P, P0_sibling+P_sibling);
    if (rank == owner) {
      sbuf = new integer_t[2+dist_upd.size()];
      sbuf[0] = _nd->dist_sep_range.first;
      sbuf[1] = _nd->dist_sep_range.second;
      std::copy(dist_upd.begin(), dist_upd.end(), sbuf+2);
      if (use_hss) sreq.resize(mpi_nprocs(this->_comm));
      else sreq.resize(dest1-dest0);
      int msg = 0;
      for (int dest=dest0; dest<dest1; dest++)
	MPI_Isend(sbuf, 2 + dist_upd.size(), mpi_type<integer_t>(), dest, 0, this->_comm, &sreq[msg++]);
      if (use_hss) {
	// when using HSS compression, every process needs to know the
	// size of this front, because you need to know if your parent
	// is HSS
	for (int dest=0; dest<dest0; dest++)
	  MPI_Isend(sbuf, 2, mpi_type<integer_t>(), dest, 0, this->_comm, &sreq[msg++]);
      	for (int dest=dest1; dest<mpi_nprocs(this->_comm); dest++)
	  MPI_Isend(sbuf, 2, mpi_type<integer_t>(), dest, 0, this->_comm, &sreq[msg++]);
      }
    }
    if (rank >= dest0 && rank < dest1) {
      MPI_Status stat;
      MPI_Probe(owner, 0, this->_comm, &stat);
      int msg_size;
      MPI_Get_count(&stat, mpi_type<integer_t>(), &msg_size);
      auto rbuf = new integer_t[msg_size];
      MPI_Recv(rbuf, msg_size, mpi_type<integer_t>(), owner, 0, this->_comm, &stat);
      dsep_begin = rbuf[0];
      dsep_end = rbuf[1];
      dsep_upd.assign(rbuf+2, rbuf+msg_size);
      delete[] rbuf;
    } else if (use_hss) {
      integer_t rbuf[2];
      MPI_Recv(rbuf, 2, mpi_type<integer_t>(), owner, 0, this->_comm, MPI_STATUS_IGNORE);
      dsep_begin = rbuf[0];
      dsep_end = rbuf[1];
    }
    if (rank == owner) {
      MPI_Waitall(sreq.size(), sreq.data(), MPI_STATUSES_IGNORE);
      delete[] sbuf;
    }
  }

  // TODO merge in communicate_distributed_separator
  template<typename scalar_t,typename integer_t> void
  EliminationTreeMPIDist<scalar_t,integer_t>::communicate_distributed_separator_HSS_tree
  (HSS::HSSPartitionTree& sep_hss_tree, integer_t dsep, int P0, int P, int owner) {
    auto rank = mpi_rank(this->_comm);
    std::vector<MPI_Request> sreq;
    std::vector<int> sbuf;
    if (rank == owner) {
      sbuf = _nd->sep_tree->HSS_trees()[dsep].serialize();
      sreq.resize(P);
      for (int dest=P0; dest<P0+P; dest++)
	MPI_Isend(sbuf.data(), sbuf.size(), MPI_INT, dest, 0, this->_comm, &sreq[dest-P0]);
    }
    if (rank >= P0 && rank < P0+P) {
      MPI_Status stat;
      MPI_Probe(owner, 0, this->_comm, &stat);
      int msg_size;
      MPI_Get_count(&stat, MPI_INT, &msg_size);
      std::vector<int> rbuf(msg_size);
      MPI_Recv(rbuf.data(), rbuf.size(), MPI_INT, owner, 0, this->_comm, &stat);
      sep_hss_tree = HSS::HSSPartitionTree(rbuf);
    }
    if (rank == owner) MPI_Waitall(sreq.size(), sreq.data(), MPI_STATUSES_IGNORE);
  }

  template<typename scalar_t,typename integer_t> FrontalMatrix<scalar_t,integer_t>*
  EliminationTreeMPIDist<scalar_t,integer_t>::proportional_mapping
  (const SPOptions<scalar_t>& opts, std::vector<integer_t>* local_upd, std::vector<integer_t>& dist_upd,
   float* local_subtree_work, float* dist_subtree_work, integer_t dsep,
   int P0, int P, int P0_sibling, int P_sibling, MPI_Comm front_comm, bool hss_parent, int level) {
    auto rank = mpi_rank(this->_comm);
    auto chl = _nd->sep_tree->lch()[dsep];
    auto chr = _nd->sep_tree->rch()[dsep];
    auto owner = _nd->proc_dist_sep[dsep];

    if (chl == -1 && chr == -1) {   // leaf of the distributed separator tree -> local subgraph
      RedistSubTree<integer_t> sub_tree(_nd, local_upd, local_subtree_work, P0, P,
					P0_sibling, P_sibling, owner, this->_comm);
      return proportional_mapping_sub_graphs(opts, sub_tree, dsep, sub_tree.root, P0, P, P0_sibling, P_sibling,
					     front_comm, hss_parent, level);
    }

    integer_t dsep_begin, dsep_end;
    std::vector<integer_t> dsep_upd;
    communicate_distributed_separator(dsep, dist_upd, dsep_begin, dsep_end, dsep_upd,
				      P0, P, P0_sibling, P_sibling, owner, opts.use_HSS() && hss_parent);
    auto dim_dsep = dsep_end - dsep_begin;
    bool is_hss = opts.use_HSS() && hss_parent && (dim_dsep >= opts.HSS_min_sep_size());
    HSS::HSSPartitionTree sep_hss_partition(dim_dsep);
    if (is_hss) communicate_distributed_separator_HSS_tree(sep_hss_partition, dsep, P0, P, owner);

    // bool is_hss = opts.use_HSS() && (dim_dsep >= opts.HSS_min_sep_size()) &&
    //   (dim_dsep + dsep_upd.size() >= opts.HSS_min_front_size());
    // // HSS::HSSPartitionTree sep_hss_partition(dim_dsep);
    // // if (is_hss) communicate_distributed_separator_HSS_tree(sep_hss_partition, dsep, P0, P, owner);

    if (rank == P0) {
      if (is_hss) this->_nr_HSS_fronts++;
      else this->_nr_dense_fronts++;
    }
    F_t* front = nullptr;
    // only store fronts you work on and their siblings (needed for extend-add operation)
    if ((rank >= P0 && rank < P0+P) || (rank >= P0_sibling && rank < P0_sibling+P_sibling)) {
      if (P == 1) {
	if (is_hss) {
	  front = new FHSS_t(&_Aprop, dsep, dsep_begin, dsep_end, dsep_upd.size(), dsep_upd.data());
	  front->set_HSS_partitioning(opts, sep_hss_partition, level == 0);
	} else front = new FD_t(&_Aprop, dsep, dsep_begin, dsep_end, dsep_upd.size(), dsep_upd.data());
	if (P0 == rank) {
	  _local_range.first = std::min(_local_range.first, dsep_begin);
	  _local_range.second = std::max(_local_range.second, dsep_end);
	}
      } else {
	if (is_hss) {
	  front = new FHSSMPI_t(&_Aprop, _local_pfronts.size(), dsep_begin, dsep_end,
				dsep_upd.size(), dsep_upd.data(), front_comm, P);
	  front->set_HSS_partitioning(opts, sep_hss_partition, level == 0);
      }	else front = new FDMPI_t(&_Aprop, _local_pfronts.size(), dsep_begin, dsep_end,
				 dsep_upd.size(), dsep_upd.data(), front_comm, P);
	if (rank == P0) _local_pfronts_master.emplace_back(dsep_begin, dsep_end, P0, P);
	if (rank >= P0 && rank < P0+P) _local_pfronts.emplace_back(static_cast<FMPI_t*>(front), P0, P);
      }
    }

    // here we should still continue, to send the local subgraph
    auto wl = dist_subtree_work[chl];
    auto wr = dist_subtree_work[chr];
    int Pl = std::max(1, std::min(int(std::round(P * wl / (wl + wr))), P-1));
    int Pr = std::max(1, P - Pl);
    MPI_Comm comm_left  = mpi_sub_comm(front_comm, 0,    Pl);
    MPI_Comm comm_right = mpi_sub_comm(front_comm, P-Pr, Pr);
    auto lch = proportional_mapping(opts, local_upd, dist_upd, local_subtree_work, dist_subtree_work,
				    chl, P0, Pl, P0+P-Pr, Pr, comm_left, is_hss, level+1);
    auto rch = proportional_mapping(opts, local_upd, dist_upd, local_subtree_work, dist_subtree_work,
				    chr, P0+P-Pr, Pr, P0, Pl, comm_right, is_hss, level+1);
    if (front) {
      front->lchild = lch;
      front->rchild = rch;
    }
    return front;
  }

  /** This should only be called by [P0,P0+P) and [P0_sibling,P0_sibling+P_sibling) */
  template<typename scalar_t,typename integer_t> FrontalMatrix<scalar_t,integer_t>*
  EliminationTreeMPIDist<scalar_t,integer_t>::proportional_mapping_sub_graphs
  (const SPOptions<scalar_t>& opts, RedistSubTree<integer_t>& tree, integer_t dsep, integer_t sep, int P0, int P,
   int P0_sibling, int P_sibling, MPI_Comm front_comm, bool hss_parent, int level) {
    if (tree.data == NULL) return NULL;
    auto rank = mpi_rank(this->_comm);
    auto sep_begin = tree.sep_ptr[sep];
    auto sep_end = tree.sep_ptr[sep+1];
    auto dim_sep = sep_end - sep_begin;
    auto dim_upd = tree.dim_upd[sep];
    auto upd = tree.upd[sep];
    F_t* front = nullptr;

    bool is_hss = opts.use_HSS() && hss_parent && (dim_sep >= opts.HSS_min_sep_size());
    // bool is_hss = opts.use_HSS() && (dim_sep >= opts.HSS_min_sep_size()) &&
    //   (dim_sep + dim_upd >= opts.HSS_min_front_size());

    if (rank == P0) {
      if (is_hss) this->_nr_HSS_fronts++;
      else this->_nr_dense_fronts++;
    }
    if ((rank >= P0 && rank < P0+P) || (rank >= P0_sibling && rank < P0_sibling+P_sibling)) {
      if (P == 1) {
	if (is_hss) {
	  front = new FHSS_t(&_Aprop, sep, sep_begin, sep_end, dim_upd, upd);
	  front->set_HSS_partitioning(opts, tree.sep_HSS_tree[sep], level == 0);
	} else front = new FD_t(&_Aprop, sep, sep_begin, sep_end, dim_upd, upd);
	if (P0 == rank) {
	  _local_range.first = std::min(_local_range.first, sep_begin);
	  _local_range.second = std::max(_local_range.second, sep_end);
	}
      } else {
	if (is_hss) {
	  front = new FHSSMPI_t(&_Aprop, _local_pfronts.size(), sep_begin, sep_end, dim_upd, upd, front_comm, P);
	  front->set_HSS_partitioning(opts, tree.sep_HSS_tree[sep], level == 0);
	} else front = new FDMPI_t(&_Aprop, _local_pfronts.size(), sep_begin, sep_end, dim_upd, upd, front_comm, P);
	if (rank == P0) _local_pfronts_master.emplace_back(sep_begin, sep_end, P0, P);
	if (rank >= P0 && rank < P0+P) _local_pfronts.emplace_back(static_cast<FMPI_t*>(front), P0, P);
      }
    }
    if (rank < P0 || rank >= P0+P) return front;
    auto chl = tree.lchild[sep];
    auto chr = tree.rchild[sep];
    if (chl != -1 && chr != -1) {
      auto wl = tree.work[chl];
      auto wr = tree.work[chr];
      int Pl = std::max(1, std::min(int(std::round(P * wl / (wl + wr))), P-1));
      int Pr = std::max(1, P - Pl);
      MPI_Comm comm_left  = mpi_sub_comm(front_comm, 0, Pl);
      MPI_Comm comm_right = mpi_sub_comm(front_comm, P-Pr, Pr);
      front->lchild = proportional_mapping_sub_graphs
	(opts, tree, dsep, chl, P0, Pl, P0+P-Pr, Pr, comm_left, is_hss, level+1);
      front->rchild = proportional_mapping_sub_graphs
	(opts, tree, dsep, chr, P0+P-Pr, Pr, P0, Pl, comm_right, is_hss, level+1);
    }
    return front;
  }

} // end namespace strumpack

#endif
