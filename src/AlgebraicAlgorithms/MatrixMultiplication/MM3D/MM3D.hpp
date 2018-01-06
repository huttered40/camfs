/* Author: Edward Hutter */


static std::tuple<MPI_Comm,
                  MPI_Comm,
                  MPI_Comm,
                  MPI_Comm,
		  int,
                  int,
                  int>
                      setUpCommunicators(MPI_Comm commWorld)
{
  int rank,size;
  MPI_Comm_rank(commWorld, &rank);
  MPI_Comm_size(commWorld, &size);

  int pGridDimensionSize = std::nearbyint(std::ceil(pow(size,1./3.)));
  int helper = pGridDimensionSize;
  helper *= helper;
  int pGridCoordX = rank%pGridDimensionSize;
  int pGridCoordY = (rank%helper)/pGridDimensionSize;
  int pGridCoordZ = rank/helper;

  MPI_Comm rowComm, columnComm, sliceComm, depthComm;

  // First, split the 3D Cube processor grid communicator into groups based on what 2D slice they are located on.
  // Then, subdivide further into row groups and column groups
  MPI_Comm_split(commWorld, pGridCoordY+pGridDimensionSize*pGridCoordX, rank, &depthComm);
  MPI_Comm_split(commWorld, pGridCoordZ, rank, &sliceComm);
  MPI_Comm_split(sliceComm, pGridCoordY, pGridCoordX, &rowComm);
  MPI_Comm_split(sliceComm, pGridCoordX, pGridCoordY, &columnComm);

  return std::make_tuple(rowComm, columnComm, sliceComm, depthComm, pGridCoordX, pGridCoordY, pGridCoordZ);
}


// This algorithm with underlying gemm BLAS routine will allow any Matrix Structure.
//   Of course we will serialize into Square Structure if not in Square Structure already in order to be compatible
//   with BLAS-3 routines.

template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
template<
	  template<typename,typename, template<typename,typename,int> class> class StructureA,
  	  template<typename,typename, template<typename,typename,int> class> class StructureB,
  	  template<typename,typename, template<typename,typename,int> class> class StructureC,
  	  template<typename,typename,int> class Distribution
	>
void MM3D<T,U,blasEngine>::Multiply(
                                   	Matrix<T,U,StructureA,Distribution>& matrixA,
                                        Matrix<T,U,StructureB,Distribution>& matrixB,
                                        Matrix<T,U,StructureC,Distribution>& matrixC,
                                        U localDimensionM,
                                        U localDimensionN,
                                        U localDimensionK,
                                        MPI_Comm commWorld,
                                        const blasEngineArgumentPackage_gemm<T>& srcPackage
                                   )
{
  // Use tuples so we don't have to pass multiple things by reference.
  // Also this way, we can take advantage of the new pass-by-value move semantics that are efficient

  auto commInfo3D = setUpCommunicators(commWorld);
  T* matrixAforEnginePtr;
  T* matrixBforEnginePtr;
  std::vector<T> foreignA;
  std::vector<T> foreignB;

  // soon, we will need a methodKey for the different MM algs
  _start1(matrixA,matrixB,localDimensionM,localDimensionN,localDimensionK,commInfo3D,matrixAforEnginePtr,matrixBforEnginePtr,foreignA,foreignB);

  // Assume, for now, that matrixC has Rectangular Structure. In the future, we can always do the same procedure as above, and add a Serialize after the AllReduce
  T* matrixCforEnginePtr = matrixC.getRawData();

  blasEngine<T,U>::_gemm(matrixAforEnginePtr, matrixBforEnginePtr, matrixCforEnginePtr, localDimensionM, localDimensionN,
    localDimensionK,
    (srcPackage.transposeA == blasEngineTranspose::AblasNoTrans ? localDimensionM : localDimensionK),
    (srcPackage.transposeB == blasEngineTranspose::AblasNoTrans ? localDimensionK : localDimensionN),
    localDimensionM, srcPackage);

  _end1(matrixCforEnginePtr,matrixC,commInfo3D);
}

template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
template<
		template<typename,typename, template<typename,typename,int> class> class StructureA,
  		template<typename,typename, template<typename,typename,int> class> class StructureB,
  		template<typename,typename,int> class Distribution
	>
void MM3D<T,U,blasEngine>::Multiply(
                                   	Matrix<T,U,StructureA,Distribution>& matrixA,
                                        Matrix<T,U,StructureB,Distribution>& matrixB,
                                        U localDimensionM,
                                        U localDimensionN,
                                        MPI_Comm commWorld,
                                        const blasEngineArgumentPackage_trmm<T>& srcPackage
                                   )
{
  // Use tuples so we don't have to pass multiple things by reference.
  // Also this way, we can take advantage of the new pass-by-value move semantics that are efficient

  auto commInfo3D = setUpCommunicators(commWorld);
  T* matrixAforEnginePtr;
  T* matrixBforEnginePtr;
  std::vector<T> foreignA;
  std::vector<T> foreignB;

  // soon, we will need a methodKey for the different MM algs
  if (srcPackage.side == blasEngineSide::AblasLeft)
  {
    _start1(matrixA, matrixB, localDimensionM, localDimensionM,localDimensionN,
      commInfo3D, matrixAforEnginePtr, matrixBforEnginePtr, foreignA, foreignB);
  }
  else
  {
    _start1(matrixB, matrixA,localDimensionM, localDimensionN, localDimensionN,
      commInfo3D, matrixBforEnginePtr, matrixAforEnginePtr, foreignB, foreignA);
  }
  // We will follow the standard here: matrixA is always the triangular matrix. matrixB is always the rectangular matrix

  // debugging
  if (matrixAforEnginePtr != matrixA.getRawData())
  {
    std::cout << "Bad for A\n";
    // print out A? Its good that it was "bad" because matrixA started off as LT and should have been serialied into a rectangle.
  }
  if (matrixBforEnginePtr != matrixB.getRawData())
  {
    std::cout << "Bad for B\n";
  }

  blasEngine<T,U>::_trmm(matrixAforEnginePtr, matrixBforEnginePtr,localDimensionM, localDimensionN,
    (srcPackage.side == blasEngineSide::AblasLeft ? localDimensionM : localDimensionN),
    (srcPackage.order == blasEngineOrder::AblasColumnMajor ? localDimensionM : localDimensionN),
    srcPackage);

  _end1(matrixBforEnginePtr,matrixB,commInfo3D);
}

template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
template<
		template<typename,typename, template<typename,typename,int> class> class StructureA,
  		template<typename,typename, template<typename,typename,int> class> class StructureB,
  		template<typename,typename,int> class Distribution
	>
void MM3D<T,U,blasEngine>::Multiply(
                                   	Matrix<T,U,StructureA,Distribution>& matrixA,
                                        Matrix<T,U,StructureB,Distribution>& matrixB,
                                        U localDimensionN,
                                        U localDimensionK,
                                        MPI_Comm commWorld,
                                        const blasEngineArgumentPackage_syrk<T>& srcPackage
                                   )
{
  // Not correct right now. Will fix later
  MPI_Abort(commWorld, -1);

  // Use tuples so we don't have to pass multiple things by reference.
  // Also this way, we can take advantage of the new pass-by-value move semantics that are efficient

  auto commInfo3D = setUpCommunicators(commWorld);

  // Simple asignments like these don't need pass-by-reference. Remember the new pass-by-value semantics are efficient anyways
  MPI_Comm rowComm = std::get<0>(commInfo3D);
  MPI_Comm transComm = std::get<1>(commInfo3D);
  MPI_Comm sliceComm = std::get<2>(commInfo3D);
  MPI_Comm depthComm = std::get<3>(commInfo3D);
  int pGridCoordX = std::get<4>(commInfo3D);
  int pGridCoordY = std::get<5>(commInfo3D);
  int pGridCoordZ = std::get<6>(commInfo3D);

  std::vector<T>& dataA = matrixA.getVectorData(); 
  std::vector<T> dataAtrans = dataA;			// need to make a copy here I think
  U sizeA = matrixA.getNumElems();
  std::vector<T> foreignA;
  std::vector<T> foreignAtrans;
  bool isRootRow = ((pGridCoordX == pGridCoordZ) ? true : false);
  bool isRootTrans = ((pGridCoordY == pGridCoordZ) ? true : false);

  BroadcastPanels((isRootRow ? dataA : foreignA), sizeA, isRootRow, pGridCoordZ, rowComm);
  BroadcastPanels((isRootTrans ? dataAtrans : foreignAtrans), sizeA, isRootTrans, pGridCoordZ, transComm);

  // Right now, foreignA and/or foreignAtrans might be empty if this processor is the rowRoot or the transRoot
  Matrix<T,U,MatrixStructureRectangle,Distribution> helperA(std::vector<T>(), localDimensionN, localDimensionN, localDimensionN, localDimensionN);
  T* matrixAforEnginePtr = getEnginePtr(matrixA, helperA, (isRootRow ? dataA : foreignA), isRootRow);

  // We assume that matrixB is Square for now. No reason to believe otherwise

  std::vector<T>& matrixBforEngine = matrixB.getVectorData();
  U numElems = matrixB.getNumElems();				// We assume that the user initialized matrixC correctly, even for TRMM

  blasEngine<T,U>::_syrk(matrixAforEnginePtr, &matrixBforEngine[0], (srcPackage.transposeA == blasEngineTranspose::AblasNoTrans ? localDimensionN : localDimensionK),
    (srcPackage.transposeA == blasEngineTranspose::AblasNoTrans ? localDimensionN : localDimensionK),
    (srcPackage.transposeA == blasEngineTranspose::AblasNoTrans ? localDimensionK : localDimensionN),
    (srcPackage.transposeA == blasEngineTranspose::AblasNoTrans ? localDimensionK : localDimensionN),
    srcPackage);

  // in a syrk, we will end up with a symmetric matrix, so we should serialize into packed buffer first to avoid half the communication!
  MPI_Allreduce(MPI_IN_PLACE, &matrixBforEngine[0], numElems, MPI_DOUBLE, MPI_SUM, depthComm);

  MPI_Comm_free(&rowComm);
  MPI_Comm_free(&transComm);
  MPI_Comm_free(&sliceComm);
  MPI_Comm_free(&depthComm);
}

template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
template<
		template<typename,typename, template<typename,typename,int> class> class StructureA,
  		template<typename,typename, template<typename,typename,int> class> class StructureB,
  		template<typename,typename, template<typename,typename,int> class> class StructureC,
  		template<typename,typename,int> class Distribution
	>
void MM3D<T,U,blasEngine>::Multiply(
                                   	Matrix<T,U,StructureA,Distribution>& matrixA,
                                        Matrix<T,U,StructureB,Distribution>& matrixB,
				        Matrix<T,U,StructureC,Distribution>& matrixC,
				        U matrixAcutXstart,
				        U matrixAcutXend,
				        U matrixAcutYstart,
				        U matrixAcutYend,
				        U matrixBcutZstart,
				        U matrixBcutZend,
				        U matrixBcutXstart,
				        U matrixBcutXend,
				        U matrixCcutZstart,
				        U matrixCcutZend,
				        U matrixCcutYstart,
				        U matrixCcutYend,
				        MPI_Comm commWorld,
				        const blasEngineArgumentPackage_gemm<T>& srcPackage,
				        bool cutA,
				        bool cutB,
				        bool cutC
                                   )
{
  // We will set up 3 matrices and call the method above.

  U rangeA_x = matrixAcutXend-matrixAcutXstart;
  U rangeA_y = matrixAcutYend-matrixAcutYstart;
  U rangeB_z = matrixBcutZend-matrixBcutZstart;
  U rangeB_x = matrixBcutXend-matrixBcutXstart;
  U rangeC_z = matrixCcutZend - matrixCcutZstart;
  U rangeC_y = matrixCcutYend - matrixCcutYstart; 
  U globalDiffA = matrixA.getNumRowsGlobal() / matrixA.getNumRowsLocal();		// picked rows arbitrarily
  U globalDiffB = matrixB.getNumRowsGlobal() / matrixB.getNumRowsLocal();		// picked rows arbitrarily
  U globalDiffC = matrixC.getNumRowsGlobal() / matrixC.getNumRowsLocal();		// picked rows arbitrarily

  U sizeA = matrixA.getNumElems(rangeA_x, rangeA_y);
  U sizeB = matrixB.getNumElems(rangeB_z, rangeB_x);
  U sizeC = matrixC.getNumElems(rangeC_y, rangeC_z);

  // I cannot use a fast-pass-by-value via move constructor because I don't want to corrupt the true matrices A,B,C. Other reasons as well.
  Matrix<T,U,StructureA,Distribution> subMatrixA(std::vector<T>(), rangeA_x, rangeA_y, rangeA_x*globalDiffA, rangeA_y*globalDiffA);
  Matrix<T,U,StructureB,Distribution> subMatrixB(std::vector<T>(), rangeB_z, rangeB_x, rangeB_z*globalDiffB, rangeB_x*globalDiffB);
  Matrix<T,U,StructureC,Distribution> subMatrixC(std::vector<T>(), rangeC_z, rangeC_y, rangeC_z*globalDiffC, rangeC_y*globalDiffC);
  Matrix<T,U,StructureA,Distribution>& matA = getSubMatrix(matrixA, subMatrixA, matrixAcutXstart, matrixAcutXend, matrixAcutYstart, matrixAcutYend, globalDiffA, cutA);
  Matrix<T,U,StructureB,Distribution>& matB = getSubMatrix(matrixB, subMatrixB, matrixBcutZstart, matrixBcutZend, matrixBcutXstart, matrixBcutXend, globalDiffB, cutB);
  Matrix<T,U,StructureC,Distribution>& matC = getSubMatrix(matrixC, subMatrixC, matrixCcutZstart, matrixCcutZend, matrixCcutYstart, matrixCcutYend, globalDiffC, cutC);

  Multiply(matA, matB, matC, rangeA_x, rangeA_y, rangeB_z, commWorld, srcPackage);

  // reverse serialize, to put the solved piece of matrixC into where it should go.
  if (cutC)
  {
    Serializer<T,U,StructureC,StructureC>::Serialize(matrixC, matC,
      matrixCcutZstart, matrixCcutZend, matrixCcutYstart, matrixCcutYend, true);
  }
}


template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
template<
		template<typename,typename, template<typename,typename,int> class> class StructureA,
  		template<typename,typename, template<typename,typename,int> class> class StructureB,
  		template<typename,typename,int> class Distribution
	 >
void MM3D<T,U,blasEngine>::Multiply(
				      Matrix<T,U,StructureA,Distribution>& matrixA,
				      Matrix<T,U,StructureB,Distribution>& matrixB,
				      U matrixAcutXstart,
				      U matrixAcutXend,
				      U matrixAcutYstart,
				      U matrixAcutYend,
				      U matrixBcutZstart,
				      U matrixBcutZend,
				      U matrixBcutXstart,
				      U matrixBcutXend,
				      MPI_Comm commWorld,
				      const blasEngineArgumentPackage_trmm<T>& srcPackage,
				      bool cutA,
				      bool cutB
                                    )
{
  // We will set up 3 matrices and call the method above.

  U rangeA_x = matrixAcutXend-matrixAcutXstart;
  U rangeA_y = matrixAcutYend-matrixAcutYstart;
  U rangeB_x = matrixBcutXend-matrixBcutXstart;
  U rangeB_z = matrixBcutZend-matrixBcutZstart;
  U globalDiffA = matrixA.getNumRowsGlobal() / matrixA.getNumRowsLocal();		// picked rows arbitrarily
  U globalDiffB = matrixB.getNumRowsGlobal() / matrixB.getNumRowsLocal();		// picked rows arbitrarily

  U sizeA = matrixA.getNumElems(rangeA_x, rangeA_y);
  U sizeB = matrixB.getNumElems(rangeB_z, rangeB_x);

  // I cannot use a fast-pass-by-value via move constructor because I don't want to corrupt the true matrices A,B,C. Other reasons as well.
  Matrix<T,U,StructureA,Distribution> subMatrixA(std::vector<T>(), rangeA_x, rangeA_y, rangeA_x*globalDiffA, rangeA_y*globalDiffA);
  Matrix<T,U,StructureB,Distribution> subMatrixB(std::vector<T>(), rangeB_z, rangeB_x, rangeB_z*globalDiffB, rangeB_x*globalDiffB);
  Matrix<T,U,StructureA,Distribution>& matA = getSubMatrix(matrixA, subMatrixA, matrixAcutXstart, matrixAcutXend, matrixAcutYstart, matrixAcutYend, globalDiffA, cutA);
  Matrix<T,U,StructureB,Distribution>& matB = getSubMatrix(matrixB, subMatrixB, matrixBcutZstart, matrixBcutZend, matrixBcutXstart, matrixBcutXend, globalDiffB, cutB);

  Multiply(matA, matB, rangeA_x, rangeA_y, rangeB_z, commWorld, srcPackage);

  // reverse serialize, to put the solved piece of matrixC into where it should go. Only if we need to
  if (cutB)
  {
    Serializer<T,U,StructureB,StructureB>::Serialize(matrixB, matB,
      matrixBcutZstart, matrixBcutZend, matrixBcutXstart, matrixBcutXend, true);
  }
}


template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
template<
		template<typename,typename, template<typename,typename,int> class> class StructureA,
  		template<typename,typename, template<typename,typename,int> class> class StructureB,
  		template<typename,typename,int> class Distribution
	 >
void MM3D<T,U,blasEngine>::Multiply(
				      Matrix<T,U,StructureA,Distribution>& matrixA,
				      Matrix<T,U,StructureB,Distribution>& matrixB,
				      U matrixAcutXstart,
				      U matrixAcutXend,
				      U matrixAcutYstart,
				      U matrixAcutYend,
				      U matrixBcutZstart,
				      U matrixBcutZend,
				      U matrixBcutXstart,
				      U matrixBcutXend,
				      MPI_Comm commWorld,
				      const blasEngineArgumentPackage_syrk<T>& srcPackage,
				      bool cutA,
				      bool cutB
				    )
{
  // We will set up 3 matrices and call the method above.

  U rangeA_x = matrixAcutXend-matrixAcutXstart;
  U rangeA_y = matrixAcutYend-matrixAcutYstart;
  U rangeB_z = matrixBcutZend-matrixBcutZstart;
  U rangeB_x = matrixBcutXend-matrixBcutXstart;
  U globalDiffA = matrixA.getNumRowsGlobal() / matrixA.getNumRowsLocal();		// picked rows arbitrarily
  U globalDiffB = matrixB.getNumRowsGlobal() / matrixB.getNumRowsLocal();		// picked rows arbitrarily

  U sizeA = matrixA.getNumElems(rangeA_x, rangeA_y);
  U sizeB = matrixB.getNumElems(rangeB_z, rangeB_x);

  // I cannot use a fast-pass-by-value via move constructor because I don't want to corrupt the true matrices A,B,C. Other reasons as well.
  Matrix<T,U,StructureA,Distribution> subMatrixA(std::vector<T>(), rangeA_x, rangeA_y, rangeA_x*globalDiffA, rangeA_y*globalDiffA);
  Matrix<T,U,StructureB,Distribution> subMatrixB(std::vector<T>(), rangeB_z, rangeB_x, rangeB_z*globalDiffB, rangeB_x*globalDiffB);
  Matrix<T,U,StructureA,Distribution>& matA = getSubMatrix(matrixA, subMatrixA, matrixAcutXstart, matrixAcutXend, matrixAcutYstart, matrixAcutYend, globalDiffA, cutA);
  Matrix<T,U,StructureB,Distribution>& matB = getSubMatrix(matrixB, subMatrixB, matrixBcutZstart, matrixBcutZend, matrixBcutXstart, matrixBcutXend, globalDiffB, cutB);

  Multiply(matA, matB, rangeA_x, rangeA_y, rangeB_z, commWorld, srcPackage);

  // reverse serialize, to put the solved piece of matrixC into where it should go. Only if we need to
  if (cutB)
  {
    Serializer<T,U,StructureB,StructureB>::Serialize(matrixB, matB,
      matrixBcutZstart, matrixBcutZend, matrixBcutXstart, matrixBcutXend, true);
  }
}


template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
template<template<typename,typename,int> class Distribution,
  template<typename,typename, template<typename,typename,int> class> class StructureArg1,
  template<typename,typename, template<typename,typename,int> class> class StructureArg2,
  typename tupleStructure>
void MM3D<T,U,blasEngine>::_start1(
					Matrix<T,U,StructureArg1,Distribution>& matrixA,
					Matrix<T,U,StructureArg2,Distribution>& matrixB,
					U localDimensionM,
					U localDimensionN,
					U localDimensionK,
					tupleStructure& commInfo3D,
					T*& matrixAforEnginePtr,
					T*& matrixBforEnginePtr,
					std::vector<T>& foreignA,
					std::vector<T>& foreignB
				  )
{
  // Simple asignments like these don't need pass-by-reference. Remember the new pass-by-value semantics are efficient anyways
  MPI_Comm rowComm = std::get<0>(commInfo3D);
  MPI_Comm columnComm = std::get<1>(commInfo3D);
  MPI_Comm sliceComm = std::get<2>(commInfo3D);
  MPI_Comm depthComm = std::get<3>(commInfo3D);
  int pGridCoordX = std::get<4>(commInfo3D);
  int pGridCoordY = std::get<5>(commInfo3D);
  int pGridCoordZ = std::get<6>(commInfo3D);

  std::vector<T>& dataA = matrixA.getVectorData(); 
  std::vector<T>& dataB = matrixB.getVectorData();
  U sizeA = matrixA.getNumElems();
  U sizeB = matrixB.getNumElems();
  bool isRootRow = ((pGridCoordX == pGridCoordZ) ? true : false);
  bool isRootColumn = ((pGridCoordY == pGridCoordZ) ? true : false);

  BroadcastPanels((isRootRow ? dataA : foreignA), sizeA, isRootRow, pGridCoordZ, rowComm);
  BroadcastPanels((isRootColumn ? dataB : foreignB), sizeB, isRootColumn, pGridCoordZ, columnComm);

  matrixAforEnginePtr = (isRootRow ? &dataA[0] : &foreignA[0]);
  matrixBforEnginePtr = (isRootColumn ? &dataB[0] : &foreignB[0]);
  if ((!std::is_same<StructureArg1<T,U,Distribution>,MatrixStructureRectangle<T,U,Distribution>>::value)
    && (!std::is_same<StructureArg1<T,U,Distribution>,MatrixStructureSquare<T,U,Distribution>>::value))		// compile time if statement. Branch prediction should be correct.
  {
    Matrix<T,U,MatrixStructureRectangle,Distribution> helperA(std::vector<T>(), localDimensionK, localDimensionM, localDimensionK, localDimensionM);
    matrixAforEnginePtr = getEnginePtr(matrixA, helperA, (isRootRow ? dataA : foreignA), isRootRow);
  }
  if ((!std::is_same<StructureArg2<T,U,Distribution>,MatrixStructureRectangle<T,U,Distribution>>::value)
    && (!std::is_same<StructureArg2<T,U,Distribution>,MatrixStructureSquare<T,U,Distribution>>::value))		// compile time if statement. Branch prediction should be correct.
  {
    Matrix<T,U,MatrixStructureRectangle,Distribution> helperB(std::vector<T>(), localDimensionN, localDimensionK, localDimensionN, localDimensionK);
    matrixBforEnginePtr = getEnginePtr(matrixB, helperB, (isRootColumn ? dataB : foreignB), isRootColumn);
  }
}


template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
template<template<typename,typename, template<typename,typename,int> class> class StructureArg,template<typename,typename,int> class Distribution,
  typename tupleStructure>
void MM3D<T,U,blasEngine>::_end1(
					T* matrixEnginePtr,
					Matrix<T,U,StructureArg,Distribution>& matrix,
					tupleStructure& commInfo3D
				)
{
  // Simple asignments like these don't need pass-by-reference. Remember the new pass-by-value semantics are efficient anyways
  MPI_Comm rowComm = std::get<0>(commInfo3D);
  MPI_Comm columnComm = std::get<1>(commInfo3D);
  MPI_Comm sliceComm = std::get<2>(commInfo3D);
  MPI_Comm depthComm = std::get<3>(commInfo3D);

  U numElems = matrix.getNumElems();

  // Prevents buffer aliasing, which MPI does not like.
  if (matrixEnginePtr == matrix.getRawData())
  {
    MPI_Allreduce(MPI_IN_PLACE,matrixEnginePtr, numElems, MPI_DOUBLE, MPI_SUM, depthComm);
  }
  else
  {
    MPI_Allreduce(matrixEnginePtr, matrix.getRawData(), numElems, MPI_DOUBLE, MPI_SUM, depthComm);
  }

  MPI_Comm_free(&rowComm);
  MPI_Comm_free(&columnComm);
  MPI_Comm_free(&sliceComm);
  MPI_Comm_free(&depthComm);
}


template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
void MM3D<T,U,blasEngine>::BroadcastPanels(
						std::vector<T>& data,
						U size,
						bool isRoot,
						int pGridCoordZ,
						MPI_Comm panelComm
					   )
{
  if (isRoot)
  {
    MPI_Bcast(&data[0], size, MPI_DOUBLE, pGridCoordZ, panelComm);
  }
  else
  {
    data.resize(size);
    MPI_Bcast(&data[0], size, MPI_DOUBLE, pGridCoordZ, panelComm);
  }
}


template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
template<template<typename,typename, template<typename,typename,int> class> class StructureArg,
  template<typename,typename,int> class Distribution>					// Added additional template parameters just for this method
T* MM3D<T,U,blasEngine>::getEnginePtr(
					Matrix<T,U,StructureArg, Distribution>& matrixArg,
					Matrix<T,U,MatrixStructureRectangle, Distribution>& matrixDest,
					std::vector<T>& data,
					bool isRoot
				     )
{
  // Need to separate the below out into its own function that will not get instantied into object code
  //   unless it passes the test above. This avoids template-enduced template compiler errors
  if (!isRoot)
  {
    Matrix<T,U,StructureArg,Distribution> matrixToSerialize(std::move(data), matrixArg.getNumColumnsLocal(),
      matrixArg.getNumRowsLocal(), matrixArg.getNumColumnsGlobal(), matrixArg.getNumRowsGlobal(), true);
    Serializer<T,U,StructureArg,MatrixStructureRectangle>::Serialize(matrixToSerialize, matrixDest);
  }
  else
  {
    // If code path gets here, StructureArg must be a LT or UT, so we need to serialize into a Square, not a Rectangle
    Serializer<T,U,StructureArg,MatrixStructureRectangle>::Serialize(matrixArg, matrixDest);
  }
  return matrixDest.getRawData();		// this is being deleted off the stack! It is a local variable!!!!!!
}


template<typename T, typename U, template<typename,typename> class blasEngine>							// Defaulted to cblasEngine
template<template<typename,typename, template<typename,typename,int> class> class StructureArg,
  template<typename,typename,int> class Distribution>					// Added additional template parameters just for this method
Matrix<T,U,StructureArg,Distribution>& MM3D<T,U,blasEngine>::getSubMatrix(
								Matrix<T,U,StructureArg, Distribution>& srcMatrix,	// pass by value via move constructor
								Matrix<T,U,StructureArg, Distribution>& fillMatrix,	// pass by value via move constructor
								U matrixArgColumnStart,
								U matrixArgColumnEnd,
								U matrixArgRowStart,
								U matrixArgRowEnd,
								U globalDiff,
								bool getSub
						       )
{
  if (getSub)
  {
    U rangeC_column = matrixArgColumnEnd - matrixArgColumnStart;
    U rangeC_row = matrixArgRowEnd - matrixArgRowStart;
    Serializer<T,U,StructureArg,StructureArg>::Serialize(srcMatrix, fillMatrix,
      matrixArgColumnStart, matrixArgColumnEnd, matrixArgRowStart, matrixArgRowEnd);
    return fillMatrix;			// I am returning a lvalue reference to a lvalue reference
  }
  else
  {
    return srcMatrix;
  }
}
