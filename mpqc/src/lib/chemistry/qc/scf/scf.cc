//
// scf.cc --- implementation of the SCF abstract base class
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Edward Seidl <seidl@janed.com>
// Maintainer: LPS
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifdef __GNUC__
#pragma implementation
#endif

#include <math.h>
#include <limits.h>
#include <sys/stat.h>

#include <util/misc/formio.h>
#include <util/group/mstate.h>

#include <math/scmat/local.h>
#include <math/scmat/repl.h>
#include <math/scmat/offset.h>
#include <math/scmat/blocked.h>

#include <math/optimize/diis.h>

#include <chemistry/qc/basis/petite.h>
#include <chemistry/qc/scf/scf.h>

///////////////////////////////////////////////////////////////////////////
// SCF

#define CLASSNAME SCF
#define VERSION 2
#define PARENTS public OneBodyWavefunction
#include <util/class/classia.h>
void *
SCF::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] = OneBodyWavefunction::_castdown(cd);
  return do_castdowns(casts,cd);
}

SCF::SCF(StateIn& s) :
  OneBodyWavefunction(s)
  maybe_SavableState(s)
{
  debug_ = 0;
  need_vec_ = 1;
  compute_guess_ = 0;

  s.get(maxiter_);
  s.get(dens_reset_freq_);
  s.get(reset_occ_);
  s.get(local_dens_);
  s.get(storage_);
  if (s.version(static_class_desc()) >= 2) {
    s.get(print_all_evals_);
    s.get(print_occ_evals_);
  }
  s.get(level_shift_);

  extrap_.restore_state(s);
  accumdih_.restore_state(s);
  accumddh_.restore_state(s);

  scf_grp_ = basis()->matrixkit()->messagegrp();
}

SCF::SCF(const RefKeyVal& keyval) :
  OneBodyWavefunction(keyval),
  need_vec_(1),
  compute_guess_(0),
  maxiter_(40),
  level_shift_(0),
  reset_occ_(0),
  local_dens_(1),
  storage_(0),
  dens_reset_freq_(10)
{
  if (keyval->exists("maxiter"))
    maxiter_ = keyval->intvalue("maxiter");

  if (keyval->exists("density_reset_frequency"))
    dens_reset_freq_ = keyval->intvalue("density_reset_frequency");

  if (keyval->exists("reset_occupations"))
    reset_occ_ = keyval->booleanvalue("reset_occupations");

  if (keyval->exists("level_shift"))
    level_shift_ = keyval->doublevalue("level_shift");

  extrap_ = keyval->describedclassvalue("extrap");
  if (extrap_.null())
    extrap_ = new DIIS;

  accumdih_ = keyval->describedclassvalue("accumdih");
  if (accumdih_.null())
    accumdih_ = new AccumHNull;
  
  accumddh_ = keyval->describedclassvalue("accumddh");
  if (accumddh_.null())
    accumddh_ = new AccumHNull;
  
  storage_ = keyval->intvalue("memory");

  debug_ = keyval->intvalue("debug");
  
  if (keyval->exists("local_density"))
    local_dens_ = keyval->booleanvalue("local_density");
    
  print_all_evals_ = keyval->booleanvalue("print_evals");
  print_occ_evals_ = keyval->booleanvalue("print_occupied_evals");
  
  scf_grp_ = basis()->matrixkit()->messagegrp();
  
  // first see if guess_wavefunction is a wavefunction, then check to
  // see if it's a string.
  if (keyval->exists("guess_wavefunction")) {
    cout << incindent << incindent;
    guess_wfn_ = keyval->describedclassvalue("guess_wavefunction");
    compute_guess_=1;
    if (guess_wfn_.null()) {
      compute_guess_=0;
      char *path = keyval->pcharvalue("guess_wavefunction");
      struct stat sb;
      if (path && stat(path, &sb)==0 && sb.st_size) {
        BcastStateInBin s(scf_grp_, path);

        // reset the default matrixkit so that the matrices in the guess
        // wavefunction will match those in this wavefunction
        RefSCMatrixKit oldkit = SCMatrixKit::default_matrixkit();
        SCMatrixKit::set_default_matrixkit(basis()->matrixkit());

        guess_wfn_.restore_state(s);

        // go back to the original default matrixkit
        SCMatrixKit::set_default_matrixkit(oldkit);
        delete[] path;
      }
    }
    cout << decindent << decindent;
  }
}

SCF::~SCF()
{
}

void
SCF::save_data_state(StateOut& s)
{
  OneBodyWavefunction::save_data_state(s);
  s.put(maxiter_);
  s.put(dens_reset_freq_);
  s.put(reset_occ_);
  s.put(local_dens_);
  s.put(storage_);
  s.put(print_all_evals_);
  s.put(print_occ_evals_);
  s.put(level_shift_);
  extrap_.save_state(s);
  accumdih_.save_state(s);
  accumddh_.save_state(s);
}

RefSCMatrix
SCF::eigenvectors()
{
  return eigenvectors_.result();
}

RefDiagSCMatrix
SCF::eigenvalues()
{
  return eigenvalues_.result();
}

int
SCF::spin_unrestricted()
{
  return 0;
}

void
SCF::print(ostream&o)
{
  OneBodyWavefunction::print(o);
  o << node0 << indent << "SCF Parameters:\n" << incindent
    << indent << "maxiter = " << maxiter_ << endl
    << indent << "density_reset_freq = " << dens_reset_freq_ << endl
    << indent << scprintf("level_shift = %f\n",level_shift_)
    << decindent << endl;
}

//////////////////////////////////////////////////////////////////////////////

void
SCF::compute()
{
  int me=scf_grp_->me();
  
  local_ = (LocalSCMatrixKit::castdown(basis()->matrixkit().pointer()) ||
            ReplSCMatrixKit::castdown(basis()->matrixkit().pointer())) ? 1:0;
  
  if (hessian_needed())
    set_desired_gradient_accuracy(desired_hessian_accuracy()/100.0);

  if (gradient_needed())
    set_desired_value_accuracy(desired_gradient_accuracy()/100.0);

  if (value_needed()) {
    cout << node0 << endl << indent
         << scprintf("SCF::compute: energy accuracy = %10.7e\n",
                     desired_value_accuracy())
         << endl;

    double eelec;
    compute_vector(eelec);
      
    // this will be done elsewhere eventually
    double nucrep = molecule()->nuclear_repulsion_energy();
    double eother = 0.0;
    if (accumddh_.nonnull()) eother = accumddh_->e();
    cout << node0 << endl << indent
         << scprintf("total scf energy = %20.15f", eelec+eother+nucrep)
         << endl;

    set_energy(eelec+eother+nucrep);
    set_actual_value_accuracy(desired_value_accuracy());
  }

  if (gradient_needed()) {
    RefSCVector gradient = matrixkit()->vector(moldim());

    cout << node0 << endl << indent
         << scprintf("SCF::compute: gradient accuracy = %10.7e\n",
                     desired_gradient_accuracy())
         << endl;

    compute_gradient(gradient);
    print_natom_3(gradient,"Total Gradient:");
    set_gradient(gradient);

    set_actual_gradient_accuracy(desired_gradient_accuracy());
  }
  
  if (hessian_needed()) {
    RefSymmSCMatrix hessian = matrixkit()->symmmatrix(moldim());
    
    cout << node0 << endl << indent
         << scprintf("SCF::compute: hessian accuracy = %10.7e\n",
                     desired_hessian_accuracy())
         << endl;

    compute_hessian(hessian);
    set_hessian(hessian);

    set_actual_hessian_accuracy(desired_hessian_accuracy());
  }
}

//////////////////////////////////////////////////////////////////////////////

signed char *
SCF::init_pmax(double *pmat_data)
{
  double l2inv = 1.0/log(2.0);
  double tol = pow(2.0,-126.0);
  
  GaussianBasisSet& gbs = *basis().pointer();
  
  signed char * pmax = new signed char[i_offset(gbs.nshell())];

  int ish, jsh, ij;
  for (ish=ij=0; ish < gbs.nshell(); ish++) {
    int istart = gbs.shell_to_function(ish);
    int iend = istart + gbs(ish).nfunction();
    
    for (jsh=0; jsh <= ish; jsh++,ij++) {
      int jstart = gbs.shell_to_function(jsh);
      int jend = jstart + gbs(jsh).nfunction();
      
      double maxp=0, tmp;

      for (int i=istart; i < iend; i++) {
        int ijoff = i_offset(i) + jstart;
        for (int j=jstart; j < ((ish==jsh) ? i+1 : jend); j++,ijoff++)
          if ((tmp=fabs(pmat_data[ijoff])) > maxp)
            maxp=tmp;
      }

      if (maxp <= tol)
        maxp=tol;

      long power = long(log(maxp)*l2inv);
      if (power < SCHAR_MIN) pmax[ij] = SCHAR_MIN;
      else if (power > SCHAR_MAX) pmax[ij] = SCHAR_MAX;
      else pmax[ij] = (signed char) power;
    }
  }

  return pmax;
}

//////////////////////////////////////////////////////////////////////////////

RefSymmSCMatrix
SCF::get_local_data(const RefSymmSCMatrix& m, double*& p, Access access)
{
  RefSymmSCMatrix l = m;
  
  if (!LocalSymmSCMatrix::castdown(l.pointer())
      && !ReplSymmSCMatrix::castdown(l.pointer())) {
    RefSCMatrixKit k = new ReplSCMatrixKit;
    l = k->symmmatrix(m.dim());
    l->convert(m);

    if (access == Accum)
      l->assign(0.0);
  } else if (scf_grp_->n() > 1 && access==Accum) {
    l = m.clone();
    l.assign(0.0);
  }

  if (ReplSymmSCMatrix::castdown(l.pointer()))
    p = ReplSymmSCMatrix::castdown(l.pointer())->get_data();
  else
    p = LocalSymmSCMatrix::castdown(l.pointer())->get_data();

  return l;
}

//////////////////////////////////////////////////////////////////////////////

void
SCF::initial_vector(int needv)
{
  if (need_vec_) {
    if (eigenvectors_.result_noupdate().null()) {
      // if guess_wfn_ is non-null then try to get a guess vector from it.
      // First check that the same basis is used...if not, then project the
      // guess vector into the present basis.
      // right now the check is crude...there should be an equiv member in
      // GaussianBasisSet
      if (guess_wfn_.nonnull()) {
        if (guess_wfn_->basis()->nbasis() == basis()->nbasis()) {
          cout << node0 << indent
               << "Using guess wavefunction as starting vector" << endl;

          // indent output of eigenvectors() call if there is any
          cout << incindent << incindent;
          SCF *g = SCF::castdown(guess_wfn_.pointer());
          if (!g || compute_guess_) {
            eigenvectors_ = guess_wfn_->eigenvectors().copy();
            eigenvalues_ = guess_wfn_->eigenvalues().copy();
          } else {
            eigenvectors_ = g->eigenvectors_.result_noupdate().copy();
            eigenvalues_ = g->eigenvalues_.result_noupdate().copy();
            eigenvectors_.result_noupdate()->schmidt_orthog(
              overlap().pointer(), basis()->nbasis());
          }
          cout << decindent << decindent;
        } else {
          cout << node0 << indent
               << "Projecting guess wavefunction into the present basis set"
               << endl;

          // indent output of projected_eigenvectors() call if there is any
          cout << incindent << incindent;
          eigenvectors_ = projected_eigenvectors(guess_wfn_);
          eigenvalues_ = projected_eigenvalues(guess_wfn_);
          cout << decindent << decindent;
        }

        // we should only have to do this once, so free up memory used
        // for the old wavefunction
        guess_wfn_=0;

        cout << node0 << endl;
      
      } else {
        cout << node0 << indent << "Starting from core Hamiltonian guess\n"
             << endl;
        eigenvectors_ = hcore_guess();
        eigenvalues_ = core_hamiltonian().eigvals();
      }
    } else {
      // this is just an old vector, so orthogonalize it
      eigenvectors_.result_noupdate()->schmidt_orthog(overlap().pointer(),
                                                      basis()->nbasis());
    }
  }

  need_vec_=needv;
}

//////////////////////////////////////////////////////////////////////////////

void
SCF::init_mem(int nm)
{
  // if local_den_ is already 0, then that means it was set to zero by
  // the user.
  if (!local_dens_) {
    integral()->set_storage(storage_);
    return;
  }
  
  int nmem = i_offset(basis()->nbasis())*nm*sizeof(double);

  // if we're actually using local matrices, then there's no choice
  if (LocalSCMatrixKit::castdown(basis()->matrixkit().pointer())
      ||ReplSCMatrixKit::castdown(basis()->matrixkit().pointer())) {
    if (nmem > storage_)
      return;
  } else {
    if (nmem > storage_) {
      local_dens_=0;
      integral()->set_storage(storage_);
      return;
    }
  }

  integral()->set_storage(storage_-nmem);
}

/////////////////////////////////////////////////////////////////////////////

void
SCF::so_density(const RefSymmSCMatrix& d, double occ, int alp)
{
  int i,j,k;
  int me=scf_grp_->me();
  int nproc=scf_grp_->n();
  int uhf = spin_unrestricted();
  
  d->assign(0.0);
  
  RefSCMatrix vector;
  if (alp || !uhf)
    vector = scf_vector_;
  else
    vector = scf_vectorb_;
      
  if (vector.null()) {
    if (uhf) {
      if (alp)
        vector = alpha_eigenvectors();
      else
        vector = beta_eigenvectors();
    } else
      vector = eigenvectors();
  }
  
  RefPetiteList pl = integral()->petite_list(basis());
  
  BlockedSCMatrix *bvec = BlockedSCMatrix::require_castdown(
    vector, "SCF::so_density: blocked vector");

  BlockedSymmSCMatrix *bd = BlockedSymmSCMatrix::require_castdown(
    d, "SCF::so_density: blocked density");
  
  for (int ir=0; ir < pl->nirrep(); ir++) {
    RefSCMatrix vir = bvec->block(ir);
    RefSymmSCMatrix dir = bd->block(ir);
    
    if (vir.null() || vir.ncol()==0)
      continue;
    
    int nbasis = vir.ncol();
    
    // figure out which columns of the scf vector we'll need
    int col0 = -1, coln = -1;
    for (i=0; i < nbasis; i++) {
      double occi;
      if (!uhf)
        occi = occupation(ir, i);
      else if (alp)
        occi = alpha_occupation(ir, i);
      else
        occi = beta_occupation(ir, i);

      if (fabs(occi-occ) < 1.0e-8) {
        if (col0 == -1)
          col0 = i;
        continue;
      } else if (col0 != -1) {
        coln = i-1;
        break;
      }
    }

    if (col0 == -1)
      continue;

    if (coln == -1)
      coln = nbasis-1;
    
    if (local_ || local_dens_) {
      RefSymmSCMatrix ldir = dir;

      RefSCMatrix occbits; // holds the occupied bits of the scf vector

      // get local copies of vector and density matrix
      if (!local_) {
        RefSCMatrixKit rk = new ReplSCMatrixKit;
        RefSCMatrix lvir = rk->matrix(vir.rowdim(), vir.coldim());
        lvir->convert(vir);
        occbits = lvir->get_subblock(0, nbasis-1, col0, coln);
        lvir = 0;

        ldir = rk->symmmatrix(dir.dim());
        ldir->convert(dir);

      } else {
        occbits = vir->get_subblock(0, nbasis-1, col0, coln);
      }
    
      double **c;
      double *dens;
      
      if (LocalSCMatrix::castdown(occbits.pointer()))
        c = LocalSCMatrix::castdown(occbits.pointer())->get_rows();
      else if (ReplSCMatrix::castdown(occbits.pointer()))
        c = ReplSCMatrix::castdown(occbits.pointer())->get_rows();
      else
        abort();

      if (LocalSymmSCMatrix::castdown(ldir.pointer()))
        dens = LocalSymmSCMatrix::castdown(ldir.pointer())->get_data();
      else if (ReplSymmSCMatrix::castdown(ldir.pointer()))
        dens = ReplSymmSCMatrix::castdown(ldir.pointer())->get_data();
      else
        abort();

      int ij=0;
      for (i=0; i < nbasis; i++) {
        for (j=0; j <= i; j++, ij++) {
          if (ij%nproc != me)
            continue;
          
          double dv = 0;

          int kk=0;
          for (k=col0; k <= coln; k++, kk++)
            dv += c[i][kk]*c[j][kk];

          dens[ij] = dv;
        }
      }

      if (nproc > 1)
        scf_grp_->sum(dens, i_offset(nbasis));

      if (!local_) {
        dir->convert(ldir);
      }
    }

    // for now quit
    else {
      cerr << node0 << indent
           << "Cannot yet use anything but Local matrices"
           << endl;
      abort();
    }
  }
}

double
SCF::one_body_energy()
{
  RefSymmSCMatrix dens = ao_density().copy();
  RefSymmSCMatrix hcore = dens->clone();
  hcore.assign(0.0);
  RefSCElementOp hcore_op = new OneBodyIntOp(integral()->hcore());
  hcore.element_op(hcore_op);

  dens->scale_diagonal(0.5);
  SCElementScalarProduct *prod = new SCElementScalarProduct;
  prod->reference();
  RefSCElementOp2 op = prod;
  hcore->element_op(prod, dens);
  double e = prod->result();
  op = 0;
  prod->dereference();
  delete prod;
  return 2.0 * e;
}

void
SCF::two_body_energy(double &ec, double &ex)
{
  cerr << class_name() << ": two_body_energy not implemented" << endl;
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "ETS"
// End:
