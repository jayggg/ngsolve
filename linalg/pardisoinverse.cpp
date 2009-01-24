/* *************************************************************************/
/* File:   pardisoinverse.cpp                                              */
/* Author: Florian Bachinger                                               */
/* Date:   Feb. 2004                                                       */
/* *************************************************************************/

#include <la.hpp>

#ifdef USE_PARDISO

#define F77_FUNC(func)  func ## _

extern "C"
{
  /* PARDISO prototype. */

#ifndef USE_MKL
  int F77_FUNC(pardisoinit)
    (void *, int *, int *);
#endif

  int F77_FUNC(pardiso)
    (void *, int *, int *, int *, int *, int *, 
     double *, int *, int *, int *, int *, int *, 
     int *, double *, double *, int *);
}






namespace ngla
{
  using namespace ngla;
  using namespace ngstd;


namespace pardisofunc
{
  using namespace pardisofunc;
  template<class TM>
  int NumRows(TM entry)
  {
    return entry.Height();
  }
  int NumRows(double entry) { return 1; }
  int NumRows(Complex entry) { return 1; }


  template<class TM>
  int NumCols(TM entry)
  {
    return entry.Width();
  }
  int NumCols(double entry) { return 1; }
  int NumCols(Complex entry) { return 1; }


  template<class TM>
  typename TM::TSCAL Elem(TM entry, int i, int j)
  {
    return entry(i,j);
  }
  double Elem (double entry, int i, int j) { return entry; }
  Complex Elem (Complex entry, int i, int j) { return entry; }



  template<class TM>
  int IsComplex(TM entry)
  {
    return ( sizeof( Elem(entry,0,0) ) == sizeof( Complex ) );
  }

}

  using namespace pardisofunc;

  template<class TM, class TV_ROW, class TV_COL>
  PardisoInverse<TM,TV_ROW,TV_COL> :: 
  PardisoInverse (const SparseMatrix<TM,TV_ROW,TV_COL> & a, 
		  const BitArray * ainner,
		  const Array<int> * acluster,
		  int asymmetric)
  { 
    static int timer = NgProfiler::CreateTimer ("Pardiso Inverse");
    NgProfiler::RegionTimer reg (timer);

    //    (*testout) << "matrix = " << a << endl;

    symmetric = asymmetric;
    inner = ainner;
    cluster = acluster;

    (*testout) << "Pardiso, symmetric = " << symmetric << endl;

    if ( inner && inner->Size() < a.Height() ||
	 cluster && cluster->Size() < a.Height() )
      {
	cout << "PardisoInverse: Size of inner/cluster does not match matrix size!" << endl;
	throw Exception("Invalid parameters inner/cluster. Thrown by PardisoInverse.");
      }

    clock_t starttime, time1, time2;
    starttime = clock();


    // prepare matrix and parameters for PARDISO
    TM entry = a(0,0);
    if ( NumRows(entry) != NumCols(entry) )
      {
	cout << "PardisoInverse: Each entry in the square matrix has to be a square matrix!" << endl;
	throw Exception("No Square Matrix. Thrown by PardisoInverse.");
      }
    entrysize = NumRows(entry);

    SetMatrixType(entry);


    height = a.Height() * entrysize;
    rowstart = new int[height+1];
    indices = new int[a.NZE() * entrysize * entrysize ];
    matrix = new TSCAL[a.NZE() * entrysize * entrysize ];     
    //#ifdef ASTRID
    spd = 0;
    if ( a.GetInverseType() == PARDISOSPD ) spd = 1;
    //#endif
    int i, j, k, l, rowsize;
    int col;
    int maxfct = 1, mnum = 1, phase = 12, nrhs = 1, msglevel = 0, error;
    // int params[64];
    int * params = const_cast <int*> (&hparams[0]);

    params[0] = 1; // no pardiso defaults
    params[2] = 1; // 1 processor

    params[1] = 2;
    params[3] = params[4] = params[5] = params[7] = params[8] = 
      params[11] = params[12] = params[18] = 0;
    params[6] = 16;
    params[9] = 20;
    params[10] = 1;

    // JS
    params[6] = 0;
    params[17] = -1;
    params[20] = 0;
    
    for (i = 0; i < 64; i++) pt[i] = 0;
 
    int retvalue;

#ifdef USE_MKL
//     retvalue = F77_FUNC(pardiso) ( pt, &maxfct, &mnum, &matrixtype, &phase, &height, 
// 				   reinterpret_cast<double *>(matrix),
// 				   rowstart, indices, NULL, &nrhs, params, &msglevel,
// 				   NULL, NULL, &error );
#else
    //    cout << "call pardiso init" << endl;
    // cout << "mtype = " << matrixtype << endl;

    retvalue = 
      F77_FUNC(pardisoinit) (pt,  &matrixtype, params); 

    // cout << "init success" << endl;
    // cout << "retvalue = " << retvalue << endl;
#endif

    SetMatrixType(entry);

    if ( symmetric )
      {
	// --- transform lower left to upper right triangular matrix ---
	// 1.) build array 'rowstart':
	// (a) get nr. of entries for each row

	for ( i=0; i<=height; i++ ) rowstart[i] = 0;

	for ( i=1; i<=a.Height(); i++ )
	  {
	    for ( j=0; j<a.GetRowIndices(i-1).Size(); j++ )
	      {
		col = a.GetRowIndices(i-1)[j];
		if ( i-1 != col )
		  {
		    if (  (!inner && !cluster) ||
			  (inner && (inner->Test(i-1) && inner->Test(col) ) ) ||
			  (!inner && cluster && 
		           ((*cluster)[i-1] == (*cluster)[col] 
			    && (*cluster)[i-1] ))  )
		      {
			for ( k=0; k<entrysize; k++ )
			  rowstart[col*entrysize+k+1] += entrysize;
		      }
		  }
		else if ( (!inner && !cluster) || 
			  (inner && inner->Test(i-1)) ||
			  (!inner && cluster && (*cluster)[i-1]) )
		  {
		    for ( k=0; k<entrysize; k++ )
		      rowstart[col*entrysize+k+1] += entrysize-k;
		  }
		else
		  {
		    for ( k=0; k<entrysize; k++ )
		      rowstart[col*entrysize+k+1] ++;
		  }
	      }
	  }
	// (b) accumulate
	rowstart[0] = 1;
	for ( i=1; i<=height; i++ ) rowstart[i] += rowstart[i-1];
	
	
	// 2.) build whole matrix:
	int * counter = new int[height];
	for ( i=0; i<a.Height(); i++ )
	  {
	    rowsize = a.GetRowIndices(i).Size();
	    for ( k=0; k<entrysize; k++ ) counter[i*entrysize+k]=0;

	    for ( j=0; j<a.GetRowIndices(i).Size(); j++ )
	      {
		col = a.GetRowIndices(i)[j];

		if ( i != col )
		  {
		    if ( (!inner && !cluster) ||
			 (inner && (inner->Test(i) && inner->Test(col)) ) ||
			 (!inner && cluster && 
			  ((*cluster)[i] == (*cluster)[col] 
			   && (*cluster)[i] ))  )
		      {
			entry = a(i,col);
			for ( k=0; k<entrysize; k++)
			  for ( l=0; l<entrysize; l++ )
			    {
			      indices[ rowstart[col*entrysize+k]+
				       counter[col*entrysize+k]-1 ] = i*entrysize+l+1;
			      matrix[ rowstart[col*entrysize+k]+
				      counter[col*entrysize+k]-1 ] = Elem(entry,l,k);
			      counter[col*entrysize+k]++;
			    }
		      }
		  }
		else if ( (!inner && !cluster) || 
			  (inner && inner->Test(i)) ||
			  (!inner && cluster && (*cluster)[i]) )
		  {
		    entry = a(i,col);
		    for ( l=0; l<entrysize; l++ )
		      for ( k=0; k<=l; k++)
			{
			  indices[ rowstart[col*entrysize+k]+
				   counter[col*entrysize+k]-1 ] = i*entrysize+l+1;
			  matrix[ rowstart[col*entrysize+k]+
				  counter[col*entrysize+k]-1 ] = Elem(entry,l,k);
			  counter[col*entrysize+k]++;
			}
		  }
		else
		  {
		    // in the case of 'inner' or 'cluster': 1 on the diagonal for
		    // unused dofs.
		    for ( l=0; l<entrysize; l++ )
		      {
			indices[ rowstart[col*entrysize+l]+
				 counter[col*entrysize+l]-1 ] = i*entrysize+l+1;
			matrix[ rowstart[col*entrysize+l]+
				counter[col*entrysize+l]-1 ] = 1;
			counter[col*entrysize+l]++;
		      }
		  }
	      }
	  }
	

	/*
	  (*testout) << endl << "row, rowstart / indices, matrix-entries" << endl;
	  for ( i=0; i<height; i++ )
	  {
	  (*testout) << endl << i+1 << ", " << rowstart[i] << ":   ";
	  for ( j=rowstart[i]-1; j<rowstart[i+1]-1; j++ )
	  (*testout) << indices[j] << ", " << matrix[j] << "      ";
	  }
	  (*testout) << endl;
	*/

	delete [] counter;
      }
    else
      {
        cout << "non-symmetric pardiso, cluster = " << cluster << ", entrysize = " << entrysize << endl;
	// for non-symmetric matrices:
	int counter = 0, rowelems;

	for ( i=0; i<a.Height(); i++ )
	  {
	    rowelems = 0;
	    rowsize = a.GetRowIndices(i).Size();

	    // get effective number of elements in the row
            /*
	    if ( inner || cluster )
	      {
		for ( j=0; j<rowsize; j++ )
		  {
		    col = a.GetRowIndices(i)[j];
		    if ( (inner && (inner->Test(i) && inner->Test(col))) ||
			 (!inner && cluster &&
		          ((*cluster)[i] == (*cluster)[col] 
			   && (*cluster)[i])) )
		      rowelems+=entrysize;
		    else if ( i == col )
		      rowelems++;
		  }
	      }
	    else rowelems = rowsize * entrysize;
            */
	    if ( inner )
	      {
		for ( j=0; j<rowsize; j++ )
		  {
		    col = a.GetRowIndices(i)[j];
		    if ( (inner && (inner->Test(i) && inner->Test(col))) ||
			 (!inner && cluster &&
		          ((*cluster)[i] == (*cluster)[col] 
			   && (*cluster)[i])) )
		      rowelems+=entrysize;
		    else if ( i == col )
		      rowelems++;
		  }
	      }
            else if (cluster)
	      {
		for ( j=0; j<rowsize; j++ )
		  {
		    col = a.GetRowIndices(i)[j];
		    if ( (*cluster)[i] == (*cluster)[col] && (*cluster)[i])
		      rowelems+=entrysize;
		    else if ( i == col )
		      rowelems++;
		  }
	      }
	    else rowelems = rowsize * entrysize;



	    for ( k=0; k<entrysize; k++ )
	      rowstart[i*entrysize+k] = counter+rowelems*k+1;
	    
	    //	    (*testout) << "rowelems: " << rowelems << endl;


            /*
	    for ( j=0; j<rowsize; j++ )
	      {
		col = a.GetRowIndices(i)[j];

		if ( (!inner && !cluster) ||
		     (inner && ( inner->Test(i) && inner->Test(col) )) ||
		     (!inner && cluster &&
		      ((*cluster)[i] == (*cluster)[col] 
		       && (*cluster)[i] )) )
		  {
		    entry = a(i,col);
		    for ( k=0; k<entrysize; k++ )
		      for ( l=0; l<entrysize; l++ )
			{
			  indices[counter+rowelems*k+l] = col*entrysize+l+1;
			  matrix[counter+rowelems*k+l] = Elem(entry,k,l);
			}

		    counter+=entrysize;
		  }
		else if ( i == col )
		  {
		    // in the case of 'inner' or 'cluster': 1 on the diagonal for
		    // unused dofs.
		    for ( l=0; l<entrysize; l++ )
		      {
			indices[counter+rowelems*l+l] = col*entrysize+l+1;
			matrix[counter+rowelems*l+l] = 1;
		      }
		    counter++;
		  }
	      }
            */

            if (inner)
              {
                for ( j=0; j<rowsize; j++ )
                  {
                    col = a.GetRowIndices(i)[j];
                    
                    if ( (!inner && !cluster) ||
                         (inner && ( inner->Test(i) && inner->Test(col) )) ||
                         (!inner && cluster &&
		      ((*cluster)[i] == (*cluster)[col] 
		       && (*cluster)[i] )) )
                      {
                        entry = a(i,col);
                        for ( k=0; k<entrysize; k++ )
                          for ( l=0; l<entrysize; l++ )
                            {
                              indices[counter+rowelems*k+l] = col*entrysize+l+1;
                              matrix[counter+rowelems*k+l] = Elem(entry,k,l);
                            }
                        
                        counter+=entrysize;
                      }
                    else if ( i == col )
                      {
		    // in the case of 'inner' or 'cluster': 1 on the diagonal for
		    // unused dofs.
                        for ( l=0; l<entrysize; l++ )
                          {
                            indices[counter+rowelems*l+l] = col*entrysize+l+1;
                            matrix[counter+rowelems*l+l] = 1;
                          }
                        counter++;
                      }
                  }
              }
            else if (cluster)
              {
                for ( j=0; j<rowsize; j++ )
                  {
                    col = a.GetRowIndices(i)[j];
                    
                    if ( (*cluster)[i] == (*cluster)[col] && (*cluster)[i] )
                      {
                        entry = a(i,col);
                        for ( k=0; k<entrysize; k++ )
                          for ( l=0; l<entrysize; l++ )
                            {
                              indices[counter+rowelems*k+l] = col*entrysize+l+1;
                              matrix[counter+rowelems*k+l] = Elem(entry,k,l);
                            }
                        counter+=entrysize;
                      }
                    else if ( i == col )
                      {
                        for ( l=0; l<entrysize; l++ )
                          {
                            indices[counter+rowelems*l+l] = col*entrysize+l+1;
                            matrix[counter+rowelems*l+l] = 1;
                          }
                        counter+=entrysize;
                      }
                  }
              }
            else
              for ( j=0; j<rowsize; j++ )
                {
                  col = a.GetRowIndices(i)[j];
                  
                  entry = a(i,col);
                  for ( k=0; k<entrysize; k++ )
                    for ( l=0; l<entrysize; l++ )
                      {
                        indices[counter+rowelems*k+l] = col*entrysize+l+1;
                        matrix[counter+rowelems*k+l] = Elem(entry,k,l);
                      }
                  
                  counter+=entrysize;
                }


	    counter += rowelems * ( entrysize-1 );
	  }
	rowstart[height] = counter+1;

	
	/*
	  (*testout) << endl << "row, rowstart / indices, matrix-entries" << endl;
	  for ( i=0; i<height; i++ )
	  {
	  (*testout) << endl << i+1 << ", " << rowstart[i] << ":   ";
	  for ( j=rowstart[i]-1; j<rowstart[i+1]-1; j++ )
	  (*testout) << indices[j] << ", " << matrix[j] << "      ";
	  }
	  (*testout) << endl;
	*/

      }
    nze = rowstart[height];

    // call pardiso for factorization:
    time1 = clock();
    cout << "call pardiso ..." << flush;
    F77_FUNC(pardiso) ( pt, &maxfct, &mnum, &matrixtype, &phase, &height, 
			reinterpret_cast<double *>(matrix),
			rowstart, indices, NULL, &nrhs, params, &msglevel,
			NULL, NULL, &error );
    cout << " done" << endl;
    time2 = clock();


    if ( error != 0 )
      {
	cout << "Setup and Factorization: PARDISO returned error " << error << "!" << endl;
	throw Exception("PardisoInverse: Setup and Factorization failed.");
      }

    (*testout) << endl << "Direct Solver: PARDISO by Schenk/Gaertner." << endl;
    (*testout) << "Matrix prepared for PARDISO in " <<
      double(time1 - starttime)/CLOCKS_PER_SEC << " sec." << endl;
    (*testout) << "Factorization by PARDISO done in " << 
      double(time2 - time1)/CLOCKS_PER_SEC << " sec." << endl << endl;

  }
  
  
  template<class TM, class TV_ROW, class TV_COL>
  PardisoInverse<TM,TV_ROW,TV_COL> :: 
  PardisoInverse (const Array<int> & aorder, 
		  const Array<CliqueEl*> & cliques,
		  const Array<MDOVertex> & vertices,
		  int symmetric)
  {
    Allocate (aorder, cliques, vertices);
  }
  

  
  template<class TM, class TV_ROW, class TV_COL>
  void PardisoInverse<TM,TV_ROW,TV_COL> :: 
  Allocate (const Array<int> & aorder, 
	    const Array<CliqueEl*> & cliques,
	    const Array<MDOVertex> & vertices)
  {
    cout << "PardisoInverse::Allocate not implemented!" << endl;
  }
  

  template<class TM, class TV_ROW, class TV_COL>
  void PardisoInverse<TM,TV_ROW,TV_COL> :: 
  FactorNew (const SparseMatrix<TM,TV_ROW,TV_COL> & a)
  {
    throw Exception ("PardisoInverse::FactorNew not implemented");
  }





  template<class TM, class TV_ROW, class TV_COL>
  void PardisoInverse<TM,TV_ROW,TV_COL> :: Factor (const int * blocknr)
  {
    cout << "PardisoInverse::Factor not implemented!" << endl;
  }
  
  


  template<class TM, class TV_ROW, class TV_COL>
  void PardisoInverse<TM,TV_ROW,TV_COL> :: 
  Mult (const BaseVector & x, BaseVector & y) const
  {


    FlatVector<TVX> fx = 
      dynamic_cast<T_BaseVector<TVX> &> (const_cast<BaseVector &> (x)).FV();
    FlatVector<TVX> fy = 
      dynamic_cast<T_BaseVector<TVX> &> (y).FV();
    

    int maxfct = 1, mnum = 1, phase = 33, nrhs = 1, msglevel = 0, error;
    // int params[64];
    int * params = const_cast <int*> (&hparams[0]);

    /*
      params[0] = 1; // no pardiso defaults
      params[2] = 1; // 1 processor

      params[1] = 2;
      params[3] = params[4] = params[5] = params[7] = params[8] = 
      params[11] = params[12] = params[18] = 0;
      params[6] = 16;
      params[9] = 20;
      params[10] = 1;
    */



    params[0] = 1; // no pardiso defaults
    params[2] = 1; // 1 processor

    params[1] = 2;
    params[3] = params[4] = params[5] = params[7] = params[8] = 
      params[11] = params[12] = params[18] = 0;
    params[6] = 16;
    params[9] = 20;
    params[10] = 1;

    // JS
    params[6] = 0;
    params[17] = -1;
    params[20] = 0;










    F77_FUNC(pardiso) ( const_cast<int *>(pt), &maxfct, &mnum, const_cast<int *>(&matrixtype),
			&phase, const_cast<int *>(&height), 
			reinterpret_cast<double *>(matrix),
			rowstart, indices,
			NULL, &nrhs, params, &msglevel,
			static_cast<double *>(fx.Data()), 
			static_cast<double *>(fy.Data()), &error );
    if ( error != 0 )
      cout << "Apply Inverse: PARDISO returned error " << error << "!" << endl;

    if (inner)
      {
	for (int i=0; i<height/entrysize; i++)
	  if (!inner->Test(i)) 
	    for (int j=0; j<entrysize; j++ ) fy(i*entrysize+j) = 0;
      }
    else if (cluster)
      {
	for (int i=0; i<height/entrysize; i++)
	  if (!(*cluster)[i]) 
	    for (int j=0; j<entrysize; j++ ) fy(i*entrysize+j) = 0;
      }


  }
  
  




  template<class TM, class TV_ROW, class TV_COL>
  void PardisoInverse<TM,TV_ROW,TV_COL> :: Set (int i, int j, const TM & val)
  {
    cout << "PardisoInverse::Set not implemented!" << endl;
  }


  template<class TM, class TV_ROW, class TV_COL>
  const TM & PardisoInverse<TM,TV_ROW,TV_COL> :: Get (int i, int j) const
  {
    cout << "PardisoInverse::Get not implemented!" << endl;
  }

  template<class TM, class TV_ROW, class TV_COL>
  ostream & PardisoInverse<TM,TV_ROW,TV_COL> :: Print (ostream & ost) const
  {
    cout << "PardisoInverse::Print not implemented!" << endl;
    return ost; 
  }


  template<class TM, class TV_ROW, class TV_COL>
  PardisoInverse<TM,TV_ROW,TV_COL> :: ~PardisoInverse()
  {
    int maxfct = 1, mnum = 1, phase = -1, nrhs = 1, msglevel = 0, error;
    // int params[64];

    int * params = const_cast <int*> (&hparams[0]);

    //    cout << "call pardiso (clean up) ..." << endl;
    F77_FUNC(pardiso) ( pt, &maxfct, &mnum, &matrixtype, &phase, &height, NULL,
			rowstart, indices, NULL, &nrhs, params, &msglevel,
			NULL, NULL, &error );
    if ( error != 0 )
      cout << "Clean Up: PARDISO returned error " << error << "!" << endl;

    delete [] rowstart;
    delete [] indices;
    delete [] matrix;
  }






  template<class TM, class TV_ROW, class TV_COL>
  void PardisoInverse<TM,TV_ROW,TV_COL> :: SetMatrixType(TM entry)
  {
    if ( IsComplex(entry) )
      {
	if ( symmetric ) matrixtype = 6;
	else
	  {
	    matrixtype = 13;
	    (*testout) << "PARDISO: Assume matrix type to be complex non-symmetric." << endl;
	    (*testout) << "Warning: Works, but is not optimal for Hermitian matrices." << endl;
	  }
      }
    else
      {
	if ( symmetric )
	  {
	    /*
	    matrixtype = 2;    // +2 .. pos def., -2  .. symmetric indef
	    (*testout) << "PARDISO: Assume matrix type to be symmetric positive definite." << endl;
	    (*testout) << "Warning: This might cause errors for symmetric indefinite matrices!!" << endl;
	    */
#ifdef ASTIRD
	    if ( spd ) 
	      matrixtype = 2;
	    else
	      matrixtype = -2;
#else
           matrixtype = -2; 
#endif
	  }
	else matrixtype = 11;
      }
    *testout << "pardiso matrixtype = " << matrixtype << endl;
  }




  template class PardisoInverse<double>;
  template class PardisoInverse<Complex>;
  template class PardisoInverse<double,Complex,Complex>;
#if MAX_SYS_DIM >= 1
  template class PardisoInverse<Mat<1,1,double> >;
  template class PardisoInverse<Mat<1,1,Complex> >;
#endif
#if MAX_SYS_DIM >= 2
  template class PardisoInverse<Mat<2,2,double> >;
  template class PardisoInverse<Mat<2,2,Complex> >;
#endif
#if MAX_SYS_DIM >= 3
  template class PardisoInverse<Mat<3,3,double> >;
  template class PardisoInverse<Mat<3,3,Complex> >;
#endif
#if MAX_SYS_DIM >= 4
  template class PardisoInverse<Mat<4,4,double> >;
  template class PardisoInverse<Mat<4,4,Complex> >;
#endif
#if MAX_SYS_DIM >= 5
  template class PardisoInverse<Mat<5,5,double> >;
  template class PardisoInverse<Mat<5,5,Complex> >;
#endif
#if MAX_SYS_DIM >= 6
  template class PardisoInverse<Mat<6,6,double> >;
  template class PardisoInverse<Mat<6,6,Complex> >;
#endif
#if MAX_SYS_DIM >= 7
  template class PardisoInverse<Mat<7,7,double> >;
  template class PardisoInverse<Mat<7,7,Complex> >;
#endif
#if MAX_SYS_DIM >= 8
  template class PardisoInverse<Mat<8,8,double> >;
  template class PardisoInverse<Mat<8,8,Complex> >;
#endif






  template class PardisoInverse<double, Vec<2,double>, Vec<2,double> >;
#if MAX_CACHEBLOCKS >= 3
  template class PardisoInverse<double, Vec<3,double>, Vec<3,double> >;
  template class PardisoInverse<double, Vec<4,double>, Vec<4,double> >;
#endif
#if MAX_CACHEBLOCKS >= 5
  template class PardisoInverse<double, Vec<5,double>, Vec<5,double> >;
  template class PardisoInverse<double, Vec<6,double>, Vec<6,double> >;
  template class PardisoInverse<double, Vec<7,double>, Vec<7,double> >;
  template class PardisoInverse<double, Vec<8,double>, Vec<8,double> >;
  template class PardisoInverse<double, Vec<9,double>, Vec<9,double> >;
  template class PardisoInverse<double, Vec<10,double>, Vec<10,double> >;
  template class PardisoInverse<double, Vec<11,double>, Vec<11,double> >;
  template class PardisoInverse<double, Vec<12,double>, Vec<12,double> >;
  template class PardisoInverse<double, Vec<13,double>, Vec<13,double> >;
  template class PardisoInverse<double, Vec<14,double>, Vec<14,double> >;
  template class PardisoInverse<double, Vec<15,double>, Vec<15,double> >;
#endif

  template class PardisoInverse<double, Vec<2,Complex>, Vec<2,Complex> >;
#if MAX_CACHEBLOCKS >= 3
  template class PardisoInverse<double, Vec<3,Complex>, Vec<3,Complex> >;
  template class PardisoInverse<double, Vec<4,Complex>, Vec<4,Complex> >;
#endif
#if MAX_CACHEBLOCKS >= 5
  template class PardisoInverse<double, Vec<5,Complex>, Vec<5,Complex> >;
  template class PardisoInverse<double, Vec<6,Complex>, Vec<6,Complex> >;
  template class PardisoInverse<double, Vec<7,Complex>, Vec<7,Complex> >;
  template class PardisoInverse<double, Vec<8,Complex>, Vec<8,Complex> >;
  template class PardisoInverse<double, Vec<9,Complex>, Vec<9,Complex> >;
  template class PardisoInverse<double, Vec<10,Complex>, Vec<10,Complex> >;
  template class PardisoInverse<double, Vec<11,Complex>, Vec<11,Complex> >;
  template class PardisoInverse<double, Vec<12,Complex>, Vec<12,Complex> >;
  template class PardisoInverse<double, Vec<13,Complex>, Vec<13,Complex> >;
  template class PardisoInverse<double, Vec<14,Complex>, Vec<14,Complex> >;
  template class PardisoInverse<double, Vec<15,Complex>, Vec<15,Complex> >;
#endif

  template class PardisoInverse<Complex, Vec<2,Complex>, Vec<2,Complex> >;
#if MAX_CACHEBLOCKS >= 3
  template class PardisoInverse<Complex, Vec<3,Complex>, Vec<3,Complex> >;
  template class PardisoInverse<Complex, Vec<4,Complex>, Vec<4,Complex> >;
#endif
#if MAX_CACHEBLOCKS >= 5
  template class PardisoInverse<Complex, Vec<5,Complex>, Vec<5,Complex> >;
  template class PardisoInverse<Complex, Vec<6,Complex>, Vec<6,Complex> >;
  template class PardisoInverse<Complex, Vec<7,Complex>, Vec<7,Complex> >;
  template class PardisoInverse<Complex, Vec<8,Complex>, Vec<8,Complex> >;
  template class PardisoInverse<Complex, Vec<9,Complex>, Vec<9,Complex> >;
  template class PardisoInverse<Complex, Vec<10,Complex>, Vec<10,Complex> >;
  template class PardisoInverse<Complex, Vec<11,Complex>, Vec<11,Complex> >;
  template class PardisoInverse<Complex, Vec<12,Complex>, Vec<12,Complex> >;
  template class PardisoInverse<Complex, Vec<13,Complex>, Vec<13,Complex> >;
  template class PardisoInverse<Complex, Vec<14,Complex>, Vec<14,Complex> >;
  template class PardisoInverse<Complex, Vec<15,Complex>, Vec<15,Complex> >;
#endif



}









#endif

