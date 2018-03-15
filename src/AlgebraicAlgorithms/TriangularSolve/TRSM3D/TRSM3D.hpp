/* Author: Edward Hutter */

template<typename T, typename U, template<typename, typename> class blasEngine>
template<
  template<typename,typename, template<typename,typename,int> class> class StructureArg,
  template<typename,typename, template<typename,typename,int> class> class StructureTriangularArg,
  template<typename,typename,int> class Distribution
>
void TRSM3D<T,U,blasEngine>::iSolveLowerLeft(
  Matrix<T,U,StructureArg,Distribution>& matrixA,
  Matrix<T,U,StructureTriangularArg,Distribution>& matrixL,
  Matrix<T,U,StructureTriangularArg,Distribution>& matrixLI,
  Matrix<T,U,StructureArg,Distribution>& matrixB,
  std::vector<U>& baseCaseDimList,
  blasEngineArgumentPackage_gemm<T>& srcPackage,
  MPI_Comm commWorld,
  std::tuple<MPI_Comm,MPI_Comm,MPI_Comm,MPI_Comm,int,int,int>& commInfo3D,
  int MM_id,
  int TR_id)
{
}


// For solving AU=B for A. But note that B is A, and we are modifying B in place to solve for A
template<typename T, typename U, template<typename, typename> class blasEngine>
template<
  template<typename,typename, template<typename,typename,int> class> class StructureArg,
  template<typename,typename, template<typename,typename,int> class> class StructureTriangularArg,
  template<typename,typename,int> class Distribution
>
void TRSM3D<T,U,blasEngine>::iSolveUpperLeft(
                       Matrix<T,U,StructureArg,Distribution>& matrixA,
                       Matrix<T,U,StructureTriangularArg,Distribution>& matrixU,
                       Matrix<T,U,StructureTriangularArg,Distribution>& matrixUI,
                       std::vector<U>& baseCaseDimList,
                       blasEngineArgumentPackage_gemm<T>& srcPackage,
                       MPI_Comm commWorld,
                       std::tuple<MPI_Comm,MPI_Comm,MPI_Comm,MPI_Comm,int,int,int>& commInfo3D,
                       int MM_id,
                       int TR_id         // allows for benchmarking to see which version is faster 
                     )
{
  TAU_FSTART(TRSM3D::iSolveUpperLeft);
  int pGridDimensionSize;
  MPI_Comm_size(std::get<0>(commInfo3D), &pGridDimensionSize);
/*
  // debugging
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
*/
  blasEngineArgumentPackage_trmm<T> trmmPackage;
  trmmPackage.order = blasEngineOrder::AblasColumnMajor;
  trmmPackage.side = blasEngineSide::AblasRight;
  trmmPackage.uplo = blasEngineUpLo::AblasLower;
  trmmPackage.diag = blasEngineDiag::AblasNonUnit;
  trmmPackage.transposeA = blasEngineTranspose::AblasTrans;
  trmmPackage.alpha = 1.;

  // to catch debugging issues, assert that this has at least one size
  assert(baseCaseDimList.size());

  // Lets operate on individual columns at a time
  // Potential optimization 1): Don't use MM3D if the columns are too skinny in relation to the block size!
     // Or this could just be taken care of when we tune block sizes?
  // Potential optimization 2) Lots of serializing going on with each MM3D, this needs to be reduced.

  // Communicate matrixA and matrixU and matrixUI immediately.
    // These 3 matrices should never need to be communicated again.
  // matrixB however will need to be AllReduced at each iteration so that final results can be summed and updated before next iteration

  U matAendX = matrixA.getNumColumnsLocal();
  U matAendY = matrixA.getNumRowsLocal();
  U matUendX = matrixU.getNumColumnsLocal();
  U matUendY = matrixU.getNumRowsLocal();

  U offset1 = 0;
  U offset2 = (baseCaseDimList.size() < 1 ? matAendX : baseCaseDimList[0]);
  U offset3 = 0;
  for (U i=0; i<baseCaseDimList.size()/*numBlockColumns*/; i++)
  {
    // Update the current column by accumulating the updates via MM
    srcPackage.alpha = -1;
    srcPackage.beta = 1.;

    // Only update once first panel is solved
    if (i>0)
    {
      // As i increases, the size of these updates gets smaller.
      // Special handling. This might only work since the triangular matrix is square, which should be ok
      U arg1 = (srcPackage.transposeB == blasEngineTranspose::AblasNoTrans ? offset1 : offset3);
      U arg2 = (srcPackage.transposeB == blasEngineTranspose::AblasNoTrans ? matUendX : offset1);
      U arg3 = (srcPackage.transposeB == blasEngineTranspose::AblasNoTrans ? offset3 : offset1);
      U arg4 = (srcPackage.transposeB == blasEngineTranspose::AblasNoTrans ? offset1 : matUendX);

      // NOTE: the serialized matrix below is not actually square
      Matrix<T,U,MatrixStructureSquare,Distribution> matrixUpartition(std::vector<T>(), arg2-arg1, arg4-arg3, (arg2-arg1)*pGridDimensionSize, (arg4-arg3)*pGridDimensionSize);
      Serializer<T,U,StructureTriangularArg,MatrixStructureSquare>::Serialize(matrixU, matrixUpartition,
        arg1, arg2, arg3, arg4);

      if (rank == 0) matrixUpartition.print();

      MM3D<T,U,blasEngine>::Multiply(
        matrixA.getRawData()+(offset3*matAendY), matrixUpartition, matrixA.getRawData()+(offset1*matAendY),
        offset1-offset3, matAendY, arg2-arg1, arg4-arg3, matAendX-offset1, matAendY, commWorld, commInfo3D, srcPackage);
    }

    // Solve via TRMM
    U save1 = offset2-offset1;
    // New optimization: prevent this copy if we are doing TRSM only at the top level
    if (baseCaseDimList.size() <= 1)
    {
      MM3D<T,U,blasEngine>::Multiply(
        matrixUI, matrixA.getRawData()+(offset1*matAendY),
        save1, save1, save1, matAendY, commWorld, commInfo3D, trmmPackage);
    }
    else
    {
      Matrix<T,U,StructureTriangularArg,Distribution> matrixUIpartition(std::vector<T>(), save1, save1, save1*pGridDimensionSize, save1*pGridDimensionSize);
      Serializer<T,U,StructureTriangularArg,StructureTriangularArg>::Serialize(matrixUI, matrixUIpartition,
        offset1, offset2, offset1, offset2);
      MM3D<T,U,blasEngine>::Multiply(
        matrixUIpartition, matrixA.getRawData()+(offset1*matAendY),
        save1, save1, save1, matAendY, commWorld, commInfo3D, trmmPackage);
    }
    if ((i+1) < baseCaseDimList.size())
    {
      // Update the offsets
      offset3 = offset1;
      offset1 = offset2;
      offset2 += baseCaseDimList[i+1];
    }
  }
  TAU_FSTOP(TRSM3D::iSolveUpperLeft);
}


// For solving RA=B for A
template<typename T, typename U, template<typename, typename> class blasEngine>
template<
  template<typename,typename, template<typename,typename,int> class> class StructureArg,
  template<typename,typename,int> class Distribution
>
void TRSM3D<T,U,blasEngine>::iSolveLowerRight(
  Matrix<T,U,MatrixStructureSquare,Distribution>& matrixR,
  Matrix<T,U,MatrixStructureSquare,Distribution>& matrixRI,
  Matrix<T,U,StructureArg,Distribution>& matrixA,
  Matrix<T,U,StructureArg,Distribution>& matrixB,
  std::vector<U>& baseCaseDimList,
  blasEngineArgumentPackage_gemm<T>& srcPackage,
  MPI_Comm commWorld,
  std::tuple<MPI_Comm,MPI_Comm,MPI_Comm,MPI_Comm,int,int,int>& commInfo3D,
  int MM_id,
  int TR_id)         // allows for benchmarking to see which version is faster 
{
  TAU_FSTART(TRSM3D::iSolveLowerRight);
  int pGridDimensionSize;
  MPI_Comm_size(std::get<0>(commInfo3D), &pGridDimensionSize);

  // to catch debugging issues, assert that this has at least one size
  assert(baseCaseDimList.size());

  // Lets operate on individual columns at a time
  // Potential optimization 1): Don't use MM3D if the columns are too skinny in relation to the block size!
     // Or this could just be taken care of when we tune block sizes?
  // Potential optimization 2) Lots of serializing going on with each MM3D, this needs to be reduced.

  U matAendX = matrixA.getNumColumnsLocal();
  U matAendY = matrixA.getNumRowsLocal();
  U matRendX = matrixR.getNumColumnsLocal();
  U matRendY = matrixR.getNumRowsLocal();
  U matBendX = matrixB.getNumColumnsLocal();
  U matBendY = matrixB.getNumRowsLocal();

  U offset1 = 0;
  U offset2 = (baseCaseDimList.size() < 1 ? matAendX : baseCaseDimList[0]);
  U offset3 = 0;
  for (U i=0; i<baseCaseDimList.size()/*numBlockColumns*/; i++)
  {

    // Update the current column by accumulating the updates via MM
    srcPackage.alpha = -1;
    srcPackage.beta = 1.;

    // Only update once first panel is solved
    if (i>0)
    {
      // As i increases, the size of these updates gets smaller.
      // Special handling. This might only work since the triangular matrix is square, which should be ok

      // Note that the beginning cases might not be correct. They are not currently used for anything though.
      U arg1 = (srcPackage.transposeA == blasEngineTranspose::AblasNoTrans ? offset3 : offset1);
      U arg2 = (srcPackage.transposeA == blasEngineTranspose::AblasNoTrans ? offset1 : matRendX);
      U arg3 = (srcPackage.transposeA == blasEngineTranspose::AblasNoTrans ? offset3 : offset3);
      U arg4 = (srcPackage.transposeA == blasEngineTranspose::AblasNoTrans ? offset1 : offset1);

      MM3D<T,U,blasEngine>::Multiply(
        matrixR, matrixA, matrixB, arg1, arg2, arg3, arg4, 0, matAendX, offset3, offset1,
        0, matBendX, offset1, matBendY, commWorld, commInfo3D, srcPackage, true, true, true, MM_id);
    }

    // Solve via MM
    // Future optimization: We are doing the same serialization over and over again between the updates and the MM. Try to reduce this!
    srcPackage.alpha = 1;
    srcPackage.beta = 0;
    // Future optimization: for 1 processor, we don't want to serialize, so change true to false
    MM3D<T,U,blasEngine>::Multiply(
      matrixRI, matrixB, matrixA, offset1, offset2, offset1, offset2,
      0, matBendX, offset1, offset2, 0, matAendX,
      offset1, offset2, commWorld, commInfo3D, srcPackage, true, true, true, MM_id);

    if ((i+1) < baseCaseDimList.size())
    {
      // Update the offsets
      offset3 = offset1;
      offset1 = offset2;
      offset2 += baseCaseDimList[i+1];
    }
  }
  TAU_FSTOP(TRSM3D::iSolveLowerRight);
}


template<typename T, typename U, template<typename, typename> class blasEngine>
template<
  template<typename,typename, template<typename,typename,int> class> class StructureArg,
  template<typename,typename,int> class Distribution
>
void TRSM3D<T,U,blasEngine>::iSolveUpperRight(
  Matrix<T,U,MatrixStructureSquare,Distribution>& matrixU,
  Matrix<T,U,MatrixStructureSquare,Distribution>& matrixUI,
  Matrix<T,U,StructureArg,Distribution>& matrixA,
  Matrix<T,U,StructureArg,Distribution>& matrixB,
  std::vector<U>& baseCaseDimList,
  blasEngineArgumentPackage_gemm<T>& srcPackage,
  MPI_Comm commWorld,
  std::tuple<MPI_Comm,MPI_Comm,MPI_Comm,MPI_Comm,int,int,int>& commInfo3D,
  int MM_id,
  int TR_id)         // allows for benchmarking to see which version is faster 
{
}
