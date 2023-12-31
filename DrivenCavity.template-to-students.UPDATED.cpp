/************************************************************************ */
/*      This code solves for the viscous flow in a lid-driven cavity      */
/**************************************************************************/

#include <iostream> 
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace std;

/************* Following are fixed parameters for array sizes **************/
#define imax 251     /* Number of points in the x-direction (use odd numbers only) */
#define jmax 251     /* Number of points in the y-direction (use odd numbers only) */
#define neq 3       /* Number of equation to be solved ( = 3: mass, x-mtm, y-mtm) */

/**********************************************/
/****** All Global variables declared here. ***/
/***** These variables SHOULD not be changed **/
/********** by the program once set. **********/
/**********************************************/
/***** The variables declared "const" CAN *****/
/*** not be changed by the program once set ***/
/**********************************************/

/*--------- Numerical Constants --------*/
  const double zero   = 0.0;
  const double tenth  = 0.1;
  const double sixth  = 1.0/6.0;
  const double fifth  = 0.2;
  const double fourth = 0.25;
  const double third  = 1.0/3.0;
  const double half   = 0.5;
  const double one    = 1.0;
  const double two    = 2.0;
  const double three  = 3.0;
  const double four   = 4.0;
  const double six    = 6.0;
  
/*--------- User sets inputs here  --------*/

  const int nmax = 1000000000;             /* Maximum number of iterations */
  const int iterout = 500;             /* Number of time steps between solution output */
  const int imms = 0;                   /* Manufactured solution flag: = 1 for manuf. sol., = 0 otherwise */
  const int isgs = 1;                   /* Symmetric Gauss-Seidel  flag: = 1 for SGS, = 0 for point Jacobi */
  const int irstr = 0;                  /* Restart flag: = 1 for restart (file 'restart.in', = 0 for initial run */
  const int ipgorder = 0;               /* Order of pressure gradient: 0 = 2nd, 1 = 3rd (not needed) */
  const int lim = 0;                    /* variable to be used as the limiter sensor (= 0 for pressure) */
  const int residualOut = 10;           /* Number of timesteps between residual output */

  const double cfl  = 0.8;              /* CFL number used to determine time step */
  const double Cx = 0.01;               /* Parameter for 4th order artificial viscosity in x */
  const double Cy = 0.01;               /* Parameter for 4th order artificial viscosity in y */
  const double toler = 1.e-10;          /* Tolerance for iterative residual convergence */
  const double rkappa = 0.1;            /* Time derivative preconditioning constant */
  const double Re = 10.0;              /* Reynolds number = rho*Uinf*L/rmu */
  const double pinf = 0.801333844662;   /* Initial pressure (N/m^2) -> from MMS value at cavity center */
  const double uinf = 1.0;              /* Lid velocity (m/s) */
  const double rho = 1.0;               /* Density (kg/m^3) */
  const double xmin = 0.0;              /* Cavity dimensions...: minimum x location (m) */
  const double xmax = 0.05;             /* maximum x location (m) */
  const double ymin = 0.0;              /* maximum y location (m) */
  const double ymax = 0.05;             /*  maximum y location (m) */
  const double Cx2 = 0.0;               /* Coefficient for 2nd order damping (not required) */
  const double Cy2 = 0.0;               /* Coefficient for 2nd order damping (not required) */
  const double fsmall = 1.e-20;         /* small parameter */

/*-- Derived input quantities (set by function 'set_derived_inputs' called from main)----*/
 
  double rhoinv;    /* Inverse density, 1/rho (m^3/kg) */
  double rlength;   /* Characteristic length (m) [cavity width] */
  double rmu;       /* Viscosity (N*s/m^2) */
  double vel2ref;   /* Reference velocity squared (m^2/s^2) */
  double dx;        /* Delta x (m) */
  double dy;        /* Delta y (m) */
  double rpi;       /* Pi = 3.14159... (defined below) */

/*-- Constants for manufactured solutions ----*/
  const double phi0[neq] = {0.25, 0.3, 0.2};            /* MMS constant */
  const double phix[neq] = {0.5, 0.15, 1.0/6.0};        /* MMS amplitude constant */
  const double phiy[neq] = {0.4, 0.2, 0.25};            /* MMS amplitude constant */
  const double phixy[neq] = {1.0/3.0, 0.25, 0.1};       /* MMS amplitude constant */
  const double apx[neq] = {0.5, 1.0/3.0, 7.0/17.0};     /* MMS frequency constant */
  const double apy[neq] = {0.2, 0.25, 1.0/6.0};         /* MMS frequency constant */
  const double apxy[neq] = {2.0/7.0, 0.4, 1.0/3.0};     /* MMS frequency constant */
  const double fsinx[neq] = {0.0, 1.0, 0.0};            /* MMS constant to determine sine vs. cosine */
  const double fsiny[neq] = {1.0, 0.0, 0.0};            /* MMS constant to determine sine vs. cosine */
  const double fsinxy[neq] = {1.0, 1.0, 0.0};           /* MMS constant to determine sine vs. cosine */
                                                        /* Note: fsin = 1 means the sine function */
                                                        /* Note: fsin = 0 means the cosine function */
                                                        /* Note: arrays here refer to the 3 variables */ 


/*****************************************************************************
*                              Array3 Class
*
*****************************************************************************/

class Array3
{
    private:
        int idim, jdim, kdim;
        double *data;

    public:
    
        Array3(int, int, int);
        ~Array3();

        void copyData(Array3&);
        void swapData(Array3&);     
    
        double& operator() (int, int, int);
        double operator() (int, int, int) const;
};

Array3::Array3 (int i, int j, int k)
{
    idim = i;
    jdim = j;
    kdim = k;
    data = new double[i*j*k];
}

Array3::~Array3 ()
{
    delete [] data;
}

//Copies data from (Array3& A) into the calling Array3 class.   Both Array3's now contain identical data arrays
void Array3::copyData (Array3& A) 
{
    memcpy( data, A.data, idim*jdim*kdim*sizeof(double) );
}


//Swaps pointers to data--thus U.swapData(U2) exchanges data arrays between U and U2
void Array3::swapData (Array3& A)                  
{
    double *temp;

    temp = data;
    data = A.data;
    A.data = temp;
}

inline
double& Array3::operator() (int i, int j, int k)
{
    return data[i*jdim*kdim + j*kdim + k];
    //return data[k*idim*jdim + i*jdim + j];
}

inline      
double Array3::operator() (int i, int j, int k) const
{
    return data[i*jdim*kdim + j*kdim + k];
    //return data[k*idim*jdim + i*jdim + j];
}

/*****************************************************************************
*                              End Array3 Class
*****************************************************************************/



/*****************************************************************************
*                              Array2 Class
*
*****************************************************************************/

class Array2
{
    private:
        int idim, jdim;
        double *data;

    public:
    
        Array2(int, int);
        ~Array2();

        void copyData(Array2&);
        void swapData(Array2&);     
    
        double& operator() (int, int);
        double operator() (int, int) const;
};

Array2::Array2 (int i, int j)
{
    idim = i;
    jdim = j;
    data = new double[i*j];
}

Array2::~Array2 ()
{
    delete [] data;
}

void Array2::copyData (Array2& A)                   //Copies data from (Array2& A) into the calling Array2 class.   
{                                                   //    Both Array2's now contain identical data arrays
    memcpy( data, A.data, idim*jdim*sizeof(double) );
}

void Array2::swapData (Array2& A)                   //Swaps pointers to data--
{                                                   //   thus U.swapData(U2) exchanges data arrays between U and U2
    double *temp;

    temp = data;
    data = A.data;
    A.data = temp;
}

inline
double& Array2::operator() (int i, int j)
{
    return data[i*jdim + j];
}

inline      
double Array2::operator() (int i, int j) const
{
    return data[i*jdim + j];
}

/*****************************************************************************
*                              End Array2 Class
*****************************************************************************/



/*****************Function Pointer Typedefs *********************************/

typedef void (*boundaryConditionPointer)( Array3& );

typedef void (*iterationStepPointer)( boundaryConditionPointer, Array3&, Array3&, Array3&, Array2&, Array2&, Array2& );

/**********************Function Prototypes**********************************/

void set_derived_inputs();
void GS_iteration( boundaryConditionPointer, Array3&, Array3&, Array3&, Array2&, Array2&, Array2& );
void PJ_iteration( boundaryConditionPointer, Array3&, Array3&, Array3&, Array2&, Array2&, Array2& );
void output_file_headers();
void initial( int&, double&, double [neq], Array3&, Array3& );
void bndry( Array3& );
void bndrymms( Array3& );
void write_output( int, Array3&, Array2&, double [neq], double );
double umms( double, double, int ); 
void compute_source_terms( Array3& ); 
double srcmms_mass( double, double );
double srcmms_xmtm( double, double );
double srcmms_ymtm( double, double );
void compute_time_step( Array3&, Array2&, double& );
void Compute_Artificial_Viscosity( Array3&, Array2&, Array2& );
void SGS_forward_sweep( Array3&, Array2&, Array2&, Array2&, Array3& );
void SGS_backward_sweep( Array3&, Array2&, Array2&, Array2&, Array3& );
void point_Jacobi( Array3&, Array3&, Array2&, Array2&, Array2&, Array3& );
void pressure_rescaling( Array3& );
void check_iterative_convergence( int, Array3&, Array3&, Array2&, double [neq], double [neq], int, double, double, double& );
void Discretization_Error_Norms( Array3& );
 

/****************** Inline Function Declarations ***************************/


inline double pow2(double x)                      /* Returns x^2 ... Duplicates pow(x,2)*/
{
    double x2 = x*x;
    return x2;
}

inline double pow3(double x)                      /* Returns x^3 ... Duplicates pow(x,3)*/
{
    double x3 = x*x*x;
    return x3;
}

inline double pow4(double x)                      /* Returns x^4 ... Duplicates pow(x,4)*/
{
    double x4 = x*x*x*x;
    return x4;
}


/******************* End Inline Function Declarations ************************/


/*--- Variables for file handling ---*/
/*--- All files are globally accessible ---*/
  
  FILE *fp1; /* For output of iterative residual history */
  FILE *fp2; /* For output of field data (solution) */
  FILE *fp3; /* For writing the restart file */
  FILE *fp4; /* For reading the restart file */  
  FILE *fp5; /* For output of final DE norms (only for MMS)*/  
//$$$$$$   FILE *fp6; /* For debug: Uncomment for debugging. */  

/***********************************************************************************************************/
/*      NOTE: The Main routine for this C++ code is found at the end                                       */
/***********************************************************************************************************/


/*************************************************************************************************************/
/*                                                                                                           */
/*                                                Functions                                                  */
/*                                                                                                           */
/**********************************************************************************************************  */


void set_derived_inputs()
{
    rhoinv = one/rho;                            /* Inverse density, 1/rho (m^3/kg) */
    rlength = xmax - xmin;                       /* Characteristic length (m) [cavity width] */
    rmu = (rho*uinf*rlength)/Re;                   /* Viscosity (N*s/m^2) */
    vel2ref = uinf*uinf;                         /* Reference velocity squared (m^2/s^2) */
    dx = (xmax - xmin)/(double)(imax - 1);          /* Delta x (m) */
    dy = (ymax - ymin)/(double)(jmax - 1);          /* Delta y (m) */
    rpi = acos(-one);                            /* Pi = 3.14159... */
    printf("rho,V,L,mu,Re: %f %f %f %f %f\n",rho,uinf,rlength,rmu,Re);
}

/**************************************************************************/

void GS_iteration( boundaryConditionPointer set_boundary_conditions, Array3& u, Array3& uold, Array3& src, Array2& viscx, Array2& viscy, Array2& dt )
{
    /* Copy u to uold (save previous flow values)*/
    uold.copyData(u);

    /* Artificial Viscosity */
    Compute_Artificial_Viscosity(u, viscx, viscy);
    /* Symmetric Gauss-Siedel: Forward Sweep */
    SGS_forward_sweep(u, viscx, viscy, dt, src);
          
    /* Set Boundary Conditions for u */
    set_boundary_conditions(u);
           
    /* Artificial Viscosity */
    Compute_Artificial_Viscosity(u, viscx, viscy);
                 
    /* Symmetric Gauss-Siedel: Backward Sweep */
    SGS_backward_sweep(u, viscx, viscy, dt, src);

    /* Set Boundary Conditions for u */
    set_boundary_conditions(u);
    cout<<"GS_Iteration worked"<<endl;
}

/**************************************************************************/

void PJ_iteration( boundaryConditionPointer set_boundary_conditions, Array3& u, Array3& uold, Array3& src, Array2& viscx, Array2& viscy, Array2& dt )
{
    /* Swap pointers for u and uold*/
    uold.swapData(u);

    /* Artificial Viscosity */
    Compute_Artificial_Viscosity(uold, viscx, viscy);
              
    /* Point Jacobi: Forward Sweep */
    point_Jacobi(u, uold, viscx, viscy, dt, src);
           
    /* Set Boundary Conditions for u */
    set_boundary_conditions(u);
}

/**************************************************************************/

void output_file_headers()
{
  /*
  Uses global variable(s): imms, fp1, fp2
  */
  
  /* Note: The vector of primitive variables is: */
  /*               u = [p, u, v]^T               */  
  /* Set up output files (history and solution)  */    

    fp1 = fopen("./history.dat","w");
    fprintf(fp1,"TITLE = \"Cavity Iterative Residual History\"\n");
    fprintf(fp1,"variables=\"Iteration\"\"Time(s)\"\"Res1\"\"Res2\"\"Res3\"\n");

    fp2 = fopen("./cavity.dat","w");
    fprintf(fp2,"TITLE = \"Cavity Field Data\"\n");
    if(imms==1)
    {
        fprintf(fp2,"variables=\"x(m)\"\"y(m)\"\"p(N/m^2)\"\"u(m/s)\"\"v(m/s)\"");\
        fprintf(fp2,"\"p-exact\"\"u-exact\"\"v-exact\"\"DE-p\"\"DE-u\"\"DE-v\"\n");      
    }
    else
    {
        if(imms==0)
        {
            fprintf(fp2,"variables=\"x(m)\"\"y(m)\"\"p(N/m^2)\"\"u(m/s)\"\"v(m/s)\"\n");
        }      
        else
        {
            printf("ERROR! imms must equal 0 or 1!!!\n");
            exit (0);
        }       
    }

  /* Header for Screen Output */
  printf("Iter. Time (s)   dt (s)      Continuity    x-Momentum    y-Momentum\n"); 
}

/**************************************************************************/

void initial(int& ninit, double& rtime, double resinit[neq], Array3& u, Array3& s)
{
    /* 
    Uses global variable(s): zero, one, irstr, imax, jmax, neq, uinf, pinf 
    To modify: ninit, rtime, resinit, u, s
    */
    int i;                       /* i index (x direction) */
    int j;                       /* j index (y direction) */
    int k;                       /* k index (# of equations) */
  
    double x;       /* Temporary variable for x location */
    double y;       /* Temporary variable for y location */

    /* This subroutine sets inital conditions in the cavity */

    /* Note: The vector of primitive variables is:  */
    /*              u = [p, u, v]^T               */
  
    if(irstr==0)   /* Starting run from scratch*/
    {  
        ninit = 1;          /* set initial iteration to one */
        rtime = zero;       /* set initial time to zero */
        for(k = 0; k<neq; k++)
        {
            resinit[k] = one;
        }
        for(i = 0; i<imax; i++)
        {
            for(j = 0; j<jmax; j++)
            {
                u(i,j,0) = pinf;
                u(i,j,1) = zero;
                u(i,j,2) = zero;

                s(i,j,0) = zero;
                s(i,j,1) = zero;
                s(i,j,2) = zero;
            }
            u(i, jmax-1, 1) = uinf; /* Initialize lid (top) to freestream velocity */
        }
    }  
    else if(irstr==1)  /* Restarting from previous run (file 'restart.in') */
    {
        fp4 = fopen("./restart.in","r"); /* Note: 'restart.in' must exist! */
        if (fp4==NULL)
        {
            printf("Error opening restart file. Stopping.\n");
            exit (0);
        }      
        fscanf(fp4, "%d %lf", ninit, rtime); /* Need to known current iteration # and time value */
        fscanf(fp4, "%lf %lf %lf", &resinit[0], &resinit[1], &resinit[2]); /* Needs initial iterative residuals for scaling */
        for(i=0; i<imax; i++)
        {
            for(j=0; j<jmax; j++)
            {
                fscanf(fp4, "%lf %lf %lf %lf %lf", &x, &y, &u(i,j,0), &u(i,j,1), &u(i,j,2)); 
            }
        }
        ninit += 1;
        printf("Restarting at iteration %d\n", ninit);
        fclose(fp4);
    }   
    else
    {
        printf("ERROR: irstr must equal 0 or 1!\n");
        exit (0);
    }
}


/**************************************************************************/

void bndry( Array3& u )
{
    /* 
    Uses global variable(s): zero, one (not used), two, half, imax, jmax, uinf  
    To modify: u 
    */
    int i;                                          //i index (x direction)
    int j;                                          //j index (y direction)

    /* This applies the cavity boundary conditions */


/* !************************************************************** */
/* !************ADD CODING HERE FOR INTRO CFD STUDENTS************ */
/* !************************************************************** */
 
/* Note: The vector of primitive variables is:  */
    /*              u = [p, u, v]^T               */

for(j = 0; j<jmax; j++)

	{
        u(0,j,1) = zero; /*Ux = 0 Left wall*/
		u(0,j,2) = zero; /*Uy = 0 Left wall*/

        u(imax-1,j,1) = zero; /*Ux = 0 Right wall*/
		u(imax-1,j,2) = zero; /*Uy = 0 Right wall*/

		/*Pressure */
 		u(imax-1,j,0) = two* u(imax-2,j,0) - u(imax-3,j,0);  /*Pressure at Right wall*/



 		u(0,j,0) = two* u(1,j,0) - u(2,j,0); /*Pressure at left wall*/


            }


for(i = 1; i<imax-1; i++)

	{
	    u(i,0,1) = zero; /*Ux = 0 bottom wall*/
	    u(i,0,2) = zero; /*Uy = 0 bottom wall*/

	    u(i,0,0) = two* u(i,1,0) - u(i,2,0);  /*Pressure at bottom wall*/
        /*cout<<u(i,0,0)<< endl;*/



        u(i, jmax-1, 1) = uinf;  /* Initialize lid (top) to freestream velocity */
        u(i, jmax-1, 2) = 0; /*Initialize lid top UY = 0*/
        u(i, jmax-1, 0) = two * u(i,jmax-2,0) - u(i,jmax-3,0); /*Pressure at top wall*/




        }

}

/**************************************************************************/

void bndrymms( Array3& u )
{
    /* 
    Uses global variable(s): two, imax, jmax, neq, xmax, xmin, ymax, ymin, rlength  
    To modify: u
    */
    int i;                       /* i index (x direction) */
    int j;                       /* j index (y direction) */
    int k;                       /* k index (# of equations) */
  
    double x;       /* Temporary variable for x location */
    double y;       /* Temporary variable for y location */

    /* This applies the cavity boundary conditions for the manufactured solution */

    /* Side Walls */
    for( j = 1; j<jmax-1; j++)
    {
        y = (ymax - ymin)*(double)(j)/(double)(jmax - 1);
        i = 0;
        x = xmin;
            
        u(i,j,0)  = umms(x,y,0);
        u(i,j,1)  = umms(x,y,1);
        u(i,j,2)  = umms(x,y,2);

        u(0,j,0) = two*u(1,j,0) - u(2,j,0);    /* 2nd Order BC */

        i=imax-1;
        x = xmax;
            
        u(i,j,0)  = umms(x,y,0);
        u(i,j,1)  = umms(x,y,1);
        u(i,j,2)  = umms(x,y,2);

        u(imax-1,j,0) = two*u(imax-2,j,0) - u(imax-3,j,0);   /* 2nd Order BC */
    }

    /* Top/Bottom Walls */
    for(i=0; i<imax; i++)
    {
        x = (xmax - xmin)*(double)(i)/(double)(imax - 1);
        j = 0;
        y = ymin;

        u(i,j,0)  = umms(x,y,0);
        u(i,j,1)  = umms(x,y,1);
        u(i,j,2)  = umms(x,y,2);

        u(i,0,0) = two*u(i,1,0) - u(i,2,0);   /* 2nd Order BC */

        j = jmax-1;
        y = ymax;
            
        u(i,j,0)  = umms(x,y,0);
        u(i,j,1)  = umms(x,y,1);
        u(i,j,2)  = umms(x,y,2);

        u(i,jmax-1,0) = two*u(i,jmax-2,0) - u(i,jmax-3,0);   /* 2nd Order BC */
    }
}

/**************************************************************************/

void write_output(int n, Array3& u, Array2& dt, double resinit[neq], double rtime)
{
        /* 
    Uses global variable(s): imax, jmax, new, xmax, xmin, ymax, ymin, rlength, imms
    Uses global variable(s): ninit, u, dt, resinit, rtime
    To modify: <none> 
    Writes output and restart files.
    */
   
    int i;                       /* i index (x direction) */
    int j;                       /* j index (y direction) */
    int k;                       /* k index (# of equations) */

    double x;       /* Temporary variable for x location */
    double y;       /* Temporary variable for y location */

    /* Field output */
    fprintf(fp2, "zone T=\"n=%d\"\n",n);
    fprintf(fp2, "I= %d J= %d\n",imax, jmax);
    fprintf(fp2, "DATAPACKING=POINT\n");

    if(imms==1) 
    {
        for(i=0; i<imax; i++)
        {
            for(j=0; j<jmax; j++)
            {
                x = (xmax - xmin)*(double)(i)/(double)(imax - 1);
                y = (ymax - ymin)*(double)(j)/(double)(jmax - 1);
                fprintf(fp2,"%e %e %e %e %e %e %e %e %e %e %e\n", x, y, u(i,j,0), u(i,j,1), u(i,j,2), 
                                               umms(x,y,0), umms(x,y,1), umms(x,y,2), 
                                                (u(i,j,0)-umms(x,y,0)), (u(i,j,1)-umms(x,y,1)), (u(i,j,2)-umms(x,y,2)));
            }
        }    
    }
    else if(imms==0)
    {
        for(i=0; i<imax; i++)
        {
            for(j=0; j<jmax; j++)
            {
                x = (xmax - xmin)*(double)(i)/(double)(imax - 1);
                y = (ymax - ymin)*(double)(j)/(double)(jmax - 1);
                fprintf(fp2,"%e %e %e %e %e\n", x, y, u(i,j,0), u(i,j,1), u(i,j,2));
            }
        }
    }
    else
    {
        printf("ERROR: imms must equal 0 or 1!\n");
        exit (0);
    }

    /* Restart file: overwrites every 'iterout' iteration */
    fp3 = fopen("./restart.out","w");       
    fprintf(fp3,"%d %e\n", n, rtime);    
    fprintf(fp3,"%e %e %e\n", resinit[0], resinit[1], resinit[2]);
    for(i=0; i<imax; i++)
    {
        for(j=0; j<jmax; j++)
        {
            x = (xmax - xmin)*(double)(i)/(double)(imax - 1);
            y = (ymax - ymin)*(double)(j)/(double)(jmax - 1);
            fprintf(fp3,"%e %e %e %e %e\n", x, y, u(i,j,0), u(i,j,1), u(i,j,2));
        }
    }
    fclose(fp3);
}

/**************************************************************************/

double umms(double x, double y, int k)  
{
    /* 
    Uses global variable(s): one, rpi, rlength
    Inputs: x, y, k
    To modify: <none>
    Returns: umms
    */

    double ummstmp; /* Define return value for umms as double precision */

    double termx;      /* Temp variable */
    double termy;      /* Temp variable */
    double termxy;     /* Temp variable */
    double argx;       /* Temp variable */
    double argy;       /* Temp variable */
    double argxy;      /* Temp variable */  
  
    /* This function returns the MMS exact solution */
  
    argx = apx[k]*rpi*x/rlength;
    argy = apy[k]*rpi*y/rlength;
    argxy = apxy[k]*rpi*x*y/rlength/rlength;
    termx = phix[k]*(fsinx[k]*sin(argx)+(one-fsinx[k])*cos(argx));
    termy = phiy[k]*(fsiny[k]*sin(argy)+(one-fsiny[k])*cos(argy));
    termxy = phixy[k]*(fsinxy[k]*sin(argxy)+(one-fsinxy[k])*cos(argxy));
  
    ummstmp = phi0[k] + termx + termy + termxy;
 
    return ummstmp;  
}

/**************************************************************************/

void compute_source_terms( Array3& s )
{
    /* 
    Uses global variable(s): imax, jmax, imms, rlength, xmax, xmin, ymax, ymin
    To modify: s (source terms)
    */

    int i;                       /* i index (x direction) */
    int j;                       /* j index (y direction) */
    
    double x;       /* Temporary variable for x location */
    double y;       /* Temporary variable for y location */

    /* Evaluate Source Terms Once at Beginning (only interior points; will be zero for standard cavity) */
  
    for(i=1; i<imax-1; i++)
    {
        for(j=1; j<jmax-1; j++)
        {
            x = (xmax - xmin)*(double)(i)/(double)(imax - 1);
            y = (ymax - ymin)*(double)(j)/(double)(jmax - 1);
            s(i,j,0) = (double)(imms)*srcmms_mass(x,y);
            s(i,j,1) = (double)(imms)*srcmms_xmtm(x,y);
            s(i,j,2) = (double)(imms)*srcmms_ymtm(x,y);
        }
    }
}

/**************************************************************************/

double srcmms_mass(double x, double y)  
{
    /* 
    Uses global variable(s): rho, rpi, rlength
    Inputs: x, y
    To modify: <none>
    Returns: srcmms_mass
    */

    double srcmasstmp; /* Define return value for srcmms_mass as double precision */

    double dudx;    /* Temp variable: u velocity gradient in x direction */
    double dvdy;  /* Temp variable: v velocity gradient in y direction */
  
    /* This function returns the MMS mass source term */

    dudx = phix[1]*apx[1]*rpi/rlength*cos(apx[1]*rpi*x/rlength) + phixy[1]*apxy[1]*rpi*y/rlength/rlength * cos(apxy[1]*rpi*x*y/rlength/rlength);
  
    dvdy = -phiy[2]*apy[2]*rpi/rlength*sin(apy[2]*rpi*y/rlength) - phixy[2]*apxy[2]*rpi*x/rlength/rlength * sin(apxy[2]*rpi*x*y/rlength/rlength);

    srcmasstmp = rho*dudx + rho*dvdy;

    return srcmasstmp;
} 

/**************************************************************************/

double srcmms_xmtm(double x, double y)  
{
    /* 
    Uses global variable(s): rho, rpi, rmu, rlength
    Inputs: x, y
    To modify: <none>
    Returns: srcmms_xmtm
    */

    double srcxmtmtmp; /* Define return value for srcmms_xmtm as double precision */

    double dudx;    /* Temp variable: u velocity gradient in x direction */
    double dudy;  /* Temp variable: u velocity gradient in y direction */
    double termx;        /* Temp variable */
    double termy;        /* Temp variable */
    double termxy;       /* Temp variable */
    double uvel;         /* Temp variable: u velocity */
    double vvel;         /* Temp variable: v velocity */
    double dpdx;         /* Temp variable: pressure gradient in x direction */
    double d2udx2;       /* Temp variable: 2nd derivative of u velocity in x direction */
    double d2udy2;       /* Temp variable: 2nd derivative of u velocity in y direction */

    /*This function returns the MMS x-momentum source term */

    termx = phix[1]*sin(apx[1]*rpi*x/rlength);
    termy = phiy[1]*cos(apy[1]*rpi*y/rlength);
    termxy = phixy[1]*sin(apxy[1]*rpi*x*y/rlength/rlength);
    uvel = phi0[1] + termx + termy + termxy;
  
    termx = phix[2]*cos(apx[2]*rpi*x/rlength);
    termy = phiy[2]*cos(apy[2]*rpi*y/rlength);
    termxy = phixy[2]*cos(apxy[2]*rpi*x*y/rlength/rlength);
    vvel = phi0[2] + termx + termy + termxy;
  
    dudx = phix[1]*apx[1]*rpi/rlength*cos(apx[1]*rpi*x/rlength) + phixy[1]*apxy[1]*rpi*y/rlength/rlength * cos(apxy[1]*rpi*x*y/rlength/rlength);
  
    dudy = -phiy[1]*apy[1]*rpi/rlength*sin(apy[1]*rpi*y/rlength) + phixy[1]*apxy[1]*rpi*x/rlength/rlength * cos(apxy[1]*rpi*x*y/rlength/rlength);
  
    dpdx = -phix[0]*apx[0]*rpi/rlength*sin(apx[0]*rpi*x/rlength) + phixy[0]*apxy[0]*rpi*y/rlength/rlength * cos(apxy[0]*rpi*x*y/rlength/rlength);

    d2udx2 = -phix[1]*pow((apx[1]*rpi/rlength),2) * sin(apx[1]*rpi*x/rlength) - phixy[1]*pow((apxy[1]*rpi*y/rlength/rlength),2) * sin(apxy[1]*rpi*x*y/rlength/rlength); 
 
    d2udy2 = -phiy[1]*pow((apy[1]*rpi/rlength),2) * cos(apy[1]*rpi*y/rlength) - phixy[1]*pow((apxy[1]*rpi*x/rlength/rlength),2) * sin(apxy[1]*rpi*x*y/rlength/rlength);
  
    srcxmtmtmp = rho*uvel*dudx + rho*vvel*dudy + dpdx - rmu*( d2udx2 + d2udy2 );

    return srcxmtmtmp;
} 

/**************************************************************************/

double srcmms_ymtm(double x, double y)  
{
    /* 
    Uses global variable(s): rho, rpi, rmu, rlength
    Inputs: x, y
    To modify: <none>
    Returns: srcmms_ymtm
    */

    double srcymtmtmp; /* Define return value for srcmms_ymtm as double precision */

    double dvdx;         /* Temp variable: v velocity gradient in x direction */
    double dvdy;         /* Temp variable: v velocity gradient in y direction */
    double termx;        /* Temp variable */
    double termy;        /* Temp variable */
    double termxy;       /* Temp variable */
    double uvel;         /* Temp variable: u velocity */
    double vvel;         /* Temp variable: v velocity */
    double dpdy;         /* Temp variable: pressure gradient in y direction */
    double d2vdx2;       /* Temp variable: 2nd derivative of v velocity in x direction */
    double d2vdy2;       /* Temp variable: 2nd derivative of v velocity in y direction */

    /* This function returns the MMS y-momentum source term */

    termx = phix[1]*sin(apx[1]*rpi*x/rlength);
    termy = phiy[1]*cos(apy[1]*rpi*y/rlength);
    termxy = phixy[1]*sin(apxy[1]*rpi*x*y/rlength/rlength);
    uvel = phi0[1] + termx + termy + termxy;
  
    termx = phix[2]*cos(apx[2]*rpi*x/rlength);
    termy = phiy[2]*cos(apy[2]*rpi*y/rlength);
    termxy = phixy[2]*cos(apxy[2]*rpi*x*y/rlength/rlength);
    vvel = phi0[2] + termx + termy + termxy;
  
    dvdx = -phix[2]*apx[2]*rpi/rlength*sin(apx[2]*rpi*x/rlength) - phixy[2]*apxy[2]*rpi*y/rlength/rlength * sin(apxy[2]*rpi*x*y/rlength/rlength);
  
    dvdy = -phiy[2]*apy[2]*rpi/rlength*sin(apy[2]*rpi*y/rlength) - phixy[2]*apxy[2]*rpi*x/rlength/rlength * sin(apxy[2]*rpi*x*y/rlength/rlength);
  
    dpdy = phiy[0]*apy[0]*rpi/rlength*cos(apy[0]*rpi*y/rlength) + phixy[0]*apxy[0]*rpi*x/rlength/rlength * cos(apxy[0]*rpi*x*y/rlength/rlength);
  
    d2vdx2 = -phix[2]*pow((apx[2]*rpi/rlength),2) * cos(apx[2]*rpi*x/rlength) - phixy[2]*pow((apxy[2]*rpi*y/rlength/rlength),2) * cos(apxy[2]*rpi*x*y/rlength/rlength);
  
    d2vdy2 = -phiy[2]*pow((apy[2]*rpi/rlength),2) * cos(apy[2]*rpi*y/rlength)   - phixy[2]*pow((apxy[2]*rpi*x/rlength/rlength),2) * cos(apxy[2]*rpi*x*y/rlength/rlength);
  
    srcymtmtmp = rho*uvel*dvdx + rho*vvel*dvdy + dpdy - rmu*( d2vdx2 + d2vdy2 );
  
    return srcymtmtmp;  
}

/**************************************************************************/

void compute_time_step( Array3& u, Array2& dt, double& dtmin )
{
    /* 
 * cout <<
    Uses global variable(s): one (not used), two, four, half, fourth
    Uses global variable(s): vel2ref, rmu, rho, dx, dy, cfl, rkappa, imax, jmax
    Uses: u
    To Modify: dt, dtmin
    */
    int i;                      //i index (x direction)
    int j;                      //j index (y direction)
  
    double dtvisc;          //Viscous time step stability criteria (constant over domain)
    double uvel2;           //Local velocity squared
    double beta2;           //Beta squared parameter for time derivative preconditioning
    double lambda_x;        //Max absolute value eigenvalue in (x,t)
    double lambda_y;        //Max absolute value eigenvalue in (y,t)
    double lambda_max;      //Max absolute value eigenvalue (used in convective time step computation)
    double dtconv;          //Local convective time step restriction

/* !************************************************************** */
/* !************ADD CODING HERE FOR INTRO CFD STUDENTS************ */
/* !************************************************************** */

for(i=1; i<imax-1; i++)
{
	for(j=1; j<jmax-1; j++)
	{

	uvel2 = u(i,j,1)* u(i,j,1) + u(i,j,2)* u(i,j,2);

	beta2 = fmax(uvel2,rkappa*uinf);
	lambda_x = 0.5 * (fabs(u(i,j,1)) +  sqrt(pow2(u(i,j,1)) + four*beta2));

	lambda_y = 0.5 * (fabs(u(i,j,2)) +  sqrt(pow2(u(i,j,2)) + four*beta2));

	lambda_max = (lambda_x > lambda_y)? lambda_x:lambda_y;
	
	/*cout << "lambda_x = " << lambda_max << endl;*/
	dtconv = fmin(dx, dy)/lambda_max ;
	
	dtvisc = (dx*dy) / (four*rmu*rhoinv);
	
	dtmin = cfl*fmin(dtconv, dtvisc);
	
	dt(i,j) = dtmin;
        }
}

}  

/**************************************************************************/

void Compute_Artificial_Viscosity( Array3& u, Array2& viscx, Array2& viscy )
{
    /* 
    Uses global variable(s): zero (not used), one (not used), two, four, six, half, fourth (not used)
    Uses global variable(s): imax, jmax, lim (not used), rho, dx, dy, Cx, Cy, Cx2 (not used), Cy2 (not used)
    , fsmall (not used), vel2ref, rkappa
    Uses: u
    To Modify: artviscx, artviscy
    */
    int i;                  //i index (x direction)
    int j;                  //j index (y direction)

    double uvel2;       //Local velocity squared
    double beta2;       //Beta squared parameter for time derivative preconditioning
    double lambda_x;    //Max absolute value e-value in (x,t)
    double lambda_y;    //Max absolute value e-value in (y,t)
    double d4pdx4;      //4th derivative of pressure w.r.t. x
    double d4pdy4;      //4th derivative of pressure w.r.t. y

/* !************************************************************** */
/* !************ADD CODING HERE FOR INTRO CFD STUDENTS************ */
/* !************************************************************** */
for(j=2; j<jmax-2; j++) //for nodes interior of the nodes closest to the wall! 
{
	for(i=2; i<imax-2; i++)
	{

	   uvel2 = pow2(u(i,j,1)) + pow2(u(i,j,2));

           beta2 = fmax(uvel2,rkappa*uinf);
//          cout<<"i index: "<<i<<"\t"<<"j index: "<<j<<endl;
//          cout<<"imax: "<<imax<<"\t"<<"jmax: "<<jmax<<endl;

           lambda_x = 0.5 * (fabs(u(i,j,1)) +  sqrt(uvel2 + four*beta2));
//          cout<<"lamba x: "<<lambda_x<<endl;
           lambda_y = 0.5 * (fabs(u(i,j,2)) +  sqrt(uvel2 + four*beta2));
//          cout<<"lamba y: "<<lambda_y<<endl;

           d4pdx4 = (u(i+2,j,0) - four*u(i+1,j,0) + six*u(i,j,0) - four*u(i-1,j,0) + u(i-2,j,0))/ double(dx);

//         cout<< "d4pdx4="<< d4pdx4<<endl;

           d4pdy4 = (u(i,j+2,0) - four*u(i,j+1,0) + six*u(i,j,0) - four*u(i,j-1,0) + u(i,j-2,0))/ double(dy);

//         cout<< "d4pdy4="<< d4pdy4<<endl;

           viscx(i,j) = (-fabs(lambda_x)* Cx *d4pdx4)/beta2;


           viscy(i,j) = (-fabs(lambda_y)* Cy *d4pdy4)/beta2;

//        cout<< "viscx="<< viscx(i,j)<<endl;
//        cout<< "viscy="<< viscy(i,j)<<endl;
        }
}
//*********LINEAR EXTRAPOLATIONS*************//

int sides[2] = {1,imax-2};
int top_bottom[2] = {1,jmax-2};

for(auto i:sides){ // for nodes closest to side boundaries
  for(j=1;j<jmax-1;j++){

    if(i==1){
      // for x-component of articial viscosity
      double slope_x = (viscx(i+2,j)-viscx(i+1,j)) / dx;
      viscx(i,j) = viscx(i+1,j) + (slope_x*dx);
    
      //for y-component of artifcial viscosity
      double slope_y = (viscx(i+2,j)-viscx(i+1,j)) / dx;
      viscx(i,j) = viscx(i+1,j) + (slope_y*dx);
    }
    if(i==imax-1){
      // for x-component of articial viscosity
      double slope_x = (viscx(i-2,j)-viscx(i-1,j)) / dx;
      viscx(i,j) = viscx(i-1,j) + (slope_x*dx);
    
      //for y-component of artifcial viscosity
      double slope_y = (viscx(i-2,j)-viscx(i-1,j)) / dx;
      viscx(i,j) = viscx(i-1,j) + (slope_y*dx);
     }
  }
}
for(auto j:top_bottom){ // for nodes closest to top & bottom boundaries
  for(i=1;i<imax-1;i++){

    if(j==1){
      // for x-component of articial viscosity
      double slope_x = (viscx(i,j+2)-viscx(i,j+1)) / dy;
      viscx(i,j) = viscx(i,j+1) + (slope_x*dy);
    
      //for y-component of artifcial viscosity
      double slope_y = (viscy(i,j+2)-viscy(i,j+1)) / dy;
      viscx(i,j) = viscy(i,j+1) + (slope_y*dy);
    }
    if(j==jmax-1){
      // for x-component of articial viscosity
      double slope_x = (viscx(i,j-2)-viscx(i,j-1)) / dy;
      viscx(i,j) = viscx(i-1,j) + (slope_x*dy);
    
      //for y-component of artifcial viscosity
      double slope_y = (viscy(i,j-2)-viscy(i,j-1)) / dy;
      viscx(i,j) = viscy(i,j-1) + (slope_y*dy);
     }
  
  }
}

}

/**************************************************************************/

void SGS_forward_sweep( Array3& u, Array2& viscx, Array2& viscy, Array2& dt, Array3& s )
{
    /* 
    Uses global variable(s): two, three (not used), six (not used), half
    Uses global variable(s): imax, jmax, ipgorder (not used), rho, rhoinv, dx, dy, rkappa,
                        xmax (not used), xmin (not used), ymax (not used), ymin (not used), 
                        rmu, vel2ref
    Uses: artviscx, artviscy, dt, s
    To Modify: u
    */
 
    double dpdx;        //First derivative of pressure w.r.t. x
    double dudx;        //First derivative of x velocity w.r.t. x
    double dvdx;        //First derivative of y velocity w.r.t. x
    double dpdy;        //First derivative of pressure w.r.t. y
    double dudy;        //First derivative of x velocity w.r.t. y
    double dvdy;        //First derivative of y velocity w.r.t. y
    double d2udx2;      //Second derivative of x velocity w.r.t. x
    double d2vdx2;      //Second derivative of y velocity w.r.t. x
    double d2udy2;      //Second derivative of x velocity w.r.t. y
    double d2vdy2;      //Second derivative of y velocity w.r.t. y
    double uvel2;       //velocity squared at node
    double beta2;       //Beta squared parameter for time derivative preconditioning

    /* Symmetric Gauss-Siedel: Forward Sweep */ 

/* !************************************************************** */
/* !************ADD CODING HERE FOR INTRO CFD STUDENTS************ */
/* !************************************************************** */
/****ONLY FOR 1 ITERATION STEP*******/
  for (auto j=1;j<jmax-1;j++){ //inner nodes only - STARTING FROM node i=1,j=1
    for (auto i=1;i<imax-1;i++){
     //local constants
     uvel2 = pow2(u(i,j,1)) + pow2(u(i,j,2)); //local velocity mag.
     beta2 = fmax(uvel2,rkappa*uinf); //time preconditioning constant

     dpdx = (u(i+1,j,0)-u(i-1,j,0))/(two*dx); //pressure derivatives
     dpdy = (u(i,j+1,0)-u(i,j-1,0))/(two*dy);

     dudx = (u(i+1,j,1)-u(i-1,j,1))/(two*dx); //u velocity derivatives
     dudy = (u(i,j+1,1)-u(i,j-1,1))/(two*dy);

     d2udx2 = (u(i+1,j,1)-2*u(i,j,1)+u(i-1,j,1))/(dx*dx);
     d2udy2 = (u(i,j+1,1)-2*u(i,j,1)+u(i,j-1,1))/(dy*dy);

     dvdx = (u(i+1,j,2)-u(i-1,j,2))/(two*dx); //v velocity derivatives
     dvdy = (u(i,j+1,2)-u(i,j-1,2))/(two*dy);

     d2vdx2 = (u(i+1,j,2)-two*u(i,j,2)+u(i-1,j,2))/(dx*dx);
     d2vdy2 = (u(i,j+1,2)-two*u(i,j,2)+u(i,j-1,2))/(dy*dy);
     // ----continuity equation----------
     double continuity_it_resid = (rho*dudx) + (rho*dvdy) - viscx(i,j) - viscy(i,j) - s(i,j,0); //steady-state iterative residual for continuity equation

     u(i,j,0) = u(i,j,0) - beta2*dt(i,j)*continuity_it_resid; //updates pressure value of node i,j

     // ----x-momentum equation----------
     double xmomentum_it_resid = (rho*u(i,j,1)*dudx) + (rho*u(i,j,2)*dudy) + dpdx - (rmu*d2udx2) - (rmu*d2udy2) - s(i,j,1); //steady-state iterative residual for x-momentum equation

     u(i,j,1) = u(i,j,1) - dt(i,j)*rhoinv*xmomentum_it_resid; //updates u-velocity value of node i,j
     
     // ----y-momentum equation---------- 
     double ymomentum_it_resid = (rho*u(i,j,1)*dvdx) + (rho*u(i,j,2)*dvdy) + dpdy - (rmu*d2vdx2) - (rmu*d2vdy2) - s(i,j,2); //steady-state iterative residval for y-momentum equation

     u(i,j,2) = u(i,j,2) - dt(i,j)*rhoinv*ymomentum_it_resid; //updates v-velocity value of node i,j
    }
  }



}

/**************************************************************************/

void SGS_backward_sweep( Array3& u, Array2& viscx, Array2& viscy, Array2& dt, Array3& s )
{
    /* 
    Uses global variable(s): two, three (not used), six (not used), half
    Uses global variable(s): imax, jmax, ipgorder (not used), rho, rhoinv, dx, dy, rkappa,
                        xmax (not used), xmin (not used), ymax (not used), ymin (not used), 
                        rmu, vel2ref
    Uses: artviscx, artviscy, dt, s
    To Modify: u
    */
 
    double dpdx;        //First derivative of pressure w.r.t. x
    double dudx;        //First derivative of x velocity w.r.t. x
    double dvdx;        //First derivative of y velocity w.r.t. x
    double dpdy;        //First derivative of pressure w.r.t. y
    double dudy;        //First derivative of x velocity w.r.t. y
    double dvdy;        //First derivative of y velocity w.r.t. y
    double d2udx2;      //Second derivative of x velocity w.r.t. x
    double d2vdx2;      //Second derivative of y velocity w.r.t. x
    double d2udy2;      //Second derivative of x velocity w.r.t. y
    double d2vdy2;      //Second derivative of y velocity w.r.t. y
    double uvel2;       //Velocity squared at node
    double beta2;       //Beta squared parameter for time derivative preconditioning

    /* Symmetric Gauss-Siedel: Backward Sweep  */


/* !************************************************************** */
/* !************ADD CODING HERE FOR INTRO CFD STUDENTS************ */
/* !************************************************************** */

/****ONLY FOR 1 ITERATION STEP*******/
  for (auto j=jmax-2;j>0;j--){ //inner nodes only - STARTING FROM node i=imax-2,j=jmax-2
    for (auto i=imax-2;i>0;i--){
     //local constants
     uvel2 = pow2(u(i,j,1)) + pow2(u(i,j,2)); //local velocity mag.
     beta2 = fmax(uvel2,rkappa*uinf); //time preconditioning constant

     dpdx = (u(i+1,j,0)-u(i-1,j,0))/(two*dx); //pressure derivatives
     dpdy = (u(i,j+1,0)-u(i,j-1,0))/(two*dy);

     dudx = (u(i+1,j,1)-u(i-1,j,1))/(two*dx); //u velocity derivatives
     dudy = (u(i,j+1,1)-u(i,j-1,1))/(two*dy);

     d2udx2 = (u(i+1,j,1)-two*u(i,j,1)+u(i-1,j,1))/(dx*dx);
     d2udy2 = (u(i,j+1,1)-two*u(i,j,1)+u(i,j-1,1))/(dy*dy);

     dvdx = (u(i+1,j,2)-u(i-1,j,2))/(two*dx); //v velocity derivatives
     dvdy = (u(i,j+1,2)-u(i,j-1,2))/(two*dy);

     d2vdx2 = (u(i+1,j,2)-two*u(i,j,2)+u(i-1,j,2))/(dx*dx);
     d2vdy2 = (u(i,j+1,2)-two*u(i,j,2)+u(i,j-1,2))/(dy*dy);
 
     // ----continuity equation----------
     double continuity_it_resid = (rho*dudx) + (rho*dvdy) - viscx(i,j) - viscy(i,j) - s(i,j,0); //steady-state iterative residual for continuity equation

     u(i,j,0) = u(i,j,0) - beta2*dt(i,j)*continuity_it_resid; //updates pressure value of node i,j

     // ----x-momentum equation----------
     double xmomentum_it_resid = (rho*u(i,j,1)*dudx) + (rho*u(i,j,2)*dudy) + dpdx - (rmu*d2udx2) - (rmu*d2udy2) - s(i,j,1); //steady-state iterative residual for x-momentum equation

     u(i,j,1) = u(i,j,1) - dt(i,j)*rhoinv*xmomentum_it_resid; //updates v-velocity value of node i,j
     
     // ----y-momentum equation---------- 
     double ymomentum_it_resid = (rho*u(i,j,1)*dvdx) + (rho*u(i,j,2)*dvdy) + dpdy - (rmu*d2vdx2) - (rmu*d2vdy2) - s(i,j,2); //steady-state iterative residval for y-momentum equation

     u(i,j,2) = u(i,j,2) - dt(i,j)*rhoinv*ymomentum_it_resid; //updates v-velocity value of node i,j
    }
  }




}

/**************************************************************************/

void point_Jacobi( Array3& u, Array3& uold, Array2& viscx, Array2& viscy, Array2& dt, Array3& s )
{
    /* 
    Uses global variable(s): two, three (not used), six (not used), half
    Uses global variable(s): imax, jmax, ipgorder (not used), rho, rhoinv, dx, dy, rkappa,
                        xmax (not used), xmin (not used), ymax (not used), ymin (not used), 
                        rmu, vel2ref
    Uses: uold, artviscx, artviscy, dt, s
    To Modify: u
    */
 
    double dpdx;        //First derivative of pressure w.r.t. x
    double dudx;        //First derivative of x velocity w.r.t. x
    double dvdx;        //First derivative of y velocity w.r.t. x
    double dpdy;        //First derivative of pressure w.r.t. y
    double dudy;        //First derivative of x velocity w.r.t. y
    double dvdy;        //First derivative of y velocity w.r.t. y
    double d2udx2;      //Second derivative of x velocity w.r.t. x
    double d2vdx2;      //Second derivative of y velocity w.r.t. x
    double d2udy2;      //Second derivative of x velocity w.r.t. y
    double d2vdy2;      //Second derivative of y velocity w.r.t. y
    double uvel2;       //Velocity squared at node
    double beta2;       //Beta squared parameter for time derivative preconditioning

    /* Point Jacobi method */


/* !************************************************************** */
/* !************ADD CODING HERE FOR INTRO CFD STUDENTS************ */
/* !************************************************************** */

    int i;
    int j;

for(int i=1; i<imax-1; i++){
        for(j=1; j<jmax-1; j++){
            dpdx = (uold(i+1,j,0)-uold(i-1,j,0))/(two*dx);
            dpdy = (uold(i,j+1,0)-uold(i,j-1,0))/(two*dy);

            dudx = (uold(i+1,j,1)-uold(i-1,j,1))/(two*dx);
            dudy = (uold(i,j+1,1)-uold(i,j-1,1))/(two*dy);

            dvdx = (uold(i+1,j,2)-uold(i-1,j,2))/(two*dx);
            dvdy = (uold(i,j+1,2)-uold(i,j-1,2))/(two*dy);

            d2udx2 = (uold(i+1,j,1)-two*uold(i,j,1)+uold(i-1,j,1))/pow2(dx);
            d2udy2 = (uold(i,j+1,1)-two*uold(i,j,1)+uold(i,j-1,1))/pow2(dy);

            d2vdx2 = (uold(i+1,j,2)-two*uold(i,j,2)+uold(i-1,j,2))/pow2(dx);
            d2vdy2 = (uold(i,j+1,2)-two*uold(i,j,2)+uold(i,j-1,2))/pow2(dy);

            uvel2 = pow2(u(i,j,1)) + pow2(u(i,j,2));

            beta2 = fmax(uvel2,rkappa*vel2ref);

            u(i,j,0) = uold(i,j,0)- (beta2*dt(i,j)*((rho*dudx)+ (rho*dvdy)-viscx(i,j)-viscy(i,j)-s(i,j,0)));

            u(i,j,1) = uold(i,j,1) - ((dt(i,j)*rhoinv)*((rho*uold(i,j,1)*dudx) + (rho*uold(i,j,2)*dudy) +(dpdx)-(rmu *d2udx2)-(rmu*d2udy2)-s(i,j,1)));

            u(i,j,2) = uold(i,j,2) - ((dt(i,j)*rhoinv)*((rho*uold(i,j,1)*dvdx) + (rho*uold(i,j,2)*dvdy) +(dpdy)-(rmu *d2vdx2)-(rmu*d2vdy2)-s(i,j,2)));

            //cout<< "p="<< u(i,j,0)<<endl;
            //cout<< "u="<< u(i,j,1)<<endl;
            //cout<< "v="<< u(i,j,2)<<endl;

        }
}


}



/**************************************************************************/

void pressure_rescaling( Array3& u )
{
    /* 
    Uses global variable(s): imax, jmax, imms, xmax, xmin, ymax, ymin, rlength, pinf
    To Modify: u
    */

    int iref;                     /* i index location of pressure rescaling point */
    int jref;                     /* j index location of pressure rescaling point */

    double x;               /* Temporary variable for x location */
    double y;               /* Temporary variable for y location */  
    double deltap;          /* delta_pressure for rescaling all values */

    iref = (imax-1)/2;     /* Set reference pressure to center of cavity */
    jref = (jmax-1)/2;
    if(imms==1)
    {
        x = (xmax - xmin)*(double)(iref)/(double)(imax - 1);
        y = (ymax - ymin)*(double)(jref)/(double)(jmax - 1);
        deltap = u(iref,jref,0) - umms(x,y,0); /* Constant in MMS */
    }
    else
    {
        deltap = u(iref,jref,0) - pinf; /* Reference pressure */
    }

    for(int i=0; i<imax; i++)
    {
        for(int j=0; j<jmax; j++)
        {
            u(i,j,0) -= deltap;
        }
    }      
}  

/**************************************************************************/

void check_iterative_convergence(int n, Array3& u, Array3& uold, Array2& dt, double res[neq], double resinit[neq], int ninit, double rtime, double dtmin, double& conv)
{
  /* 
  Uses global variable(s): zero
  Uses global variable(s): imax, jmax, neq, fsmall (not used)
  Uses: n, u, uold, dt, res, resinit, ninit, rtime, dtmin
  To modify: conv
  */

  int i;                       /* i index (x direction) */
  int j;                       /* j index (y direction) */
  int k;                       /* k index (# of equations) */

  /* Compute iterative residuals to monitor iterative convergence */

    res[0] = zero;              //Reset to zero (as they are sums)
    res[1] = zero;
    res[2] = zero;

  double beta2;
  double uvel2;

  double local_resid = 0.0; /*Stores sum of all res[0]*/
  

  double L2Norminit =0; /*To Calculate initial L2norm*/

/* !************************************************************** */
/* !************ADD CODING HERE FOR INTRO CFD STUDENTS************ */
/* !************************************************************** */
   for(i=1; i<imax-1; i++){
        for(j=1; j<jmax-1; j++){
            for (k=0; k<neq; k++){
               
               /* cout<<"Pressure: "<<"new:"<<u(i,j,0)<<"\t"<<"old:"<<uold(i,j,0)<<endl;
                cout<<"U-Velocity: "<<"new:"<<u(i,j,1)<<"\t"<<"old:"<<uold(i,j,1)<<endl;
                cout<<"V-Velocity: "<<"new:"<<u(i,j,2)<<"\t"<<"old:"<<uold(i,j,2)<<endl;*/
                
                if(k==0){ //continuity equation

                    //time preconditioning term
	            uvel2 = pow2(u(i,j,1)) + pow2(u(i,j,2));

     	            beta2 = fmax(uvel2,rkappa*uinf);

                    local_resid = (u(i,j,0)-uold(i,j,0)) / (-beta2*dt(i,j)); 
                    //cout << "Beta2 value(for continuity): "<<beta2<<endl; 
                    //cout << "time step(for continuity): "<<dt(i,j)<<endl; 
            //        cout<<"local continuity residual: "<<res[k]<<endl;

                }else if (k==1){ //x-momentum equation
                    local_resid = -rho*(u(i,j,1)-uold(i,j,1)) / dt(i,j); 
          //          cout<<"local x-momentum residual: "<<res[k]<<endl;

                }else if (k==2){ //y-momentum equation
                    local_resid = -rho*(u(i,j,2)-uold(i,j,2)) / dt(i,j); 
        //            cout<<"local y-momentum residual: "<<res[k]<<endl;
                }
                res[k] += pow2(fabs(local_resid));
                
                }
            }

        }

        //Norms of each equation
	res[0] = sqrt(res[0]/ double(imax*jmax)); //continuity norm
        res[1] = sqrt(res[1]/ double(imax*jmax)); //x-momentum norm
        res[2] = sqrt(res[2]/ double(imax*jmax)); //y-momentum norm

        //cout<<"Continuity iterative residual L2 norm: "<<norm_continuity<<endl;
        //cout<<"X-Momentum iterative residual L2 norm: "<<norm_xmomentum<<endl;
        //cout<<"Y-Momentum iterative residual L2 norm: "<<norm_ymomentum<<endl;

        L2Norminit = sqrt(pow2(resinit[0])/(imax*jmax));

        cout<<"L2Norminit: "<<L2Norminit<<endl;
        conv = fmax(res[0],fmax(res[1],res[2])) / L2Norminit; /*L2 Norms ratio*/

        cout<<"conv: "<<conv<<endl;
  
  
    /* Write iterative residuals every "residualOut" iterations */
    if( ((n%residualOut)==0)||(n==ninit) )
    {
        fprintf(fp1, "%d %e %e %e %e\n",n, rtime, res[0], res[1], res[2] );
        printf("%d   %e   %e   %e   %e   %e\n",n, rtime, dtmin, res[0], res[1], res[2] );    

        /* Write header for iterative residuals every 20 residual printouts */
        if( ((n%(residualOut*20))==0)||(n==ninit) )
        {
            printf("Iter. Time (s)   dt (s)      Continuity    x-Momentum    y-Momentum\n"); 
        }    
    }
     
}

/**************************************************************************/

void Discretization_Error_Norms( Array3& u ) 
{
    /* 
    Uses global variable(s): zero
    Uses global variable(s): imax, jmax, neq, imms, xmax, xmin, ymax, ymin, rlength (not used)
    Uses: u
    To modify: rL1norm, rL2norm, rLinfnorm 
    */

    double x;                   //Temporary variable for x location
    double y;                   //Temporary variable for y location
    double DE;                      //Discretization error (absolute value)

    double rL1norm[neq];
    double rL2norm[neq]; 
    double rLinfnorm[neq];
    
    double rL1[neq];
    double rL2[neq];
    double rLinf[neq];

    int i;
    int j;
    int k;

    /* Only compute discretization error norms for manufactured solution */
    if(imms==1)
    {

/* !************************************************************** */
/* !************ADD CODING HERE FOR INTRO CFD STUDENTS************ */
/* !************************************************************** */
/* Initialize error norms*/

      for (int k = 0; k < neq; k++) {
        rL1norm[k] = 0.0;
        rL2norm[k] = 0.0;
        rLinfnorm[k] = 0.0;
      }

      for(int i=1; i<imax-1; i++)
      {
        for(int j=1; j<jmax-1; j++)
        {
            x = (xmax - xmin)*(double)(i)/(double)(imax - 1);
            y = (ymax - ymin)*(double)(j)/(double)(jmax - 1);

            /*Calculating Discretization Error*/
            for(int k = 0; k<neq; k++) {
              DE = fabs(u(i,j,k)- umms(x,y,k));

              /*Calculating Error */

              rL1[k] += DE;
              rL2[k] += pow2(DE);
              rLinf[k] = fmax(rLinf[k], DE);
            }

        }
     }
     /*Norm Calculation*/
     for (int k=0; k < neq; k++){
       rL1norm[k]= rL1[k]/(imax * jmax);
       rL2norm[k]= sqrt(rL2[k]/(imax*jmax));
       rLinfnorm[k] = rLinf[k];


     }


   }
   cout<<"Continuity DE Norms:\n"<<endl;cout<<"L1Norm: "<<rL1norm[0]<<" L2Norm: "<<rL2norm[0]<<" LinfNorm: "<<rLinfnorm[0]<<endl; 
   cout<<"X-Momentum DE Norms:\n"<<endl;cout<<"L1Norm: "<<rL1norm[1]<<" L2Norm: "<<rL2norm[1]<<" LinfNorm: "<<rLinfnorm[1]<<endl;
   cout<<"Y-Momentum DE Norms:\n"<<endl;cout<<"L1Norm: "<<rL1norm[2]<<" L2Norm: "<<rL2norm[2]<<" LinfNorm: "<<rLinfnorm[2]<<endl;
}

/********************************************************************************************************************/
/*                                                                                                                  */
/*                                                End Functions                                                     */
/*                                                                                                                  */
/********************************************************************************************************************/







/********************************************************************************************************************/
/*                                                                                                                  */
/*                                                Main Function                                                     */
/*                                                                                                                  */
/********************************************************************************************************************/
int main()
{
    //Data class declarations: hold all the data needed across the entire grid
    Array3 u     (imax, jmax, neq);     //u and uold store the current and previous primitive variable solution on the entire grid
    Array3 uold  (imax, jmax, neq);

    Array3 src   (imax, jmax, neq);     //src stores the source terms over the entire grid (used for MMS)

    Array2 viscx (imax, jmax);          //Artificial viscosity, x and y directions
    Array2 viscy (imax, jmax);

    Array2 dt    (imax, jmax);          //Local timestep array



    /* Minimum of iterative residual norms from three equations */
    double conv;
    double resTest;
    int n = 0;  //Iteration number

                                                      
    /*--------- Solution variables declaration ----------------------*/
      
     int ninit = 0;                 /* Initial iteration number (used for restart file) */

     double res[neq];               /* Iterative residual for each equation */
     double resinit[neq];           /* Initial iterative residual for each equation (from iteration 1) */
     double rtime;                  /* Variable to estimate simulation time */
     double dtmin = 1.0e99;         /* Minimum time step for a given iteration (initialized large) */

     double x;                      /* Temporary variable for x location */
     double y;                      /* Temporary variable for y location */


    /*-------Set Function Pointers-----------------------------------*/
    
    iterationStepPointer     iterationStep;
    boundaryConditionPointer set_boundary_conditions;

    if(isgs==1)                 /* ==Symmetric Gauss Seidel== */
    {
        iterationStep = &GS_iteration;
    }
    else if(isgs==0)             /* ==Point Jacobi== */
    {
        iterationStep = &PJ_iteration;
    }
    else
    {
        printf("ERROR: isgs must equal 0 or 1!\n");
        exit (0);  
    }
      
    if(imms==0) 
    {
            set_boundary_conditions = &bndry;
    }
    else if(imms==1)
        {
            set_boundary_conditions = &bndrymms;
        }
        else
        {
            printf("ERROR: imms must equal 0 or 1!\n");
            exit (0);
        }

    /*-------End Set Function Pointers-------------------------------*/

    /* Debug output: Uncomment and modify if debugging */
    //$$$$$$ fp6 = fopen("./Debug.dat","w");
    //$$$$$$ fprintf(fp6,"TITLE = \"Debug Data Data\"\n");
    //$$$$$$ fprintf(fp6,"variables=\"x(m)\"\"y(m)\"\"visc-x\"\"visc-y\"\n");
    //$$$$$$ fprintf(fp6, "zone T=\"n=%d\"\n",n);
    //$$$$$$ fprintf(fp6, "I= %d J= %d\n",imax, jmax);
    //$$$$$$ fprintf(fp6, "DATAPACKING=POINT\n");

    /* Set derived input quantities */
    set_derived_inputs();

    /* Set up headers for output files */
    output_file_headers();

    /* Set Initial Profile for u vector */
    initial( ninit, rtime, resinit, u, src );   

    /* Set Boundary Conditions for u */
    set_boundary_conditions( u );

    /* Write out inital conditions to solution file */
    write_output(ninit, u, dt, resinit, rtime);
     
    /* Evaluate Source Terms Once at Beginning */
    /*(only interior points; will be zero for standard cavity) */
    compute_source_terms( src );

    /*========== Main Loop ==========*/
    for (n = ninit; n<= nmax; n++)
    {
        /* Calculate time step */  
        compute_time_step( u, dt, dtmin );
           
        /* Perform main iteration step (point jacobi or gauss seidel)*/    
        iterationStep( set_boundary_conditions, u, uold, src, viscx, viscy, dt ); 

        /* Pressure Rescaling (based on center point) */
        pressure_rescaling( u );

        /* Update the time */
        rtime += dtmin;

        /* Check iterative convergence using L2 norms of iterative residuals */
        check_iterative_convergence(n, u, uold, dt, res, resinit, ninit, rtime, dtmin, conv);

        if(conv<toler) 
        {
            fprintf(fp1, "%d %e %e %e %e\n",n, rtime, res[0], res[1], res[2]);
                goto converged;
        }
            
        /* Output solution and restart file every 'iterout' steps */
        if( ((n%iterout)==0) ) 
        {
                write_output(n, u, dt, resinit, rtime);
        }
        
    }  /* ========== End Main Loop ========== */

    printf("\nSolver stopped in %d iterations because the specified maximum number of timesteps was exceeded.\n", nmax);
        
    goto notconverged;
        
converged:  /* go here once solution is converged */

    printf("\nSolver stopped in %d iterations because the convergence criteria was met OR because the solution diverged.\n", n);
    printf("   Solution divergence is indicated by inf or NaN residuals.\n", n);
    
notconverged:

    /* Calculate and Write Out Discretization Error Norms (will do this for MMS only) */
    Discretization_Error_Norms( u );

    /* Output solution and restart file */
    write_output(n, u, dt, resinit, rtime);

    /* Close open files */
    fclose(fp1);
    fclose(fp2);
    //$$$$$$   fclose(fp6); /* Uncomment for debug output */

    return 0;
}

