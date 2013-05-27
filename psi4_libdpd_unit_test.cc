#include <ctime>
#include <cstdlib>
#include <libplugin/plugin.h>
#include <psi4-dec.h>
#include <libparallel/parallel.h>
#include <liboptions/liboptions.h>
#include <libmints/mints.h>
#include <libpsio/psio.hpp>
#include <libciomr/libciomr.h>
#include <libdpd/dpd.h>
#include <libdpd/dpd.gbl>
#include <psifiles.h>

INIT_PLUGIN

using namespace boost;

namespace psi{ namespace dpd_unit_test {

int **cacheprep_rhf(int level, int *cachefiles);
void cachedone_rhf(int **cachelist);
double dpd_rand(void);

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

    // Initialize the library
    dpd_init(0, nirreps, 256000000, 0, cachefiles, cachelist, NULL, 2, 
             occpi, occsym, virtpi, virsym);

    dpdbuf4 X;
    dpd_buf4_init(&X, PSIF_CC_TMP0, 0, 0, 5, 0, 5, 0, "X(ij,ab)");
    dpd_buf4_print(&X, outfile, 0);
    dpd_buf4_close(&X);

    // Test random-number generation
    srand(time(0));
    for(int j=0; j < 30; j++) {
      fprintf(outfile, "d = %20.12f\n", dpd_rand());
    }

    // Shut down the library
    dpd_close(0);
    cachedone_rhf(cachelist);
    delete [] occpi;
    delete [] virtpi;
    delete [] occsym;
    delete [] virsym;

    return Success;
}

double dpd_rand()  // must pre-seed
{
  long prec = (long) pow(10.0,7.0);
  double i = ((long) rand()) % (40l*prec) - (20l*prec);
  return i / (100l*prec);
}

}} // End namespaces

