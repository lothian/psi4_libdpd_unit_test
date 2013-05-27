import psi4
import re
import os
import inputparser
import math
import warnings
from driver import *
from wrappers import *
from molutil import *
import p4util
from psiexceptions import *


def run_dpd_unit_test(name, **kwargs):
    r"""Function encoding sequence of PSI module and plugin calls so that
    dpd_unit_test can be called via :py:func:`~driver.energy`. For post-scf plugins.

    >>> energy('dpd_unit_test')

    """
    lowername = name.lower()
    kwargs = p4util.kwargs_lower(kwargs)

    # Your plugin's psi4 run sequence goes here
    psi4.set_global_option('BASIS', 'sto-3g')
    psi4.set_local_option('DPD_UNIT_TEST', 'PRINT', 1)
    scf_helper(name, **kwargs)
    returnvalue = psi4.plugin('dpd_unit_test.so')
    psi4.set_variable('CURRENT ENERGY', returnvalue)


# Integration with driver routines
procedures['energy']['dpd_unit_test'] = run_dpd_unit_test


def exampleFN():
    # Your Python code goes here
    pass
