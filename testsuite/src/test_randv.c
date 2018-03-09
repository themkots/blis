/*

   BLIS    
   An object-based framework for developing high-performance BLAS-like
   libraries.

   Copyright (C) 2014, The University of Texas at Austin

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    - Neither the name of The University of Texas at Austin nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "blis.h"
#include "test_libblis.h"


// Static variables.
static char*     op_str                    = "randv";
static char*     o_types                   = "v";  // x
static char*     p_types                   = "";   // (no parameters)
static thresh_t  thresh[BLIS_NUM_FP_TYPES] = { { 1e-04, 1e-05 },   // warn, pass for s
                                               { 1e-04, 1e-05 },   // warn, pass for c
                                               { 1e-13, 1e-14 },   // warn, pass for d
                                               { 1e-13, 1e-14 } }; // warn, pass for z

// Local prototypes.
void libblis_test_randv_deps
     (
       test_params_t* params,
       test_op_t*     op
     );

void libblis_test_randv_experiment
     (
       test_params_t* params,
       test_op_t*     op,
       iface_t        iface,
       num_t          datatype,
       char*          pc_str,
       char*          sc_str,
       unsigned int   p_cur,
       double*        perf,
       double*        resid
     );

void libblis_test_randv_impl
     (
       iface_t   iface,
       obj_t*    x
     );

void libblis_test_randv_check
     (
       test_params_t* params,
       obj_t*         x,
       double*        resid
     );



void libblis_test_randv_deps
     (
       test_params_t* params,
       test_op_t*     op
     )
{
	// No dependencies.
}



void libblis_test_randv
     (
       test_params_t* params,
       test_op_t*     op
     )
{

	// Return early if this test has already been done.
	if ( op->test_done == TRUE ) return;

	// Return early if operation is disabled.
	if ( libblis_test_op_is_disabled( op ) ||
	     op->ops->util_over == DISABLE_ALL ) return;

	// Call dependencies first.
	if ( TRUE ) libblis_test_randv_deps( params, op );

	// Execute the test driver for each implementation requested.
	if ( op->front_seq == ENABLE )
	{
		libblis_test_op_driver( params,
		                        op,
		                        BLIS_TEST_SEQ_FRONT_END,
		                        op_str,
		                        p_types,
		                        o_types,
		                        thresh,
		                        libblis_test_randv_experiment );
	}
}



void libblis_test_randv_experiment
     (
       test_params_t* params,
       test_op_t*     op,
       iface_t        iface,
       num_t          datatype,
       char*          pc_str,
       char*          sc_str,
       unsigned int   p_cur,
       double*        perf,
       double*        resid
     )
{
	unsigned int n_repeats = params->n_repeats;
	unsigned int i;

	double       time_min  = DBL_MAX;
	double       time;

	dim_t        m;

	char         x_store;

	obj_t        x;


	// Map the dimension specifier to an actual dimension.
	m = libblis_test_get_dim_from_prob_size( op->dim_spec[0], p_cur );

	// Map parameter characters to BLIS constants.


	// Extract the storage character for each operand.
	x_store = sc_str[0];

	// Create the test objects.
	libblis_test_vobj_create( params, datatype, x_store, m, &x );

	// Repeat the experiment n_repeats times and record results. 
	for ( i = 0; i < n_repeats; ++i )
	{
		time = bli_clock();

		libblis_test_randv_impl( iface, &x );

		time_min = bli_clock_min_diff( time_min, time );
	}

	// Estimate the performance of the best experiment repeat.
	*perf = ( 2.0 * m ) / time_min / FLOPS_PER_UNIT_PERF;
	if ( bli_obj_is_complex( x ) ) *perf *= 2.0;

	// Perform checks.
	// For randv(), we don't return a meaningful residual/diff, since we can't
	// really say for sure what is "random" and what is not, so instead we
	// manually perform some checks that will fail under some scenarios whic
	// we consider to be likely.
	libblis_test_randv_check( params, &x, resid );

	// Zero out performance and residual if output vector is empty.
	libblis_test_check_empty_problem( &x, perf, resid );

	// Free the test objects.
	bli_obj_free( &x );
}



void libblis_test_randv_impl
     (
       iface_t   iface,
       obj_t*    x
     )
{
	switch ( iface )
	{
		case BLIS_TEST_SEQ_FRONT_END:
		bli_randv( x );
		break;

		default:
		libblis_test_printf_error( "Invalid interface type.\n" );
	}
}



void libblis_test_randv_check
     (
       test_params_t* params,
       obj_t*         x,
       double*        resid
     )
{
	num_t  dt_real = bli_obj_datatype_proj_to_real( *x );
	dim_t  m_x     = bli_obj_vector_dim( *x );
	obj_t  sum;

	*resid = 0.0;

	//
	// The two most likely ways that randv would fail is if all elements
	// were zero, or if all elements were greater than or equal to one.
	// We check both of these conditions by computing the sum of the
	// absolute values of the elements of x.
	//

	bli_obj_scalar_init_detached( dt_real, &sum );

	bli_norm1v( x, &sum );

	if ( bli_is_float( dt_real ) )
	{
		float*  sum_x = bli_obj_buffer_at_off( sum );

		if      ( *sum_x == *bli_d0   ) *resid = 1.0;
		else if ( *sum_x >= 2.0 * m_x ) *resid = 2.0;
	}
	else // if ( bli_is_double( dt_real ) )
	{
		double* sum_x = bli_obj_buffer_at_off( sum );

		if      ( *sum_x == *bli_d0   ) *resid = 1.0;
		else if ( *sum_x >= 2.0 * m_x ) *resid = 2.0;
	}
}

