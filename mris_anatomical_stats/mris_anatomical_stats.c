
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "mrisurf.h"
#include "mri.h"
#include "macros.h"
#include "fio.h"

static char vcid[] = "$Id: mris_anatomical_stats.c,v 1.9 2003/03/26 17:40:16 fischl Exp $";

int main(int argc, char *argv[]) ;

static int  get_option(int argc, char *argv[]) ;
static void usage_exit(void) ;
static void print_usage(void) ;
static void print_help(void) ;
static void print_version(void) ;
double MRISmeasureTotalWhiteMatterVolume(MRI *mri) ;
double MRISmeasureCorticalGrayMatterVolume(MRI_SURFACE *mris) ;
int MRIScomputeCurvatureStats(MRI_SURFACE *mris, double *pavg, double *pvar,
                              float ignore_below, float ignore_above) ;

double MRIScomputeAbsoluteCurvature(MRI_SURFACE *mris) ;
int    MRISreadAnnotFile(MRI_SURFACE *mris, char *fname) ;
int    MRISripVerticesWithMark(MRI_SURFACE *mris, int mark) ;
int    MRISripVerticesWithoutMark(MRI_SURFACE *mris, int mark) ;
int    MRISrestoreSurface(MRI_SURFACE *mris) ;
int    MRISreplaceMarks(MRI_SURFACE *mris, int in_mark, int out_mark) ;

char *Progname ;
static double sigma = 0.0f ;
static float ignore_below = 0 ;
static float ignore_above = 20 ;

static char *label_name = NULL ;
static char *annotation_name = NULL ;
static char *thickness_name = "thickness" ;

static int histo_flag = 0 ;
static char *gray_histo_name ;
static char *mri_name = "T1" ;
static int noheader = 0 ;
static char *log_file_name = NULL ;

int
main(int argc, char *argv[])
{
  char          **av, *hemi, *sname, sdir[400], *cp, fname[500], *surf_name ;
  int           ac, nargs, vno ;
  MRI_SURFACE   *mris ;
  MRI           *mri_wm, *mri_kernel = NULL, *mri_orig ;
  double        gray_volume, wm_volume, thickness_mean, thickness_var,
                total_abs_mean_curvature, total_abs_gaussian_curvature, ici, fi ;
  int           mark = 0 ;
  FILE          *log_fp = NULL ;
  VERTEX        *v ;
  HISTOGRAM     *histo_gray ;

  total_abs_mean_curvature = total_abs_gaussian_curvature = gray_volume = 0.0 ;
  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  ac = argc ;
  av = argv ;
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++)
  {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (argc < 3)
    usage_exit() ;

  sname = argv[1] ;
  cp = getenv("SUBJECTS_DIR") ;
  if (!cp)
    ErrorExit(ERROR_BADPARM, 
              "%s: SUBJECTS_DIR not defined in environment.\n", Progname) ;
  strcpy(sdir, cp) ;

  hemi = argv[2] ; 
  if (argc > 3)
    surf_name = argv[3] ;
  else
    surf_name = WHITE_MATTER_NAME ;

  if (sigma > 0.0)
    mri_kernel = MRIgaussian1d(sigma, 100) ;
  sprintf(fname, "%s/%s/mri/wm", sdir, sname) ;
  fprintf(stderr, "reading volume %s...\n", fname) ;
  mri_wm = MRIread(fname) ;
  if (!mri_wm)
    ErrorExit(ERROR_NOFILE, "%s: could not read input volume %s",
              Progname, fname) ;

  if (mri_kernel)
  {
    fprintf(stderr, "smoothing brain volume with sigma = %2.3f\n", sigma) ;
    MRIconvolveGaussian(mri_wm, mri_wm, mri_kernel) ;
#if 0
    fprintf(stderr, "smoothing wm volume with sigma = %2.3f\n", sigma) ;
    MRIconvolveGaussian(mri_wm, mri_wm, mri_kernel) ;
    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON) 
      MRIwrite(mri_wm, "/tmp/wm_smooth.mnc") ;
#endif
    MRIfree(&mri_kernel);
  }

  sprintf(fname, "%s/%s/surf/%s.%s", sdir, sname, hemi, surf_name) ;
  fprintf(stderr, "reading input surface %s...\n", fname) ;
  mris = MRISread(fname) ;
  if (!mris)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s",
              Progname, fname) ;
  MRISsaveVertexPositions(mris, ORIGINAL_VERTICES) ;
  MRIScomputeMetricProperties(mris) ;
  wm_volume = MRISmeasureTotalWhiteMatterVolume(mri_wm) ;
#if 0
  fprintf(stderr, "measuring gray matter thickness of surface...\n") ;
  sprintf(fname, "%s/%s/surf/%s.%s", sdir, sname, hemi, GRAY_MATTER_NAME) ;
  fprintf(stderr, "reading pial surface position from %s...\n", fname) ;
  MRISreadVertexPositions(mris, fname) ;
#else
  MRISreadCurvatureFile(mris, thickness_name) ;
  
#endif

  if (label_name)
  {
    LABEL  *area ;

    area = LabelRead(sname, label_name) ;
    if (!area)
      ErrorExit(ERROR_NOFILE, "%s: could not read label file %s\n", sname) ;
    LabelRipRestOfSurface(area, mris) ;
    LabelFree(&area) ;
    MRIScomputeMetricProperties(mris) ;
  }

  if (histo_flag)
  { 
    sprintf(fname, "%s/%s/mri/%s", sdir, sname, mri_name) ;
    fprintf(stderr, "reading volume %s...\n", fname) ;
    mri_orig = MRIread(fname) ;
    if (!mri_orig)
      ErrorExit(ERROR_NOFILE, "%s: could not read input volume %s",
                Progname, fname) ;
    histo_gray = HISTOalloc(256) ; 
  }
  else
  { 
    histo_gray = NULL ; mri_orig = NULL ;
  }

#if 0
  fprintf(stderr, "measuring cortical thickness...") ;
  MRISmeasureCorticalThickness(mris, mri_wm) ;
#endif
  MRIScopyCurvatureToImagValues(mris) ;   /* save thickness measures */

  fprintf(stderr, "done.\ncomputing second fundamental form...") ;
  MRISsetNeighborhoodSize(mris, 2) ;
  MRIScomputeSecondFundamentalForm(mris) ;
  if (annotation_name)
    MRISreadAnnotFile(mris, annotation_name) ;
  fprintf(stderr, "done.\n") ;

  if (log_file_name)
  {
    log_fp = fopen(log_file_name, "a") ;
    if (!log_fp)
      ErrorExit(ERROR_NOFILE, "%s: could not open log file %s", Progname, 
                log_file_name) ;
  }
    
  fprintf(stdout, "total white matter volume               = %2.0f mm^3\n", 
          wm_volume) ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    if (!histo_flag && annotation_name == NULL)
      break ;
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (histo_flag)
    {
      double d, xw, yw, zw, x, y, z, thickness, val ;
      
      thickness = v->imag_val ;

      for (d = 0.5 ; d < (thickness-0.5) ; d += 0.1)
      {
        x = v->x+d*v->nx ; y = v->y+d*v->ny ; z = v->z+d*v->nz ;
        MRIworldToVoxel(mri_orig, x, y, z, &xw, &yw, &zw) ;
        MRIsampleVolume(mri_orig, xw, yw, zw, &val) ;
        HISTOaddSample(histo_gray, val, 0, 255) ;
      }
    }
    if (annotation_name)
    {
      mark = v->marked ;
      fprintf(stdout, "statistics for annotation %d (%d %d %d)\n", 
              v->marked,
              v->marked&0xff,(v->marked>>8)&0xff,(v->marked>>16)&0xff) ;
      MRISripVerticesWithoutMark(mris, mark) ;
      MRIScomputeMetricProperties(mris) ;



      MRIScopyCurvatureFromImagValues(mris) ;  /* restore thickness measures */
      MRIScomputeCurvatureStats(mris, &thickness_mean, &thickness_var,
                                ignore_below, ignore_above) ;

      fprintf(stdout, "total surface area                      = %2.0f mm^2\n",
              mris->total_area) ;
      
      gray_volume = MRISmeasureCorticalGrayMatterVolume(mris) ;
      fprintf(stdout, "total gray matter volume                = %2.0f mm^3\n",
              gray_volume) ;

      fprintf(stdout, 
            "average cortical thickness              = %2.3f mm +- %2.3f mm\n",
            thickness_mean, sqrt(thickness_var)) ;
      
      MRISuseMeanCurvature(mris) ;
      total_abs_mean_curvature = MRIScomputeAbsoluteCurvature(mris) ;
      fprintf(stdout, "integrated rectified mean curvature     = %2.3f\n", 
              total_abs_mean_curvature) ;
      MRISuseGaussianCurvature(mris) ;
      total_abs_gaussian_curvature = MRIScomputeAbsoluteCurvature(mris) ;
      fprintf(stdout, "integrated rectified Gaussian curvature = %2.3f\n", 
              total_abs_gaussian_curvature) ;
      MRIScomputeCurvatureIndices(mris, &ici, &fi) ;
      fprintf(stdout, "folding index                           = %2.3f\n", fi);
      fprintf(stdout, "intrinsic curvature index               = %2.3f\n",ici);
      MRISrestoreSurface(mris) ;
      MRISreplaceMarks(mris, mark, -1) ;
      MRISripVerticesWithMark(mris, -1) ;
    }
  }

  if (annotation_name == NULL)
  {
    MRIScopyCurvatureFromImagValues(mris) ;  /* restore thickness measures */
    MRIScomputeCurvatureStats(mris, &thickness_mean, &thickness_var,
                              ignore_below, ignore_above) ;
    
    fprintf(stdout, "total surface area                      = %2.0f mm^2\n", 
            mris->total_area) ;
    
    gray_volume = MRISmeasureCorticalGrayMatterVolume(mris) ;
    fprintf(stdout, "total gray matter volume                = %2.0f mm^3\n", 
            gray_volume) ;
    
    fprintf(stdout, 
            "average cortical thickness              = %2.3f mm +- %2.3f mm\n",
            thickness_mean, sqrt(thickness_var)) ;
    
    MRISuseMeanCurvature(mris) ;
    total_abs_mean_curvature = MRIScomputeAbsoluteCurvature(mris) ;
    fprintf(stdout, "integrated rectified mean curvature     = %2.3f\n", 
            total_abs_mean_curvature) ;
    MRISuseGaussianCurvature(mris) ;
    total_abs_gaussian_curvature = MRIScomputeAbsoluteCurvature(mris) ;
    fprintf(stdout, "integrated rectified Gaussian curvature = %2.3f\n", 
            total_abs_gaussian_curvature) ;
    MRIScomputeCurvatureIndices(mris, &ici, &fi) ;
    fprintf(stdout, "folding index                           = %2.3f\n", fi);
    fprintf(stdout, "intrinsic curvature index               = %2.3f\n", ici);
  }
  if (log_fp)
  {
		if (!noheader)
			fprintf(log_fp, "%% %s: <wm vol> <surf area> <gray vol> <thick mean> <thick var> <integ rect. mean curv> <integ rect. Gauss curv> <fold index> <intr curv ind>\n",sname) ;
    fprintf(log_fp, "%2.0f\t%2.0f\t%2.0f\t%2.3f\t%2.3f\t%2.3f\t%2.3f\t%2.3f\t%2.3f\n", 
            wm_volume,
            mris->total_area,
            gray_volume,
            thickness_mean, 
            sqrt(thickness_var), 
            total_abs_mean_curvature, 
            total_abs_gaussian_curvature, 
            fi, 
            ici) ;
    fclose(log_fp) ;
  }
  if (histo_flag)
  {
    fprintf(stderr, "plotting gray matter histogram to file %s...\n",
            gray_histo_name) ;
    HISTOplot(histo_gray, gray_histo_name) ;
    MRIfree(&mri_orig) ;
    HISTOfree(&histo_gray) ;
  }
  exit(0) ;
  return(0) ;  /* for ansi */
}

/*----------------------------------------------------------------------
            Parameters:

           Description:
----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[])
{
  int  nargs = 0 ;
  char *option ;
  
  option = argv[1] + 1 ;            /* past '-' */
  if (!stricmp(option, "-help"))
    print_help() ;
  else if (!stricmp(option, "-version"))
    print_version() ;
  else if (!stricmp(option, "log"))
  {
    log_file_name = argv[2] ;
    nargs = 1 ;
    fprintf(stderr, "outputting results to %s...\n", log_file_name) ;
  }
  else if (!stricmp(option, "noheader"))
  {
		noheader = 1 ;
    printf("supressing printing of headers to log file\n") ;
  }
  else switch (toupper(*option))
  {
  case 'T':
    thickness_name = argv[2] ;
    nargs = 1 ;
    fprintf(stderr, "using thickness file %s.\n", thickness_name) ;
    break ;
  case 'L':
    label_name = argv[2] ;
    nargs = 1 ;
    fprintf(stderr, "limiting computations to label %s.\n", label_name) ;
    break ;
  case 'M':
    mri_name = argv[2] ;
    nargs = 1 ;
    fprintf(stderr, "computing histograms on intensity values from %s...\n",
            mri_name) ;
    break ;
  case 'H':
    histo_flag = 1 ;
    gray_histo_name = argv[2] ;
    nargs = 1 ;
    fprintf(stderr,"writing histograms of intensity distributions to %s...\n",
            gray_histo_name);
    break ;
  case 'A':
    annotation_name = argv[2] ;
    nargs = 1 ;
    fprintf(stderr, "computing statistics for each annotation in %s.\n", 
            annotation_name) ;
    break ;
  case 'I':
    ignore_below = atof(argv[2]) ;
    ignore_above = atof(argv[3]) ;
    fprintf(stderr, 
            "only considering thicknesses in the range [%2.1f,%2.1f].\n", 
            ignore_below, ignore_above) ;
    nargs = 2 ;
    break ;
  case '?':
  case 'U':
    print_usage() ;
    exit(1) ;
    break ;
  default:
    fprintf(stderr, "unknown option %s\n", argv[1]) ;
    exit(1) ;
    break ;
  }

  return(nargs) ;
}

static void
usage_exit(void)
{
  print_help() ;
  exit(1) ;
}

static void
print_usage(void)
{
  fprintf(stderr, 
          "usage: %s [options] <subject name> <hemi> [<surface name>]\n", 
          Progname) ;
}

static void
print_help(void)
{
  print_usage() ;
  fprintf(stderr, 
          "\nThis program measures a variety of anatomical properties\n") ;
  fprintf(stderr, "\nvalid options are:\n\n") ;
  fprintf(stderr, 
          "-i  <low thresh> <hi thresh> - only consider thicknesses in\n"
          "                               the specified range.\n") ;
  fprintf(stderr,
          "-l <label file>              - limit calculations to specified "
          "label\n") ;
  fprintf(stderr,
          "-t <thickness file>          - use specified file for computing "
          "thickness statistics\n") ;
  fprintf(stderr,
          "-a <annotation file>         - compute properties for each label\n"
          "                               in the annotation file separately"
          "\n") ;
  exit(1) ;
}

static void
print_version(void)
{
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}

double
MRISmeasureTotalWhiteMatterVolume(MRI *mri)
{
  double  total_volume, voxel_volume ;
  int     x, y, z, width, height, depth ;
  BUFTYPE *psrc ;

  width = mri->width ; height = mri->height ; depth = mri->depth ; 
  voxel_volume = mri->xsize * mri->ysize * mri->zsize ;
  for (total_volume = 0.0, y = 0 ; y < height ; y++)
  {
    for (z = 0 ; z < depth ; z++)
    {
      psrc = &MRIvox(mri, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        if (*psrc++ > 10)
          total_volume += voxel_volume ;
      }
    }
  }
  return(total_volume) ;
}

int
MRIScomputeCurvatureStats(MRI_SURFACE *mris, double *pavg, double *pvar,
                          float ignore_below, float ignore_above)
{
  VERTEX    *v ;
  int       vno ;
  double    mean, var, n ;

  for (n = mean = 0.0, vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->curv < ignore_below || v->curv > ignore_above)
      continue ;
    mean += v->curv ;
    n += 1.0 ;
  }

  mean /= n ;
  for (var = 0.0, vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->curv < ignore_below || v->curv > ignore_above)
      continue ;
    var += (v->curv - mean) * (v->curv - mean) ;
  }

  var /= n ;
  *pavg = mean ; *pvar = var ;
  return(NO_ERROR) ;
}
double
MRISmeasureCorticalGrayMatterVolume(MRI_SURFACE *mris)
{
  VERTEX    *v ;
  int       vno ;
  double    mean, var, total, volume, n ;

  for (n = total = 0.0, vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    volume = (v->curv * v->area) ;
    total += volume ;
    n += 1.0 ;
  }

  mean = total / n ;
  for (var = 0.0, vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    volume = (v->curv * v->area) ;
    var += (volume - mean) * (volume - mean) ;
  }

  var /= n ;
  return(total) ;
}



double
MRIScomputeAbsoluteCurvature(MRI_SURFACE *mris)
{
  VERTEX    *v ;
  int       vno ;
  double    total, n ;

  for (n = total = 0.0, vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    total += (fabs(v->curv) * v->area) ;
    n += 1.0 ;
  }

  return(total) ;
}

int
MRISreadAnnotFile(MRI_SURFACE *mris, char *name)
{
  int    i,j,vno,num;
  FILE   *fp;
  char   fname[100] ;

  MRISbuildFileName(mris, name, fname) ;
  fp = fopen(fname,"r");
  if (fp==NULL) 
  ErrorReturn(ERROR_NOFILE, 
              (ERROR_NOFILE, "Annotion file %s not found",fname)); 
  for (vno=0;vno<mris->nvertices;vno++)
    mris->vertices[vno].marked = -1 ;
  num = freadInt(fp);
  /*  printf("num=%d\n",num);*/
  for (j=0;j<num;j++)
  {
    vno = freadInt(fp);
    i = freadInt(fp);
    if (vno>=mris->nvertices||vno<0)
      printf("vertex index out of range: %d i=%d\n",vno,i);
    else
      mris->vertices[vno].marked = i;
  }
  fclose(fp);
  return(NO_ERROR) ;
}
int
MRISripVerticesWithMark(MRI_SURFACE *mris, int mark)
{
  int      vno ;
  VERTEX   *v ;
  
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->marked == mark)
      v->ripflag = 1 ;
  }
  MRISripFaces(mris) ;
  return(NO_ERROR) ;
}
int
MRISripVerticesWithoutMark(MRI_SURFACE *mris, int mark)
{
  int      vno ;
  VERTEX   *v ;
  
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->marked != mark)
      v->ripflag = 1 ;
  }
  MRISripFaces(mris) ;
  return(NO_ERROR) ;
}

int
MRISrestoreSurface(MRI_SURFACE *mris)
{
  int      vno, fno ;
  VERTEX   *v ;
  FACE     *f ;
  
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    v->ripflag = 0 ;
  }
  for (fno = 0 ; fno < mris->nfaces ; fno++)
  {
    f = &mris->faces[fno] ;
    f->ripflag = 0 ;
  }
  return(NO_ERROR) ;
}

int
MRISreplaceMarks(MRI_SURFACE *mris, int in_mark, int out_mark)
{
  int      vno ;
  VERTEX   *v ;
  
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->marked == in_mark)
      v->marked = out_mark ;
  }
  return(NO_ERROR) ;
}

