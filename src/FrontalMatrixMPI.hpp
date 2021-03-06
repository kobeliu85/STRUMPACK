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
 * (5) year renewals, the U.S. Government igs granted for itself and others acting
 * on its behalf a paid-up, nonexclusive, irrevocable, worldwide license in the
 * Software to reproduce, prepare derivative works, distribute copies to the
 * public, perform publicly and display publicly, and to permit others to do so.
 *
 * Developers: Pieter Ghysels, Francois-Henry Rouet, Xiaoye S. Li.
 *             (Lawrence Berkeley National Lab, Computational Research Division).
 *
 */
#ifndef FRONTAL_MATRIX_MPI_HPP
#define FRONTAL_MATRIX_MPI_HPP

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include "CompressedSparseMatrix.hpp"
#include "DistributedMatrix.hpp"
#include "MatrixReordering.hpp"
#include "TaskTimer.hpp"
#include "MPI_wrapper.hpp"
#include "ExtendAdd.hpp"

namespace strumpack {

  template<typename scalar_t,typename integer_t>
  class FrontalMatrixMPI : public FrontalMatrix<scalar_t,integer_t> {
    using FMPI_t = FrontalMatrixMPI<scalar_t,integer_t>;
    using F_t = FrontalMatrix<scalar_t,integer_t>;
    using DenseM_t = DenseMatrix<scalar_t>;
    using DistM_t = DistributedMatrix<scalar_t>;
    using DistMW_t = DistributedMatrixWrapper<scalar_t>;
    using ExtAdd = ExtendAdd<scalar_t,integer_t>;
  public:
    FrontalMatrixMPI(CompressedSparseMatrix<scalar_t,integer_t>* _A,
		     integer_t _sep, integer_t _sep_begin, integer_t _sep_end,
		     integer_t _dim_upd, integer_t* _upd, MPI_Comm _front_comm,
		     int _total_procs);
    FrontalMatrixMPI(const FrontalMatrixMPI&) = delete;
    FrontalMatrixMPI& operator=(FrontalMatrixMPI const&) = delete;
    virtual ~FrontalMatrixMPI();
    void sample_CB(const SPOptions<scalar_t>& opts, DenseM_t& R, DenseM_t& Sr, DenseM_t& Sc, F_t* parent, int task_depth=0) {};
    virtual void sample_CB(const SPOptions<scalar_t>& opts, const DistM_t& R, DistM_t& Sr, DistM_t& Sc, F_t* pa) const = 0;
    void extract_2d(const std::vector<std::size_t>& I, std::vector<std::size_t>& J, DistM_t& B) const;
    void get_child_submatrix_2d(const F_t* ch, const std::vector<std::size_t>& I, const std::vector<std::size_t>& J, DistM_t& B) const;
    void extract_CB_sub_matrix(const std::vector<std::size_t>& I, const std::vector<std::size_t>& J, DenseM_t& B, int task_depth) const {};
    virtual void extract_CB_sub_matrix_2d(const std::vector<std::size_t>& I, const std::vector<std::size_t>& J, DistM_t& B) const = 0;
    void extend_add_b(F_t* ch, DistM_t& b_sep, scalar_t* wmem, int tag);
    void extend_add_b_mpi_to_mpi(FMPI_t* ch, DistM_t& b_sep, scalar_t* wmem, int tag);
    void extend_add_b_seq_to_mpi(F_t* ch, DistM_t& b_sep, scalar_t* wmem, int tag);
    void extract_b(F_t* ch, DistM_t& b_sep, scalar_t* wmem);
    void extract_b_mpi_to_mpi(FMPI_t* ch, DistM_t& b_sep, scalar_t* wmem);
    void extract_b_seq_to_mpi(F_t* ch, DistM_t& b_sep, scalar_t* wmem);
    inline bool visit(const F_t* ch) const;
    inline int child_master(const F_t* ch) const;
    inline int blacs_context() const { return ctxt; }
    inline int blacs_context_all() const { return ctxt_all; }
    inline bool active() const { return front_comm != MPI_COMM_NULL && mpi_rank(front_comm) < proc_rows*proc_cols; }
    static inline void processor_grid(int np_procs, int& np_rows, int& np_cols) {
      np_cols = std::floor(std::sqrt((float)np_procs));
      np_rows = np_procs / np_cols;
    }
    inline int np_rows() const { return proc_rows; }
    inline int np_cols() const { return proc_cols; }
    inline int find_rank(integer_t r, integer_t c, DistM_t& F) const;
    inline int find_rank_fixed(integer_t r, integer_t c, DistM_t& F) const;
    virtual void solve_workspace_query(integer_t& mem_size);
    virtual long long dense_factor_nonzeros(int task_depth=0) const;
    virtual std::string type() const { return "FrontalMatrixMPI"; }
    virtual bool isMPI() const { return true; }
    MPI_Comm comm() const { return front_comm; }
    virtual void bisection_partitioning(const SPOptions<scalar_t>& opts, integer_t* sorder,
					bool isroot=true, int task_depth=0);

    friend class FrontalMatrixDenseMPI<scalar_t,integer_t>;
    friend class FrontalMatrixHSSMPI<scalar_t,integer_t>;

  protected:
    int ctxt;         // this is a blacs context with only the active process for this front
    int ctxt_all;     // this is a blacs context with all process for this front
    int proc_rows;    // number of processes per row in the blacs ctxt
    int proc_cols;    // number of processes per col in the blacs ctxt
    int total_procs;  // number of processes that work on the subtree belonging to this front,
    // this can be more than the processes in the blacs context ctxt and is
    // not necessarily the same as mpi_nprocs(front_comm),
    // because if this rank is not part of front_comm, mpi_nprocs(front_comm) == 0
    MPI_Comm front_comm;  // MPI communicator for this front
  };

  template<typename scalar_t,typename integer_t>
  FrontalMatrixMPI<scalar_t,integer_t>::FrontalMatrixMPI
  (CompressedSparseMatrix<scalar_t,integer_t>* _A, integer_t _sep, integer_t _sep_begin, integer_t _sep_end,
   integer_t _dim_upd, integer_t* _upd, MPI_Comm _front_comm, int _total_procs)
    : F_t(_A, NULL, NULL,_sep, _sep_begin, _sep_end, _dim_upd, _upd),
    total_procs(_total_procs), front_comm(_front_comm) {
    processor_grid(total_procs, proc_rows, proc_cols);
    if (front_comm != MPI_COMM_NULL) {
      int active_procs = proc_rows * proc_cols;
      if (active_procs < total_procs) {
	MPI_Comm active_front_comm = mpi_sub_comm(front_comm, 0, active_procs);
	if (mpi_rank(front_comm) < active_procs) {
	  ctxt = Csys2blacs_handle(active_front_comm);
	  Cblacs_gridinit(&ctxt, "C", proc_rows, proc_cols);
	} else ctxt = -1;
	mpi_free_comm(&active_front_comm);
      } else {
	ctxt = Csys2blacs_handle(front_comm);
	Cblacs_gridinit(&ctxt, "C", proc_rows, proc_cols);
      }
      ctxt_all = Csys2blacs_handle(front_comm);
      Cblacs_gridinit(&ctxt_all, "R", 1, total_procs);
    } else ctxt = ctxt_all = -1;
  }

  template<typename scalar_t,typename integer_t>
  FrontalMatrixMPI<scalar_t,integer_t>::~FrontalMatrixMPI() {
    if (ctxt != -1) Cblacs_gridexit(ctxt);
    if (ctxt_all != -1) Cblacs_gridexit(ctxt_all);
    mpi_free_comm(&front_comm);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extract_2d
  (const std::vector<std::size_t>& I, std::vector<std::size_t>& J, DistM_t& B) const {
    auto m = I.size();
    auto n = J.size();
    TIMER_TIME(TaskType::EXTRACT_SEP_2D, 2, t_ex_sep);
    {
      DistM_t tmp(ctxt, m, n);
      this->A->extract_separator_2d(this->sep_end, I, J, tmp, front_comm);
      strumpack::copy(m, n, tmp, 0, 0, B, 0, 0, ctxt_all); // TODO why this copy???
    }
    TIMER_STOP(t_ex_sep);
    TIMER_TIME(TaskType::GET_SUBMATRIX_2D, 2, t_getsub);
    DistM_t Bl, Br;
    get_child_submatrix_2d(this->lchild, I, J, Bl);
    get_child_submatrix_2d(this->rchild, I, J, Br);
    DistM_t tmp(B.ctxt(), m, n);
    strumpack::copy(m, n, Bl, 0, 0, tmp, 0, 0, ctxt_all);
    B.add(tmp);
    strumpack::copy(m, n, Br, 0, 0, tmp, 0, 0, ctxt_all);
    B.add(tmp);
    TIMER_STOP(t_getsub);
  }

  // this should not be necessary with proper polymorphic code
  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::get_child_submatrix_2d
  (const F_t* ch, const std::vector<std::size_t>& I, const std::vector<std::size_t>& J, DistM_t& B) const {
    if (!ch) return;
    auto m = I.size();
    auto n = J.size();
    if (auto mpi_child = dynamic_cast<const FMPI_t*>(ch)) {
      // TODO check if this really needs to be the child context,
      // maybe we can do directly to this context??
      // check in both FrontalMatrixDenseMPI and HSSMPI
      B = DistM_t(mpi_child->blacs_context(), m, n);
      B.zero();
      mpi_child->extract_CB_sub_matrix_2d(I, J, B);
    } else {
      TIMER_TIME(TaskType::GET_SUBMATRIX, 2, t_getsub);
      auto pch = child_master(ch);
      B = DistM_t(ctxt, m, n);
      DenseM_t lB;
      if (mpi_rank(front_comm) == pch) {
	lB = DenseM_t(m, n);
	lB.zero();
	ch->extract_CB_sub_matrix(I, J, lB, 0);
      }
      strumpack::copy(m, n, lB, pch, B, 0, 0, ctxt_all);
    }
  }

  /** return the rank in front_comm where element r,c in matrix F is located */
  template<typename scalar_t,typename integer_t> inline int
  FrontalMatrixMPI<scalar_t,integer_t>::find_rank(integer_t r, integer_t c, DistM_t& F) const {
    // the blacs grid is column major
    return ((r / F.MB()) % proc_rows) + ((c / F.NB()) % proc_cols) * proc_rows;
  }

  template<typename scalar_t,typename integer_t> inline int
  FrontalMatrixMPI<scalar_t,integer_t>::find_rank_fixed(integer_t r, integer_t c, DistM_t& F) const {
    assert(F.fixed());
    return ((r / DistM_t::default_MB) % proc_rows)
      + ((c / DistM_t::default_NB) % proc_cols) * proc_rows;
  }

  /**
   * Check if the child needs to be visited not necessary when this rank
   * is not part of the processes assigned to the child.
   */
  template<typename scalar_t,typename integer_t> bool
  FrontalMatrixMPI<scalar_t,integer_t>::visit(const F_t* ch) const {
    if (!ch) return false;
    if (auto mpi_child = dynamic_cast<const FMPI_t*>(ch)) {
      if (mpi_child->front_comm == MPI_COMM_NULL) return false;        // child is MPI
    } else if (mpi_rank(front_comm) != child_master(ch)) return false; // child is sequential
    return true;
  }

  template<typename scalar_t,typename integer_t> int
  FrontalMatrixMPI<scalar_t,integer_t>::child_master(const F_t* ch) const {
    int ch_master;
    if (auto mpi_ch = dynamic_cast<const FMPI_t*>(ch))
      ch_master = (ch == this->lchild) ? 0 : total_procs - mpi_ch->total_procs;
    else ch_master = (ch == this->lchild) ? 0 : total_procs - 1;
    return ch_master;
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extend_add_b(F_t* ch, DistM_t& b_sep, scalar_t* wmem, int tag) {
    if (mpi_rank(front_comm) == 0) { STRUMPACK_FLOPS(static_cast<long long int>(ch->dim_upd)*ch->dim_upd); }
    if (FMPI_t* mpi_child = dynamic_cast<FMPI_t*>(ch))
      extend_add_b_mpi_to_mpi(mpi_child, b_sep, wmem, tag); // child is MPI
    else extend_add_b_seq_to_mpi(ch, b_sep, wmem, tag);     // child is sequential
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extract_b(F_t* ch, DistM_t& b_sep, scalar_t* wmem) {
    if (mpi_rank(front_comm) == 0) { STRUMPACK_FLOPS(static_cast<long long int>(ch->dim_upd)*ch->dim_upd); }
    if (FMPI_t* mpi_child = dynamic_cast<FMPI_t*>(ch))
      extract_b_mpi_to_mpi(mpi_child, b_sep, wmem); // child is MPI
    else extract_b_seq_to_mpi(ch, b_sep, wmem);     // child is sequential
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extend_add_b_mpi_to_mpi
  (FMPI_t* ch, DistM_t& b_sep, scalar_t* wmem, int tag) {
    DistMW_t ch_b_upd, b_upd;
    if (visit(ch)) ch_b_upd = DistMW_t(ch->ctxt, ch->dim_upd, 1, wmem+ch->p_wmem);
    if (front_comm != MPI_COMM_NULL)
      b_upd = DistMW_t(ctxt, this->dim_upd, 1, wmem+this->p_wmem);
    std::vector<std::vector<scalar_t>> sbuf;
    std::vector<MPI_Request> sreq;
    if (ch_b_upd.pcol() == 0) {
      // send to all active processes in the parent (only 1 column)
      auto I = ch->upd_to_parent(this);
      sbuf.resize(proc_rows);
      ExtAdd::extend_add_column_copy_to_buffers(ch_b_upd, b_sep, b_upd, sbuf, this, I);
      sreq.resize(proc_rows);
      for (int p=0; p<proc_rows; p++)
	MPI_Isend(sbuf[p].data(), sbuf[p].size(), mpi_type<scalar_t>(), p, tag, front_comm, &sreq[p]);
    }
    if (b_sep.pcol() == 0) {
      // receive from all active processes in the child (only 1 column)
      std::vector<std::vector<scalar_t>> rbuf(ch->proc_rows);
      MPI_Status status;
      int msg_size;
      auto ch_master = child_master(ch);
      for (int p=0; p<ch->proc_rows; p++) {
	MPI_Probe(MPI_ANY_SOURCE, tag, front_comm, &status);
	MPI_Get_count(&status, mpi_type<scalar_t>(), &msg_size);
	auto pi = status.MPI_SOURCE - ch_master;
	rbuf[pi].resize(msg_size);
	MPI_Recv(rbuf[pi].data(), msg_size, mpi_type<scalar_t>(), status.MPI_SOURCE, tag, front_comm, &status);
      }
      auto b_rank = [&](integer_t r, integer_t c) -> int { return ch->find_rank(r, 0, ch_b_upd); };
      ExtAdd::extend_add_column_copy_from_buffers
	(b_sep, b_upd, rbuf, this->sep_begin, this->upd, ch->upd, ch->dim_upd, b_rank);
    }
    if (ch_b_upd.pcol() == 0)
      MPI_Waitall(sreq.size(), sreq.data(), MPI_STATUSES_IGNORE);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extend_add_b_seq_to_mpi
  (F_t* ch, DistM_t& b_sep, scalar_t* wmem, int tag) {
    int child_rank = child_master(ch);
    scalar_t* ch_b_upd = wmem+ch->p_wmem;
    DistMW_t b_upd;
    if (front_comm != MPI_COMM_NULL)
      b_upd = DistMW_t(ctxt, this->dim_upd, 1, wmem+this->p_wmem);
    std::vector<MPI_Request> sreq;
    std::vector<std::vector<scalar_t>> sbuf;
    if (mpi_rank(front_comm) == child_rank) {
      auto I = ch->upd_to_parent(this);
      sbuf.resize(proc_rows);
      for (auto& buf : sbuf) buf.reserve(ch->dim_upd / proc_rows);
      for (integer_t r=0; r<ch->dim_upd; r++) {
	integer_t pa_row = I[r];
	if (pa_row < this->dim_sep)
	  sbuf[find_rank(pa_row, 0, b_sep)].push_back(ch_b_upd[r]);
	else sbuf[find_rank(pa_row-this->dim_sep, 0, b_upd)].push_back(ch_b_upd[r]);
      }
      // send to all active processes in the parent (1 column)
      sreq.resize(proc_rows);
      for (int p=0; p<proc_rows; p++)
	MPI_Isend(sbuf[p].data(), sbuf[p].size(), mpi_type<scalar_t>(), p, tag, front_comm, &sreq[p]);
    }
    if (b_sep.pcol() == 0) {
      // receive from the process working on the child
      MPI_Status status;
      MPI_Probe(child_rank, tag, front_comm, &status);
      int msg_size;
      MPI_Get_count(&status, mpi_type<scalar_t>(), &msg_size);
      std::vector<std::vector<scalar_t>> rbuf(1);
      rbuf[0].resize(msg_size);
      MPI_Recv(rbuf[0].data(), msg_size, mpi_type<scalar_t>(), child_rank, tag, front_comm, &status);
      ExtAdd::extend_add_column_copy_from_buffers
	(b_sep, b_upd, rbuf, this->sep_begin, this->upd, ch->upd, ch->dim_upd,
	 [](integer_t,integer_t) -> int { return 0; });
    }
    if (mpi_rank(front_comm) == child_rank)
      MPI_Waitall(sreq.size(), sreq.data(), MPI_STATUSES_IGNORE);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::solve_workspace_query(integer_t& mem_size) {
    if (this->lchild) this->lchild->solve_workspace_query(mem_size);
    if (this->rchild) this->rchild->solve_workspace_query(mem_size);
    this->p_wmem = mem_size;
    if (ctxt != -1) {
      int np_rows, np_cols, p_row, p_col;
      Cblacs_gridinfo(ctxt, &np_rows, &np_cols, &p_row, &p_col);
      mem_size += scalapack::numroc(this->dim_upd, DistM_t::default_NB, p_row, 0, np_rows);
    }
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extract_b_mpi_to_mpi(FMPI_t* ch, DistM_t& b_sep, scalar_t* wmem) {
    DistMW_t ch_b_upd, b_upd;
    if (visit(ch)) ch_b_upd = DistMW_t(ch->ctxt, ch->dim_upd, 1, wmem+ch->p_wmem);
    if (front_comm != MPI_COMM_NULL) b_upd = DistMW_t(ctxt, this->dim_upd, 1, wmem+this->p_wmem);
    std::vector<MPI_Request> sreq;
    std::vector<std::vector<scalar_t>> sbuf;
    auto I = ch->upd_to_parent(this);
    if (b_sep.pcol() == 0) {
      // send to all active processes in the child (only 1 column)
      sbuf.resize(ch->proc_rows);
      std::function<int(integer_t)> ch_rank = [&](integer_t r) {
	return ch->find_rank(r, 0, ch_b_upd);
      };
      ExtAdd::extract_b_copy_to_buffers(b_sep, b_upd, sbuf, ch_rank, I, ch->proc_rows);
      sreq.resize(ch->proc_rows);
      for (int p=0; p<ch->proc_rows; p++)
	MPI_Isend(sbuf[p].data(), sbuf[p].size(), mpi_type<scalar_t>(),
		  p+child_master(ch), 0, front_comm, &sreq[p]);
    }
    if (ch_b_upd.pcol() == 0) {
      // receive from all active processes in the parent (only 1 column)
      std::vector<std::vector<scalar_t>> rbuf(proc_rows);
      MPI_Status status;
      int msg_size;
      for (int p=0; p<proc_rows; p++) {
	MPI_Probe(MPI_ANY_SOURCE, 0, front_comm, &status);
	MPI_Get_count(&status, mpi_type<scalar_t>(), &msg_size);
	rbuf[status.MPI_SOURCE].resize(msg_size);
	MPI_Recv(rbuf[status.MPI_SOURCE].data(), msg_size, mpi_type<scalar_t>(), status.MPI_SOURCE, 0, front_comm, &status);
      }
      std::function<int(integer_t)> src_rank = [&](integer_t r) {
	return (r < this->dim_sep) ? find_rank(r, 0, b_sep) : find_rank(r-this->dim_sep, 0, b_upd);
      };
      ExtAdd::extract_b_copy_from_buffers(ch_b_upd, rbuf, I, src_rank);
    }
    if (b_sep.pcol() == 0)
      MPI_Waitall(sreq.size(), sreq.data(), MPI_STATUSES_IGNORE);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::extract_b_seq_to_mpi(F_t* ch, DistM_t& b_sep, scalar_t* wmem) {
    int child_rank = child_master(ch);
    scalar_t* ch_b_upd = wmem+ch->p_wmem;
    DistMW_t b_upd;
    if (front_comm != MPI_COMM_NULL) b_upd = DistMW_t(ctxt, this->dim_upd, 1, wmem+this->p_wmem);
    MPI_Request sreq;
    std::vector<std::vector<scalar_t>> sbuf(1);
    auto I = ch->upd_to_parent(this);
    if (b_sep.pcol() == 0) {
      // send to all active processes in the child (only 1 column)
      std::function<int(integer_t)> dest_rank = [&](integer_t){ return 0; };
      ExtAdd::extract_b_copy_to_buffers(b_sep, b_upd, sbuf, dest_rank, I, 1);
      MPI_Isend(sbuf[0].data(), sbuf[0].size(), mpi_type<scalar_t>(),
		child_rank, 0, front_comm, &sreq);
    }
    if (mpi_rank(front_comm) == child_rank) {
      // receive from all active processes in the parent (only 1 column)
      std::vector<std::vector<scalar_t>> rbuf(proc_rows);
      MPI_Status status;
      int msg_size;
      for (int p=0; p<proc_rows; p++) {
	MPI_Probe(MPI_ANY_SOURCE, 0, front_comm, &status);
	MPI_Get_count(&status, mpi_type<scalar_t>(), &msg_size);
	rbuf[status.MPI_SOURCE].resize(msg_size);
	MPI_Recv(rbuf[status.MPI_SOURCE].data(), msg_size, mpi_type<scalar_t>(), status.MPI_SOURCE, 0, front_comm, &status);
      }
      std::vector<scalar_t*> pbuf(rbuf.size());
      for (std::size_t p=0; p<rbuf.size(); p++) pbuf[p] = rbuf[p].data();
      for (integer_t r=0; r<ch->dim_upd; r++) {
	integer_t pa_r = I[r];
	integer_t rank = (pa_r < this->dim_sep) ? find_rank(pa_r, 0, b_sep)
	  : find_rank(pa_r-this->dim_sep, 0, b_upd);
	ch_b_upd[r] = *(pbuf[rank]);
	pbuf[rank]++;
      }
    }
    if (b_sep.pcol() == 0) MPI_Wait(&sreq, MPI_STATUS_IGNORE);
  }

  template<typename scalar_t,typename integer_t> long long
  FrontalMatrixMPI<scalar_t,integer_t>::dense_factor_nonzeros(int task_depth) const {
    long long nnz = (this->front_comm != MPI_COMM_NULL && mpi_rank(this->front_comm) == 0) ?
      this->dim_blk*this->dim_blk-this->dim_upd*this->dim_upd : 0;
    if (visit(this->lchild)) nnz += this->lchild->dense_factor_nonzeros(task_depth);
    if (visit(this->rchild)) nnz += this->rchild->dense_factor_nonzeros(task_depth);
    return nnz;
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixMPI<scalar_t,integer_t>::bisection_partitioning
  (const SPOptions<scalar_t>& opts, integer_t* sorder, bool isroot, int task_depth) {
    for (integer_t i=this->sep_begin; i<this->sep_end; i++) sorder[i] = -i;

    if (visit(this->lchild)) this->lchild->bisection_partitioning(opts, sorder, false, task_depth);
    if (visit(this->rchild)) this->rchild->bisection_partitioning(opts, sorder, false, task_depth);
  }

} // end namespace strumpack

#endif //FRONTAL_MATRIX_MPI_HPP
