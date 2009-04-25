
#ifdef LAPACK
#include <iostream>
#include <complex>
 
using namespace std;


#include "LapackInterface.hpp" 

/*
#include <mkl_lapack.h>
#define dggev_ dggev
#define dsyev_ dsyev
#define dggbak_ dggbak
#define dggbal_ dggbal
#define zggev_ zggev
#define zggbal_ zggbal
#define zggev_ zggev
*/

// #include "LapackGEP.hpp" 

namespace ngbla
{

void LaEigNSSolveTest()
{   

  double *  A= new double[16];
  double *  B= new double[16]; 
 
    
  int i;   
  for(i=0;i<16;i++) {A[i]=0.;} 
  
  A[0]=1.; 
  A[4]=2.; 
  A[1]=2.;
  A[5]=5.; 
  A[15]=1.; 
  A[10]=0.2; 
    
  for(i=0;i<16;i++) B[i] = A[i]; 
  
  char jobvr = 'V', jobvl= 'N';
  int  n = 4;
  
  int info;
   
  double * wr = new double[n]; 

  double* work = new double[16*n]; 
  int lwork = 16*n; 

  char uplo = 'U' ; 

  dsyev_(&jobvr,&uplo , &n , A , &n, wr, work, &lwork, &info); 

    //  dggev_(&jobvl, &jobvr, &n, A, &n, B, &n, wr, wi, beta, &vl, &nvl, &vr, &nvl, 
    // work , &lwork, &info); 



  int j,k; 
  double *ev = new double[4];
  double *v2 = new double[4]; 
  
  /*cout << " Matrix B (original) " << endl; 
  for(i=0;i<n;i++) 
    {
      for(j=0;j<n;j++) 
	  cout << B[i*n+j] << "\t" ; 
	  
      cout << endl; 
    }

  cout << " Matrix A (original) " << endl; 
  for(i=0;i<n;i++) 
    {
      for(j=0;j<n;j++) 
	  cout << A[i*n+j] << "\t" ; 
	  
      cout << endl; 
    }

  for(i=0;i<n;i++)
    {
      for(j=0;j<n;j++) { ev[j] = A[i*n+j]; v2[j] = 0.; }
      cout << "lami" <<  (wr[i]) << endl;
      cout << " Residuum " << endl;  
      for(j=0;j<n;j++)
	{
	  for(k=0;k<n;k++)
	    {
	      v2[j] += B[j*n+k]*ev[k];
	    }
	  cout <<  v2[j] - wr[i]*ev[j] << "\t" ; 
	}
      cout << endl; 
    } 
  */    
  delete[] A; 
  delete[] B; 
  delete[] wr; 
  delete[] work; 
  delete[] ev; 
  delete[] v2; 
}
       
 


//Attention: A,B are overwritten !! 
void LaEigNSSolve(int n, double * A, double * B, std::complex<double> * lami, int evecs_bool, double * evecs_re, double * evecs_im, char balance_type )
{   
  char jobvr , jobvl= 'N';
  bool balancing = 0; 

  if ( balance_type == 'B' || balance_type == 'S' || balance_type == 'P' )
    balancing =1; 

  
  int info;
   
  double * wi= new double[n], * wr = new double[n]; 
  double * beta = new double[n]; 
  double vl=0; 
   
  int nvl = 1; 
  int nvr ; 

  if (evecs_bool)
    {
      jobvr = 'V'; 
      nvr = n; 
    }
  else 
    { 
      nvr=1; 
      jobvr = 'N';
    }

  double *vr = new double[nvr*nvr]; 
  
  double* work = new double[16*n]; 
  int lwork = 16*n; 
  int i,j,k; 
   
  char job=balance_type; // Permute and Scale in Balancing 
  int ihi,ilo; 
  double * lscale, *rscale; 
  lscale = new double[n]; 
  rscale = new double[n]; 

  char side = 'R'; 
  

  if(balancing) 
    dggbal_(&job,&n, A,&n , B, &n, &ilo, &ihi,  lscale, rscale, work, &info) ; 
  else info =0; 

  if(info ==0 ) 
    { 
      dggev_(&jobvl, &jobvr, &n, A, &n, B, &n, wr, wi, beta, &vl, &nvl, vr, &nvr, 
	     work , &lwork, &info);
      
      if(info==0) 	
	{ 
	  if(jobvr == 'V' && balancing) 
	    {
	      dggbak_(&job, &side, &n, &ilo, &ihi, lscale, rscale, &n, vr, &n,&info)  ; 
	      if(info!=0)
		{
		  cout << " Error in dggbak_ :: info  " << info << endl; 
		  return;
		}
	    }
	}
      else 
	{
	  cout << " Error in dggev_ :: info  " << info << endl; 
	  return;
	}  
    }
  else 
    {
      cout << " Error in dggbal_ :: info " << info << endl; 
      return; 
    }

  delete[] lscale; 
  delete[] rscale;  
  
  for(i=0;i<n;i++)
    {
      if (fabs(beta[i])>1e-30)  // not infinite eigenvalue 
	lami[i]=std::complex<double>(wr[i]/beta[i],wi[i]/beta[i]);
      else 
	{
	  lami[i]=std::complex<double>(100.,100.); 
	}
    }
  
  if(evecs_bool)
    {
      
      for(i=0;i<n;i++)
	{
	 
	  if( imag(lami[i])== 0. || beta[i] == 0.) //real eigenvalue -> real eigenvector in i-th line
	    { 
	      for(j=0;j<n;j++) 
	      {
		// evecs[i*n+j]= std::complex<double>(vr[i*n + j],0.);
		evecs_re[i*n+j]= vr[i*n+j];
		evecs_im[i*n+j]= 0.; 
	      }
	    } 
	  else // conjugate complex eigenvectors 
	    {
	      for(j=0;j<n;j++)
		{
		  // evecs[i*n+j]= std::complex<double>(vr[i*n+j],vr[(i+1)*n+j]);
		  // evecs[(i+1)*n+j]=std::complex<double>(vr[i*n+j],-vr[(i+1)*n+j]);
		  evecs_re[i*n+j]= vr[i*n+j];
		  evecs_re[(i+1)*n+j]=vr[i*n+j];
		  evecs_im[i*n+j]= vr[(i+1)*n+j];
		  evecs_im[(i+1)*n+j]=-vr[(i+1)*n+j];
		}
	      i++; 
	    } 
	}
   }
   
 
  delete[] wi;
  delete[] wr; 
  delete[] beta; 
  delete[] work; 
  delete[] vr;


 
}

// Attention A,B are overwritten !!! 
void LaEigNSSolve(int n, std::complex<double> * A, std::complex<double> * B, std::complex<double> * lami, int evecs_bool, std::complex<double> * evecs, std::complex<double> * dummy, char balance_type)
{
  
  balance_type = 'N'; 
  
  
  std::complex<double> * at = new std::complex<double> [n*n];
  std::complex<double> * bt = new std::complex<double> [n*n];
  
  for(int i=0;i<n;i++)
    for(int   j=0;j<n;j++)
        at[j*n+i] = A[i*n+j];
      
  for(int i=0;i<n;i++)
    for(int   j=0;j<n;j++)
      bt[j*n+i] = B[i*n+j];
  
  
  char jobvr , jobvl= 'N';
  bool balancing = 0; 
  
  balance_type = 'P'; 
  if ( balance_type == 'B' || balance_type == 'S' || balance_type == 'P' )
    balancing =1; 
  
   balancing = 0; 
  
  std::complex<double> * alpha= new std::complex<double>[n];
  std::complex<double> * beta = new std::complex<double>[n]; 
  std::complex<double> vl=0.; 
  
  int nvl = 1; 
  std::complex<double> * vr ;
  
  std::complex<double> * work = new std::complex<double>[8*n]; 
  int lwork = 8*n; 
  double *rwork = new double[8*n];  
  
  int nvr = n ; 
  if (evecs_bool) 
    {
      jobvr = 'V'; 
      vr = evecs; 
    }
  else jobvr = 'N'; 
  
  //std::complex<double>  * A1,*B1; 
  int i; 
  
  char job=balance_type; // Permute and Scale in Balancing 
  int ihi,ilo; 
   
  double * lscale = new double[n]; 
  double * rscale = new double[n]; 
   
  double * work2 = new double[6*n];
  
  char side = 'R'; 
  
  int info = 0;

  int ii; 
  ii=0; 
   
  if(balancing) zggbal_(&job,&n, at, &n , bt, &n, &ilo, &ihi,  lscale, rscale, work2, &info) ; 
  
  
  
  if(info == 0 ) 
    {  
      zggev_(&jobvl, &jobvr, &n, at, &n, bt, &n, alpha, beta, &vl, &nvl, vr, &nvr,  
	     work , &lwork, rwork,  &info);
      
      if(info==0) 	
        // if(info>=0) 	
	{
	  if(jobvr == 'V' && balancing) 
	    {
	      zggbak_(&job, &side, &n, &ilo, &ihi, lscale, rscale, &n, vr, &n,&info)  ;
            
	      if(info!=0)
		{ 
		  cout << "*****  Error in zggbak_ **** " << endl; 
		  return; 
		}
	    }
	} 
      else 
	{
	  cout << "**** Error in zggev_, info = " << info << " *****" << endl; 
	  return;
	}
    }	
  else 
    {
      cout << "**** Error in zggbal_ **** " << endl; 
      return;
    }
    
  delete[] work; 
  delete[] rwork;
  
  delete[] lscale; 
  delete[] rscale; 
  delete[] work2;   
 
  
 
  for(i=0;i<n;i++)
    {
      if(abs(beta[i]) >= 1.e-30) 
	lami[i]=std::complex<double>(alpha[i]/beta[i]);     
      else 
	{
	  lami[i] = std::complex<double>(100.,100.);
	}
    } 
  
  /*  
    std:: complex<double> resid[n]; 
    double error;  
    for(int k=0; k<n;k++)
      {
	for(i=0; i<n;i++) 
	  { 
	    resid[i] = 0.; 
	    for(int j=0;j<n;j++)
	      {
		resid[i] += A[j*n + i]*evecs[k*n+j];
		resid[i] -=B[j*n+i]*evecs[k*n+j]*lami[k];
	      }
    
	    error = abs(resid[i]) * abs(resid[i]) ;
	  } 
	error = sqrt(error); 
	cout << " lami (i) " << lami[k] <<  "\t" <<  alpha[k] << "\t"  << beta[k]  << endl; 
	cout << " error lapack " << k << "  \t " << error << endl; 
      }
  */
  delete[] alpha; 
  delete[] beta; 
  delete[] at;
  delete[] bt; 
 
}   


// Attention A,B are overwritten !!! 
void LaEigNSSolveX(int n, std::complex<double> * A, std::complex<double> * B, std::complex<double> * lami, int evecs_bool, std::complex<double> * evecs, std::complex<double> * dummy, char balance_type)
{
  
  std::complex<double> * at = new std::complex<double> [n*n];
  std::complex<double> * bt = new std::complex<double> [n*n];
  
  for(int i=0;i<n;i++)
    for(int   j=0;j<n;j++)
      at[j*n+i] = A[i*n+j];
      
  for(int i=0;i<n;i++)
    for(int   j=0;j<n;j++)
      bt[j*n+i] = B[i*n+j];
  
  
  
  
  
  
  char jobvr , jobvl= 'N';
 
  if(balance_type != 'B' && balance_type != 'S' && balance_type != 'P') 
    balance_type = 'N'; 
  
  std::complex<double> * alpha= new std::complex<double>[n];
  std::complex<double> * beta = new std::complex<double>[n]; 
  std::complex<double> vl=0.; 
  
  int nvl = 1; 
  std::complex<double> * vr ;
  
  std::complex<double> * work = new std::complex<double>[20*n]; 
  int lwork = 20*n; 
  double *rwork = new double[20*n];  
  
  int nvr = n ; 
  if (evecs_bool) 
  {
    jobvr = 'V'; 
    vr = evecs; 
  }
  else jobvr = 'N'; 
  
  //std::complex<double>  * A1,*B1; 
  int i; 
  
  char job=balance_type; // Permute and Scale in Balancing 
  int ihi,ilo; 
   
  double * lscale = new double[4*n]; 
  double * rscale = new double[4*n]; 
   
  
  int * iwork = new int[n+2];
  
  
  char side = 'R'; 
  char sense = 'N'; // no recoprocal condition number computed !! 
  double rconde, rcondv; // not referenced if sense == N 
  bool bwork; // not referenced 
  
  int info = 0;

  int ii; 
  ii=0; 
   

    double abnrm, bbnrm; // 1-norm of A and B 
    
    zggevx_(&balance_type,&jobvl, &jobvr, &sense, &n, at, &n, bt, &n, alpha, beta, &vl, &nvl, vr, &nvr,  
            &ilo, &ihi, lscale, rscale, &abnrm, &bbnrm, &rconde, &rcondv, work , &lwork, rwork,  iwork, &bwork, &info);
    
    if(info!=0)       
        // if(info>=0)  
    {
          cout << " ****** INFO " << info << endl;  
          cout << "*****  Error in zggevx_ **** " << endl; 
          //throw (); 
          return; 
        }
    
      for(int i=0;i<n;i++) 
        cout << " i " << i << " alpha " << alpha[i] << " beta " << beta[i] << endl; 
        
        
        
  delete[] iwork;      
  delete[] work; 
  delete[] rwork;
  
  delete[] lscale; 
  delete[] rscale;    
 
  
 
  for(i=0;i<n;i++)
  {
    if(abs(beta[i]) >= 1.e-30) 
      lami[i]=std::complex<double>(alpha[i]/beta[i]);     
    else 
    {
      lami[i] = std::complex<double>(100.,100.);
    }
  } 
  
  /*  
  std:: complex<double> resid[n]; 
  double error;  
  for(int k=0; k<n;k++)
  {
  for(i=0; i<n;i++) 
  { 
  resid[i] = 0.; 
  for(int j=0;j<n;j++)
  {
  resid[i] += A[j*n + i]*evecs[k*n+j];
  resid[i] -=B[j*n+i]*evecs[k*n+j]*lami[k];
}
    
  error = abs(resid[i]) * abs(resid[i]) ;
} 
  error = sqrt(error); 
  cout << " lami (i) " << lami[k] <<  "\t" <<  alpha[k] << "\t"  << beta[k]  << endl; 
  cout << " error lapack " << k << "  \t " << error << endl; 
}
  */
  delete[] alpha; 
  delete[] beta; 
  delete[] at;
  delete[] bt; 
 
}   

void LaEigNSSolveX(int n, double * A, double * B, std::complex<double> * lami, int evecs_bool, double * evecs, double * dummy, char balance_type)
{
  cout << "LaEigNSSolveX not implemented for double" << endl;
 // throw();  
}

void LapackSSEP(int n, double* A, double* lami, double* evecs)  
{
  int i,j; 

  for(i=0;i<n*n;i++) evecs[i] = A[i]; 
    
  char jobzm = 'V' , uplo = 'U'; 
  
  int lwork=2*n*n; 
 
  double* work = new double[lwork];
  
  int info; 
 
  dsyev_(&jobzm,&uplo , &n , evecs , &n, lami, work, &lwork, &info); 

  delete[] work; 
}


extern "C"
void zhseqr_(const char & job, const char & compz, const int & n, 
	     const int & ilo, const int & ihi, complex<double> & h, const int & ldh, 
	     complex<double> & w, complex<double> & z, const int & ldz, 
	     complex<double> & work, const int & lwork, int & info);

extern "C"
void zhsein_ (const char & side, const char & eigsrc, const char & initv,
	      int * select, const int & n, 
	      complex<double> & h, const int & ldh, complex<double> & w,
	      complex<double> & vl, const int & ldvl,
	      complex<double> & vr, const int & ldvr,
	      const int & mm, int & m, 
	      complex<double> & work, double & rwork,
	      int & ifaill, int & ifailr, int & info);

void LapackHessenbergEP (int n, std::complex<double> * A, std::complex<double> * lami, std::complex<double> * evecs)
{
  int lwork = 2 * n * n;  // or n ?
  complex<double> * work = new complex<double>[lwork];
  //  complex<double> * work2 = new complex<double>[lwork];
  double * rwork = new double[n];

  complex<double> * hA = new complex<double>[n*n];
  for (int i = 0; i < n*n; i++)  { hA[i] = A[i]; }

  int * select = new int[n];
  for (int i = 0; i < n; i++) select[i] = 'V';

  complex<double> vl;
  //  complex<double> * vl = new complex<double>[n*n];
  //  complex<double> * vr = new complex<double>[n*n];

  int info;
  int * ifaill = new int[n];
  int * ifailr = new int[n];

  cout << "calls zhseqr" << endl;
  zhseqr_('E', 'N', n, 1, n, *hA, n, *lami, *evecs, n, *work, lwork, info);
  //  zhseqr_('S', 'I', n, 1, n, *A, n, *lami, *evecs, n, *work, lwork, info);
  if (info)
    cout << "error in eigensolver, info = " << info << endl;

  for (int i = 0; i < n; i++)
    cout << "ev(" << i << ") = " << lami[i] << endl;

  for (int i = 0; i < n*n; i++)  { hA[i] = A[i]; }

  int m = 0;
  char side = 'R', eigsrc = 'N', initv = 'N';
  int hn = n, ldh = n, ldvl = n, ldvr = n, mm = n;
  cout << "call zhsein" << endl;
  zhsein_ ('R', 'Q', 'N', select, hn, *A, ldh, *lami, vl, ldvl, *evecs, ldvr,
	   mm, m, *work, *rwork, *ifaill, *ifailr, info);
  cout << "m = " << m << endl;
  cout << "rwork[0] = " << rwork[0] << endl;
  //  for (int i = 0; i < n; i++)
  //    cout << "ifail[" << i << "] = " << ifaill[i] << endl;
  // cout << "ifaill = " << ifaill << endl;
  //   cout << "ifailr = " << ifailr << endl;
  cout << "info = " << info << endl;
  
  delete[] select;
  delete[] hA;
  delete[] rwork;
  delete[] work;
  cout << "hessenberg complete" << endl;
}



void LapackGHEP(int n, double* A, double* B,  double* lami)  
{
  int i,j; 

    
  double *B1 = new double[n*n]; 
  double *A1 = new double[n*n]; 
  
  for(i=0;i<n*n;i++)
    { 
      A1[i] = A[i];
      B1[i] = B[i]; 
    } 
 
  char jobzm = 'V' , uplo = 'U'; 
  
  int lwork=16*n; 
 
  double* work = new double[lwork];
  
  int info; 
  int itype =1; 


  dsygv_(&itype,&jobzm,&uplo , &n , A1 , &n, B1, &n, lami, work, &lwork, &info); 
  

  delete[] A1; 
  delete[] B1; 
  delete[] work; 
}
       
int LapackGHEPEPairs(int n, double* A, double* B,  double* lami)  
{
  int i,j; 


  char jobzm = 'V' , uplo = 'U'; 
  
  int lwork=4*n; 
 
  double* work = new double[lwork];
  
  int info; 
  int itype =1; 
  int lda = n;
  int ldb = n;

  dsygv_(&itype,&jobzm,&uplo , &n , A , &lda, B, &ldb, lami, work, &lwork, &info); 

  if(info != 0) 
    {
      cout << "LapackGHEPEPairs Info " << info << endl;  
      cout << "n = " << n << endl; 
    }
  
  
  delete [] work;  
  return(info); 
}

int LapackGHEPEPairs(int n, complex<double>* A, complex<double>* B,  double* lami)  
{
  int i,j; 

  char jobzm = 'V' , uplo = 'U'; 
  
  int lwork=8*n; 

  std::complex<double>* work = new std::complex<double>[lwork];
  double* rwork = new double[lwork]; 
  
  int info; 
  int itype =1; 
  int lda = n;
  int ldb = n;

	cout << " zhegv " << endl; 

   cout << " A s " << endl; 
  for(int j=0;j<n;j++)
	{
	for(int k=0;k<n;k++)	
	  cout <<  A[j*n+k] << " \t " ; 
	cout << endl; 
	}
 cout << " M " << endl; 
  for(int j=0;j<n;j++)
	{
	for(int k=0;k<n;k++)	
	  cout <<  B[j*n+k] << " \t " ; 
	cout << endl; 
	}


  zhegv_(&itype,&jobzm,&uplo , &n , A , &lda, B, &ldb, lami, work, &lwork, rwork, &info); 

cout << " ... is back " << endl; 
  if(info != 0) 
    {
      cout << "LapackGHEPEPairs Info " << info << endl;  
      cout << "n = " << n << endl; 
      
      
    }
  
  delete [] work; 
  delete [] rwork;
  
  return(info); 
}
       



void LaLinearSolveComplex(int n, std::complex<double> * A, std::complex<double> * F)
{
  // Solve Ax=F
  // A on exit LU factorization 
  // F is overwritten by solution x 


  int nrhs =1; 
  int *ipiv; 
  ipiv = new int[n]; 
  int info; 

  
  zgesv_(&n, &nrhs, A, &n, ipiv, F, &n, &info ); 

  if(info!=0) 
    cout << " ***** Error in LapackGEP.cpp LaLinearSolveComplex : info =  " <<  info << endl; 
  delete[] ipiv; 

  return; 
  } 


void LaLinearSolve(int n, double * A, double * F)
{
  // Invert
  // A on exit LU factorization 
  // F is overwritten by solution x 


  int nrhs = n; 
  int *ipiv; 
  ipiv = new int[n*n]; 
  int info; 

  
  dgesv_(&n, &nrhs, A, &n, ipiv, F, &n, &info ); 

  if(info!=0) 
    cout << " ***** Error in LapackGEP.cpp LaLinearSolveComplex : info =  " <<  info << endl; 
  delete[] ipiv; 

  return; 
  } 


void LaLinearSolveRHS(int n, double * A, double * F)
{
  // Solve linear system A x = F for 1 rhs 
  // A on exit LU factorization 
  // F is overwritten by solution x 


  int nrhs = 1; 
  int *ipiv; 
  ipiv = new int[n]; 
  int info; 

 

  dgesv_(&n, &nrhs, A, &n, ipiv, F, &n, &info ); 




  if(info!=0) 
    cout << " ***** Error in LapackGEP.cpp LaLinearSolveComplex : info =  " <<  info << endl; 
  delete[] ipiv; 

  return; 
  } 
}



#endif
