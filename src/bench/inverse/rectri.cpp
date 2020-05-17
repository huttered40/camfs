/* Author: Edward Hutter */

#include "../../alg/inverse/rectri/rectri.h"
//#include "../../test/inverse/validate.h"

using namespace std;

int main(int argc, char** argv){
  using T = double; using U = int64_t; using MatrixType = matrix<T,U,lowertri>; using namespace inverse;

  int rank,size,provided; MPI_Init_thread(&argc, &argv, MPI_THREAD_SINGLE, &provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank); MPI_Comm_size(MPI_COMM_WORLD, &size);

  char dir          = 'L';
  U num_rows        = atoi(argv[1]);// number of rows in global matrix
  U rep_div         = atoi(argv[2]);// cuts the depth of cubic process grid (only trivial support of value '1' is supported)
  size_t num_chunks = atoi(argv[3]);// splits up communication in summa into nonblocking chunks
  size_t num_iter   = atoi(argv[4]);// number of simulations of the algorithm for performance testing
  size_t factor     = atoi(argv[5]);// factor by which to multiply the critter stats internally
  size_t id         = atoi(argv[6]);

  using trtri_type = typename inverse::rectri<policy::rectri::NoSerialize,policy::rectri::SaveIntermediates>;
  size_t process_cube_dim = std::nearbyint(std::ceil(pow(size,1./3.)));
  size_t rep_factor = process_cube_dim/rep_div; double time_global;
  T residual_error_local,residual_error_global; auto mpi_dtype = mpi_type<T>::type;
  { 
    auto SquareTopo = topo::square(MPI_COMM_WORLD,rep_factor,num_chunks,true);
    MatrixType A(num_rows,num_rows, SquareTopo.d, SquareTopo.d);
    A.distribute_random(SquareTopo.x, SquareTopo.y, SquareTopo.d, SquareTopo.d, rank/SquareTopo.c);

    // Generate algorithmic structure via instantiating packs
    trtri_type::info<T,U> pack(dir);

    for (size_t i=0; i<num_iter; i++){
      MPI_Barrier(MPI_COMM_WORLD);
      if (id != 3) critter::start(id);
      trtri_type::invoke(A, pack, SquareTopo);
      if (id != 3) critter::stop(id,factor);
      if (id==3){
        trtri_type::invoke(A, pack, SquareTopo);
//        residual_error_local = trtri::validate<trtri_type>::residual(A, pack, SquareTopo);
        MPI_Reduce(&residual_error_local, &residual_error_global, 1, mpi_dtype, MPI_MAX, 0, MPI_COMM_WORLD);
        if (rank==0){ std::cout << residual_error_global << std::endl; }
      }
    }

  }
  MPI_Finalize();
  return 0;
}
