#include <ctime>
#include <cstdlib>
#include <libplugin/plugin.h>
#include <psi4-dec.h>
#include <libparallel/parallel.h>
#include <liboptions/liboptions.h>
#include <libmints/mints.h>
#include <libpsio/psio.hpp>
#include <libpsio/psio.h>
#include <libciomr/libciomr.h>
#include <libdpd/dpd.h>
#include <libdpd/dpd.gbl>
#include <psifiles.h>

INIT_PLUGIN

using namespace boost;

namespace psi{ namespace dpd_unit_test {

int **cacheprep_rhf(int level, int *cachefiles);
void cachedone_rhf(int **cachelist);
double dpd_rand(int places);
void dpd_buf4_fill(dpdbuf4 *);
bool dpd_buf4_compare(dpdbuf4 *X, dpdbuf4 *Y, double cutoff);

extern "C" 
int read_options(std::string name, Options& options)
{
    if (name == "DPD_UNIT_TEST"|| options.read_globals()) {
        /*- The amount of information printed to the output file -*/
        options.add_int("PRINT", 1);
    }

    return true;
}

extern "C" 
PsiReturnType psi4_libdpd_unit_test(Options& options)
{
    int print = options.get_int("PRINT");
    boost::shared_ptr<PSIO> psio(new PSIO);

    for(int i =PSIF_CC_MIN; i <= PSIF_CC_MAX; i++) psio_open(i,1);

    // Set up lots of look-ups needed for DPD functionality
    int **cachelist, *cachefiles;
    cachefiles = init_int_array(PSIO_MAXUNIT);
    cachelist = cacheprep_rhf(0, cachefiles);

    int nirreps = 4;
    int nocc = 15;
    int nvirt = 65;
    int nmo = nocc + nvirt;
    int *occpi = new int[nirreps];
    occpi[0] = 6; occpi[1] = 2; occpi[2] = 3; occpi[3] = 4;
    int *virtpi = new int[nirreps];
    virtpi[0] = 35; virtpi[1] = 7; virtpi[2] = 11; virtpi[3] = 12;
    int *occsym = new int[nocc];
    for(int h=0,offset=0; h < nirreps; h++) {
      for(int i=offset; i < offset + occpi[h]; h++)
        occsym[i] = h;
      offset += occpi[h];
    }
    int *virsym = new int[nvirt];
    for(int h=0,offset=0; h < nirreps; h++) {
      for(int i=offset; i < offset + virtpi[h]; h++)
        virsym[i] = h;
      offset += virtpi[h];
    }

    // See random-number generator
    srand(time(0));

    // Initialize the library
    dpd_init(0, nirreps, 256000000, 0, cachefiles, cachelist, NULL, 2, 
             occpi, occsym, virtpi, virsym);

    // Testing dpd_contract444() out-of-core algorithm #2
    //  alpha * X(ab,ij) * Y(ij,cd) --> Z(ab,cd) + beta * Z(ab,cd)
    dpdbuf4 X, Y, Z;
    dpd_buf4_init(&X, PSIF_CC_TMP0, 0, 5, 0, 5, 0, 0, "X(ab,ij)");
    dpd_buf4_fill(&X);
    dpd_buf4_close(&X);

    dpd_buf4_init(&Y, PSIF_CC_TMP0, 0, 0, 5, 0, 5, 0, "Y(ij,ab)");
    dpd_buf4_fill(&Y);
    dpd_buf4_close(&Y);

    // Run in core
    dpd_buf4_init(&Z, PSIF_CC_TMP0, 0, 5, 5, 5, 5, 0, "Z(ab,cd)");
    dpd_buf4_init(&Y, PSIF_CC_TMP0, 0, 0, 5, 0, 5, 0, "Y(ij,ab)");
    dpd_buf4_init(&X, PSIF_CC_TMP0, 0, 5, 0, 5, 0, 0, "X(ab,ij)");
    dpd_contract444(&X, &Y, &Z, 0, 1, 2, 0);
    dpd_buf4_close(&X);
    dpd_buf4_close(&Y);
    dpd_buf4_close(&Z);

    // Force out-of-core
    dpd_memset(750000);
    dpd_buf4_init(&Z, PSIF_CC_TMP0, 0, 5, 5, 5, 5, 0, "Z(ab,cd) ooc");
    dpd_buf4_init(&Y, PSIF_CC_TMP0, 0, 0, 5, 0, 5, 0, "Y(ij,ab)");
    dpd_buf4_init(&X, PSIF_CC_TMP0, 0, 5, 0, 5, 0, 0, "X(ab,ij)");
    dpd_contract444(&X, &Y, &Z, 0, 1, 2, 0);
    dpd_buf4_close(&X);
    dpd_buf4_close(&Z);
    dpd_memset(256000000);

    // Compare results
    dpdbuf4 Z1, Z2;
    dpd_buf4_init(&Z1, PSIF_CC_TMP0, 0, 5, 5, 5, 5, 0, "Z(ab,cd)");
    dpd_buf4_init(&Z2, PSIF_CC_TMP0, 0, 5, 5, 5, 5, 0, "Z(ab,cd) ooc");
    if(!dpd_buf4_compare(&Z1, &Z2, 1e-6))
      fprintf(outfile, "\nResulting Z(ab,cd) matrices do not match from contract444.\n");
    else
      fprintf(outfile, "\nResulting Z(ab,cd) matrices from contract444 match!\n");
    dpd_buf4_close(&Z1);
    dpd_buf4_close(&Z2);


    // Testing out-of-core psrq sort
    dpd_buf4_init(&X, PSIF_CC_TMP0, 0, 5, 0, 5, 0, 0, "X(ab,ij)");
    dpd_buf4_fill(&X);
    // In-core
    dpd_buf4_sort(&X, PSIF_CC_TMP0, psrq, 11, 10, "X(aj,ib)");

    // Out-of-core
    dpd_memset(750000);
    dpd_buf4_sort(&X, PSIF_CC_TMP0, psrq, 11, 10, "X(aj,ib)");
    dpd_memset(256000000);

    // Shut down the library
    dpd_close(0);
    cachedone_rhf(cachelist);
    delete [] occpi;
    delete [] virtpi;
    delete [] occsym;
    delete [] virsym;

    for(int i=PSIF_CC_TMP; i <= PSIF_CC_TMP11; i++) psio_close(i,0);

    return Success;
}

// Generate random numbers (seed in caller)
double dpd_rand(int places)  // arg = # decimal places
{
  long prec = (long) pow(10.0,(double) places); 
  double i = ((long) rand()) % (40l*prec) - (20l*prec);
  return i / (100l*prec);
}

// Fill a given DPD buffer with random numbers (seed in caller)
void dpd_buf4_fill(dpdbuf4 *X)
{
  for(int h=0; h < X->params->nirreps; h++) {
    dpd_buf4_mat_irrep_init(X, h);
    for(int p=0; p < X->params->rowtot[h]; p++)
      for(int q=0; q < X->params->coltot[h]; q++)
        X->matrix[h][p][q] = dpd_rand(7);
    dpd_buf4_mat_irrep_wrt(X, h);
    dpd_buf4_mat_irrep_close(X, h);
  }
}

// Compare two buffers element-by-element to within the specified cutoff
bool dpd_buf4_compare(dpdbuf4 *X, dpdbuf4 *Y, double cutoff)
{
  bool match = true;

  for(int h=0; h < X->params->nirreps; h++) {
    dpd_buf4_mat_irrep_init(X, h);
    dpd_buf4_mat_irrep_rd(X, h);
    dpd_buf4_mat_irrep_init(Y, h);
    dpd_buf4_mat_irrep_rd(Y, h);
    for(int p=0; p < X->params->rowtot[h]; p++)
      for(int q=0; q < X->params->coltot[h]; q++)
        if(fabs(X->matrix[h][p][q] - Y->matrix[h][p][q]) > cutoff) match = false;
    dpd_buf4_mat_irrep_close(X, h);
    dpd_buf4_mat_irrep_close(Y, h);
  }
  return match;
}

}} // End namespaces

