/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 3.0
//       Copyright (2020) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY NTESS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NTESS OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Christian R. Trott (crtrott@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#include <Kokkos_Core.hpp>
#include <Kokkos_Random.hpp>
#include <Kokkos_ScatterView.hpp>

using particle_pos_t = Kokkos::View<double*[3]>;

template<class particle_val_t, class grid_t, class grid_sv_t>
void discretize(double dx, particle_pos_t pos, particle_val_t value, grid_t grid, grid_sv_t grid_sv) {
  grid_sv.reset();
  Kokkos::parallel_for("Deposit", pos.extent(0), KOKKOS_LAMBDA(int i) {
    auto grid_access = grid_sv.access();
    int x = pos(i,0)/dx;
    int y = pos(i,1)/dx;
    int z = pos(i,2)/dx;
    grid_access(x,y,z)+=value(i);
  });
  Kokkos::Experimental::contribute(grid,grid_sv);
}

template<class T>
double run_test(int R, double dx, particle_pos_t pos, const int grid_size) {
  Kokkos::View<T*> values("Values",pos.extent(0));
  Kokkos::View<T***> grid("grid",grid_size+1,grid_size+1,grid_size+1);
  Kokkos::deep_copy(values, T(1.));
  Kokkos::Experimental::ScatterView<T***> grid_sv(grid);
  Kokkos::fence();
  discretize(dx,pos,values,grid,grid_sv);

  Kokkos::fence();
  Kokkos::Timer timer;
  for(int r=0; r<R; r++)
    discretize(dx,pos,values,grid,grid_sv);
  Kokkos::fence();
  double time = timer.seconds();
  return time;
}

int main(int argc, char *argv[]) {
  Kokkos::ScopeGuard guard(argc, argv);

  int const num_particles = argc>1?atoi(argv[1]):10000000;
  int const grid_size = argc>2?atoi(argv[2]):20;
  const int R = argc>3?atoi(argv[3]):20;
  double dx = 1./grid_size;

  particle_pos_t pos("a",num_particles);
  Kokkos::Random_XorShift64_Pool<> g(1931);
    Kokkos::fill_random(pos, g, 1.);

  Kokkos::fence();

  double time_int = run_test<int>(R, dx, pos, grid_size);
  double time_double = run_test<double>(R, dx, pos, grid_size);
  double time_complex = run_test<Kokkos::complex<double>>(R, dx, pos, grid_size);

  double gup = 1.e-9*num_particles*R;
  printf("N: %i Grid: %i Time: %e %e %e GUPS: %e %e %e\n", num_particles, grid_size,
    time_int/R, time_double/R, time_complex/R, gup/time_int, gup/time_double, gup/time_complex);
  return 0;
}