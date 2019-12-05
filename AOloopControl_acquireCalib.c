/**
 * @file    AOloopControl_acquireCalib.c
 * @brief   Adaptive Optics Control loop engine acquire calibration
 * 
 * Acquire AO system calibration assuming linear response
 *  
 * 
 * @bug No known bugs.
 * 
 * 
 */



#define _GNU_SOURCE



#define AOLOOPCONTROL_ACQUIRECALIB_LOGDEBUG 1

#if defined(AOLOOPCONTROL_ACQUIRECALIB_LOGDEBUG) && !defined(STANDALONE)
#define TESTPOINT(...) do { \
sprintf(data.testpoint_file, "%s", __FILE__); \
sprintf(data.testpoint_func, "%s", __func__); \
data.testpoint_line = __LINE__; \
sprintf(data.testpoint_msg, __VA_ARGS__); \
} while(0)
#else
#define TESTPOINT(...)
#endif




/* =============================================================================================== */
/* =============================================================================================== */
/*                                        HEADER FILES                                             */
/* =============================================================================================== */
/* =============================================================================================== */


#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>

#ifdef __MACH__
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
int clock_gettime(int clk_id, struct mach_timespec *t) {
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    uint64_t time;
    time = mach_absolute_time();
    double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
    double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
    t->tv_sec = seconds;
    t->tv_nsec = nseconds;
    return 0;
}
#else
#include <time.h>
#endif



#include <fitsio.h>

#include "CommandLineInterface/CLIcore.h"
#include "00CORE/00CORE.h"
#include "COREMOD_memory/COREMOD_memory.h"
#include "COREMOD_iofits/COREMOD_iofits.h"
#include "COREMOD_tools/COREMOD_tools.h"
#include "COREMOD_arith/COREMOD_arith.h"
#include "info/info.h"

#include "statistic/statistic.h"
#include "image_gen/image_gen.c"

#include "AOloopControl/AOloopControl.h"
#include "AOloopControl_IOtools/AOloopControl_IOtools.h"
#include "AOloopControl_acquireCalib/AOloopControl_acquireCalib.h"
#include "AOloopControl_computeCalib/AOloopControl_computeCalib.h"
#include "AOloopControl_compTools/AOloopControl_compTools.h"

/* =============================================================================================== */
/* =============================================================================================== */
/*                                      DEFINES, MACROS                                            */
/* =============================================================================================== */
/* =============================================================================================== */



# ifdef _OPENMP
# include <omp.h>
#define OMP_NELEMENT_LIMIT 1000000
# endif




/* =============================================================================================== */
/* =============================================================================================== */
/*                                  GLOBAL DATA DECLARATION                                        */
/* =============================================================================================== */
/* =============================================================================================== */

//extern long aoloopcontrol_var.aoconfID_dmRM;
//extern long aoloopcontrol_var.aoconfID_wfsim;
//extern long aoloopcontrol_var.aoconfID_imWFS0;
//extern long aoloopcontrol_var.aoconfID_imWFS1;
//extern long aoloopcontrol_var.aoconfID_imWFS1;
//extern long aoloopcontrol_var.aoconfID_cmd_modesRM;

static int RMACQUISITION = 0;  // toggles to 1 when resp matrix is being acquired
static int INITSTATUS_AOloopControl_acquireCalib = 0;

extern long LOOPNUMBER; // current loop index
extern int AOloopcontrol_meminit; // declared in AOloopControl_compTools.c


/* =============================================================================================== */
/*                                     MAIN DATA STRUCTURES                                        */
/* =============================================================================================== */

extern AOLOOPCONTROL_CONF *AOconf; // declared in AOloopControl.c
extern AOloopControl_var aoloopcontrol_var;









// CLI commands
//
// function CLI_checkarg used to check arguments
// CLI_checkarg ( CLI argument index , type code )
//
// type codes:
// 1: float
// 2: long
// 3: string, not existing image
// 4: existing image
// 5: string
//



/* =============================================================================================== */
/* =============================================================================================== */
/** @name AOloopControl_acquireCalib - 1. ACQUIRING CALIBRATION
 *  Measure system response */
/* =============================================================================================== */
/* =============================================================================================== */

int_fast8_t AOloopControl_acquireCalib_mkRandomLinPokeSequence_cli() {
    if(CLI_checkarg(1,4)
            + CLI_checkarg(2,2)
            + CLI_checkarg(3,5)
            + CLI_checkarg(4,5)==0) {
        AOloopControl_acquireCalib_mkRandomLinPokeSequence(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.numl,
            data.cmdargtoken[3].val.string,
            data.cmdargtoken[4].val.string);
        return 0;
    }
    else return 1;
}



/** @brief CLI function for AOloopControl_RespMatrix_Fast */
int_fast8_t AOloopControl_acquireCalib_RespMatrix_Fast_cli() {
    if(CLI_checkarg(1,4)
            + CLI_checkarg(2,4)
            + CLI_checkarg(3,4)
            + CLI_checkarg(4,2)
            + CLI_checkarg(5,1)
            + CLI_checkarg(6,1)
            + CLI_checkarg(7,1)
            + CLI_checkarg(8,3)==0) {
        AOloopControl_acquireCalib_RespMatrix_Fast(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string,
            data.cmdargtoken[3].val.string,
            data.cmdargtoken[4].val.numl,
            data.cmdargtoken[5].val.numf,
            data.cmdargtoken[6].val.numf,
            data.cmdargtoken[7].val.numf,
            data.cmdargtoken[8].val.string);
        return 0;
    }
    else return 1;
}


/** @brief CLI function for AOloopControl_Measure_WFSrespC */
int_fast8_t AOloopControl_acquireCalib_Measure_WFSrespC_cli() {
    if(CLI_checkarg(1,2)
            + CLI_checkarg(2,2)
            + CLI_checkarg(3,2)
            + CLI_checkarg(4,2)
            + CLI_checkarg(5,4)
            + CLI_checkarg(6,5)
            + CLI_checkarg(7,2)
            + CLI_checkarg(8,2)
            + CLI_checkarg(9,2)
            + CLI_checkarg(10,2)==0) {
        AOloopControl_acquireCalib_Measure_WFSrespC(LOOPNUMBER,
                data.cmdargtoken[1].val.numl,
                data.cmdargtoken[2].val.numl,
                data.cmdargtoken[3].val.numl,
                data.cmdargtoken[4].val.numl,
                data.cmdargtoken[5].val.string,
                data.cmdargtoken[6].val.string,
                data.cmdargtoken[7].val.numl,
                data.cmdargtoken[8].val.numl,
                data.cmdargtoken[9].val.numl,
                (uint32_t) data.cmdargtoken[10].val.numl);
        return 0;
    }
    else return 1;
}



/** @brief CLI function for AOloopControl_Measure_WFS_linResponse */

int_fast8_t AOloopControl_acquireCalib_Measure_WFS_linResponse_cli() {
    int stringmaxlen = 500;
    char fpsname[stringmaxlen];

    // First, we try to execute function through FPS interface
    if(CLI_checkarg(1, 5) == 0) { // check that first arg is string
        // unsigned int OptionalArg00 = data.cmdargtoken[2].val.numl;
        // Set FPS interface name
        // By convention, if there are optional arguments, they should be appended to the fps name
        //
        if(data.processnameflag == 0) { // name fps to something different than the process name
            if(strlen(data.cmdargtoken[2].val.string)>0)
                snprintf(fpsname, stringmaxlen, "measlinRM-%s", data.cmdargtoken[2].val.string);
            else
                snprintf(fpsname, stringmaxlen, "measlinRM");
        } else { // Automatically set fps name to be process name up to first instance of character '.'
            strcpy(fpsname, data.processname0);
        }
        if(strcmp(data.cmdargtoken[1].val.string, "_FPSINIT_") == 0) {  // Initialize FPS
            AOcontrolLoop_acquireCalib_Measure_WFS_linResponse_FPCONF(fpsname, CMDCODE_FPSINIT);
            return RETURN_SUCCESS;
        }
        if(strcmp(data.cmdargtoken[1].val.string, "_CONFSTART_") == 0) {  // Start conf process
            AOcontrolLoop_acquireCalib_Measure_WFS_linResponse_FPCONF(fpsname, CMDCODE_CONFSTART);
            return RETURN_SUCCESS;
        }
        if(strcmp(data.cmdargtoken[1].val.string, "_CONFSTOP_") == 0) { // Stop conf process
            AOcontrolLoop_acquireCalib_Measure_WFS_linResponse_FPCONF(fpsname, CMDCODE_CONFSTOP);
            return RETURN_SUCCESS;
        }
        if(strcmp(data.cmdargtoken[1].val.string, "_RUNSTART_") == 0) { // Run process
            AOcontrolLoop_acquireCalib_Measure_WFS_linResponse_RUN(fpsname);
            return RETURN_SUCCESS;
        }
        if(strcmp(data.cmdargtoken[1].val.string, "_RUNSTOP_") == 0) { // Stop process
            //     AOcontrolLoop_acquireCalib_Measure_WFS_linResponse_STOP(OptionalArg00);
            return RETURN_SUCCESS;
        }
    }
    // non FPS implementation - all parameters specified at function launch
    if( CLI_checkarg(1,1)
            + CLI_checkarg(2,2)
            + CLI_checkarg(3,2)
            + CLI_checkarg(4,2)
            + CLI_checkarg(5,2)
            + CLI_checkarg(6,4)
            + CLI_checkarg(7,5)
            + CLI_checkarg(8,5)
            + CLI_checkarg(9,2)
            + CLI_checkarg(10,2)
            + CLI_checkarg(11,2)
            + CLI_checkarg(12,2)==0) {
        AOloopControl_acquireCalib_Measure_WFS_linResponse(LOOPNUMBER,
                data.cmdargtoken[1].val.numf,
                data.cmdargtoken[2].val.numl,
                data.cmdargtoken[3].val.numl,
                data.cmdargtoken[4].val.numl,
                data.cmdargtoken[5].val.numl,
                data.cmdargtoken[6].val.string,
                data.cmdargtoken[7].val.string,
                data.cmdargtoken[8].val.string,
                data.cmdargtoken[9].val.numl,
                data.cmdargtoken[10].val.numl,
                data.cmdargtoken[11].val.numl,
                data.cmdargtoken[12].val.numl);
        return RETURN_SUCCESS;
    } else {
        return RETURN_FAILURE;
    }
}




/** @brief CLI function for AOloopControl_Measure_zonalRM */
int_fast8_t AOloopControl_acquireCalib_Measure_zonalRM_cli() {
    if(CLI_checkarg(1,1)
            + CLI_checkarg(2,2)
            + CLI_checkarg(3,2)
            + CLI_checkarg(4,2)
            + CLI_checkarg(5,2)
            + CLI_checkarg(6,3)
            + CLI_checkarg(7,3)
            + CLI_checkarg(8,3)
            + CLI_checkarg(9,3)
            + CLI_checkarg(10,2)
            + CLI_checkarg(11,2)
            + CLI_checkarg(12,2)
            + CLI_checkarg(13,2)==0) {
        AOloopControl_acquireCalib_Measure_zonalRM(LOOPNUMBER,
                data.cmdargtoken[1].val.numf,
                data.cmdargtoken[2].val.numl,
                data.cmdargtoken[3].val.numl,
                data.cmdargtoken[4].val.numl,
                data.cmdargtoken[5].val.numl,
                data.cmdargtoken[6].val.string,
                data.cmdargtoken[7].val.string,
                data.cmdargtoken[8].val.string,
                data.cmdargtoken[9].val.string,
                data.cmdargtoken[10].val.numl,
                data.cmdargtoken[11].val.numl,
                data.cmdargtoken[12].val.numl,
                data.cmdargtoken[13].val.numl);
        return 0;
    }
    else return 1;
}


/** @brief CLI function for AOloopControl_Measure_Resp_Matrix */
int_fast8_t AOloopControl_acquireCalib_Measure_Resp_Matrix_cli() {
    if(CLI_checkarg(1,2)
            + CLI_checkarg(2,1)
            + CLI_checkarg(3,2)
            + CLI_checkarg(4,2)
            + CLI_checkarg(5,2)==0) {
        AOloopControl_acquireCalib_Measure_Resp_Matrix(LOOPNUMBER,
                data.cmdargtoken[1].val.numl,
                data.cmdargtoken[2].val.numf,
                data.cmdargtoken[3].val.numl,
                data.cmdargtoken[4].val.numl,
                data.cmdargtoken[5].val.numl);
        return 0;
    }
    else return 1;
}



int_fast8_t AOloopControl_acquireCalib_RMseries_deinterlace_cli() {
    if(CLI_checkarg(1,2)
            + CLI_checkarg(2,2)
            + CLI_checkarg(3,2)
            + CLI_checkarg(4,3)
            + CLI_checkarg(5,2)
            + CLI_checkarg(6,2)==0) {
        AOloopControl_acquireCalib_RMseries_deinterlace(
            data.cmdargtoken[1].val.numl,
            data.cmdargtoken[2].val.numl,
            data.cmdargtoken[3].val.numl,
            data.cmdargtoken[4].val.string,
            data.cmdargtoken[5].val.numl,
            data.cmdargtoken[6].val.numl);
        return 0;
    }
    else return 1;
}















/* =============================================================================================== */
/* =============================================================================================== */
/*                                    FUNCTIONS SOURCE CODE                                        */
/* =============================================================================================== */
/* =============================================================================================== */
/** @name AOloopControl_IOtools functions */

void __attribute__ ((constructor)) libinit_AOloopControl_acquireCalib()
{
    if( INITSTATUS_AOloopControl_acquireCalib == 0)
    {
        init_AOloopControl_acquireCalib();
        RegisterModule(__FILE__, "cacao", "AO loop control acquire calibration");
        INITSTATUS_AOloopControl_acquireCalib = 1;
    }
}





int_fast8_t init_AOloopControl_acquireCalib()
{

    /* =============================================================================================== */
    /* =============================================================================================== */
    /** @name AOloopControl_acquireCalib - 1. ACQUIRING CALIBRATION                                                 */
    /* =============================================================================================== */
    /* =============================================================================================== */

    RegisterCLIcommand(
        "aolacqmkrlps",
        __FILE__,
        AOloopControl_acquireCalib_mkRandomLinPokeSequence_cli,
        "create sequence of linearly coupled poke maps",
        "<poke modes cube> <NBpokemaps> <output poke map> <output poke frames>",
        "aolacqmkrlps pokeModesC 50 pokemC pokeC",
        "long AOloopControl_acquireCalib_mkRandomLinPokeSequence(char *IDmodeC_name, long NBpokemap, char *IDpokemapC_name, char *IDpokeC_name)");


    RegisterCLIcommand(
        "aolacqresp",
        __FILE__,
        AOloopControl_acquireCalib_Measure_Resp_Matrix_cli,
        "acquire AO response matrix and WFS reference",
        "<ave# [long]> <ampl [float]> <nbloop [long]> <frameDelay [long]> <NBiter [long]>",
        "aolacqresp 50 0.1 5 2",
        "int AOloopControl_acquireCalib_ Measure_Resp_Matrix(long loop, long NbAve, float amp, long nbloop, long fDelay, long NBiter)");

    RegisterCLIcommand(
        "aolmRMfast",
        __FILE__,
        AOloopControl_acquireCalib_RespMatrix_Fast_cli,
        "acquire fast modal response matrix",
        "<modes> <dm RM stream> <WFS stream> <sem trigger> <hardware latency [s]> <loop frequ [Hz]> <ampl [um]> <outname>",
        "aolmRMfast DMmodes aol0_dmRM aol0_wfsim 4 0.00112 2000.0 0.03 rm000",
        "long AOloopControl_acquireCalib_RespMatrix_Fast(char *DMmodes_name, char *dmRM_name, char *imWFS_name, long semtrig, float HardwareLag, float loopfrequ, float ampl, char *outname)");

    RegisterCLIcommand(
        "aolmeasWFSrespC",
        __FILE__,
        AOloopControl_acquireCalib_Measure_WFSrespC_cli,
        "measure WFS resp to DM patterns",
        "<delay frames [long]> <DMcommand delay us [long]> <nb frames per position [long]> <nb frames excluded [long]> <input DM patter cube [string]> <output response [string]> <normalize flag> <AOinitMode> <NBcycle>",
        "aolmeasWFSrespC 2 135 20 0 dmmodes wfsresp 1 0 5",
        "long AOloopControl_acquireCalib_Measure_WFSrespC(long loop, long delayfr, long delayRM1us, long NBave, long NBexcl, char *IDpokeC_name, char *IDoutC_name, int normalize, int AOinitMode, long NBcycle, uint32_t SequInitMode);");

    RegisterCLIcommand(
        "aolmeaslWFSrespC",
        __FILE__,
        AOloopControl_acquireCalib_Measure_WFS_linResponse_cli,
        "measure linear WFS response to DM patterns",
        "<ampl [um]> <delay frames [long]> <DMcommand delay us [long]> <nb frames per position [long]> <nb frames excluded [long]> <input DM patter cube [string]> <output response [string]> <output reference [string]> <normalize flag> <AOinitMode> <NBcycle>",
        "aolmeasWFSrespC 0.05 2 135 20 0 dmmodes wfsresp wfsref 1 0 5",
        "long AOloopControl_acquireCalib_Measure_WFS_linResponse(long loop, float ampl, long delayfr, long delayRM1us, long NBave, long NBexcl, char *IDpokeC_name, char *IDrespC_name, char *IDwfsref_name, int normalize, int AOinitMode, long NBcycle, long NBinnerCycle)");

    RegisterCLIcommand(
        "aolmeaszrm",
        __FILE__, AOloopControl_acquireCalib_Measure_zonalRM_cli,
        "measure zonal resp mat, WFS ref, DM and WFS response maps",
        "<ampl [float]> <delay frames [long]> <DMcommand delay us [long]> <nb frames per position [long]> <nb frames excluded [long]> <output image [string]> <output WFS ref [string]>  <output WFS response map [string]>  <output DM response map [string]> <mode> <normalize flag> <AOinitMode> <NBcycle>",
        "aolmeaszrm 0.05 2 135 20 zrm wfsref wfsmap dmmap 1 0 0 0",
        "long AOloopControl_acquireCalib_Measure_zonalRM(long loop, double ampl, long delayfr, long delayRM1us, long NBave, long NBexcl, char *zrespm_name, char *WFSref_name, char *WFSmap_name, char *DMmap_name, long mode, int normalize, int AOinitMode, long NBcycle)");


    RegisterCLIcommand(
        "aolRMdeinterlace",
        __FILE__,
        AOloopControl_acquireCalib_RMseries_deinterlace_cli,
        "process overlapping RMs",
        "aolRMdeinterlace <Number RMs> <refstart> <refend> <outname>",
        "aolRMdeinterlace 8 4 9 outRMseq",
        "long AOloopControl_acquireCalib_RMseries_deinterlace(int NBRM, int refstart, int refend, char *IDout_name)");



    // add atexit functions here
    // atexit((void*) myfunc);

    return 0;
}












/* =============================================================================================== */
/* =============================================================================================== */
/** @name AOloopControl - 4. ACQUIRING CALIBRATION                                                 */
/* =============================================================================================== */
/* =============================================================================================== */







long AOloopControl_acquireCalib_mkRandomLinPokeSequence(
	char *IDmodeC_name,  // input
	long NBpokemap,  // input	
	char *IDpokemapC_name,
	char *IDpokeC_name
)
{
	long IDmodeC;
	long IDpokemapC;
	long IDpokeC;
	
	long NBpoke;
	
	printf("Creating pokes\n");
	fflush(stdout);
	
    IDmodeC = image_ID(IDmodeC_name);
    long xsize = data.image[IDmodeC].md[0].size[0];
    long ysize = data.image[IDmodeC].md[0].size[1];
    long NBmode = data.image[IDmodeC].md[0].size[2];

	printf("  %ld %ld %ld\n", xsize, ysize, NBmode);
	fflush(stdout);    

	printf("Creating image %s\n", IDpokemapC_name);
	fflush(stdout);
    IDpokemapC = create_3Dimage_ID(IDpokemapC_name, xsize, ysize, NBpokemap);	


	NBpoke = NBpokemap*3;
	IDpokeC = create_3Dimage_ID(IDpokeC_name, xsize, ysize, NBpoke);	

	// create direction vectors
	float *vectarray;
	vectarray = (float*) malloc(sizeof(float)*NBpokemap*NBmode);
	
	// random allocation
	long pm; // poke map index
	for(pm=0; pm<NBpokemap; pm++)
	{
		long axis;
		float val = 0.0;
		for(axis=0; axis<NBmode; axis++)
		{
			vectarray[pm*NBmode+axis] = 2.0*ran1()-1.0;
			val += vectarray[pm*NBmode+axis]*vectarray[pm*NBmode+axis];
		}
		// set norm = 1
		for(axis=0; axis<NBmode; axis++)
			vectarray[pm*NBmode+axis] /= sqrt(val);
			
		printf("Vect #%3ld   ", pm);
		for(axis=0; axis<NBmode; axis++)
			printf("  %+5.3f", vectarray[pm*NBmode+axis]);
		printf("\n");
	}
	
	
	
	
	// Write vectors to disk
	FILE *fpvect;
	fpvect = fopen("PokeVect.dat", "w");
	for(pm=0; pm<NBpokemap; pm++)
	{
		fprintf(fpvect, "%3ld   ", pm);
		long axis;
		for(axis=0; axis<NBmode; axis++)
			fprintf(fpvect, "  %+8.6f", vectarray[pm*NBmode+axis]);
		fprintf(fpvect, "\n");
	}
	fclose(fpvect);
	
	
	
	long pm1 = 0; // poke frame index
	for(pm=0; pm<NBpokemap; pm++)
	{	
		long axis;
		long ii;
		
		for(axis=0; axis<NBmode; axis++)
		{
			for(ii=0;ii<xsize*ysize;ii++)
				data.image[IDpokemapC].array.F[xsize*ysize*pm + ii] += vectarray[pm*NBmode+axis] * data.image[IDmodeC].array.F[axis*xsize*ysize+ii];
		}
		
		
			for(ii=0;ii<xsize*ysize;ii++)
				data.image[IDpokeC].array.F[xsize*ysize*pm1 + ii] = data.image[IDpokemapC].array.F[xsize*ysize*pm + ii];
			pm1++;
			
			for(ii=0;ii<xsize*ysize;ii++)
				data.image[IDpokeC].array.F[xsize*ysize*pm1 + ii] = -data.image[IDpokemapC].array.F[xsize*ysize*pm + ii];
			pm1++;
			
			for(ii=0;ii<xsize*ysize;ii++)
				data.image[IDpokeC].array.F[xsize*ysize*pm1 + ii] = 0.0;
			pm1++;			
		
	
	}
	
	
	
	free(vectarray);
	
	
	return(IDpokeC);
}







// manages configuration parameters
// initializes configuration parameters structure
//
errno_t AOloopControl_acquireCalib_Measure_WFSrespC_FPCONF(
    char *fpsname,
    uint32_t CMDmode,
    long optarg00
)
{
    uint16_t loopstatus;


    // ===========================
    // SETUP FPS
    // ===========================
    int SMfd = -1;
    FUNCTION_PARAMETER_STRUCT fps = function_parameter_FPCONFsetup(fpsname, CMDmode, &loopstatus, &SMfd);
	strncpy(fps.md->sourcefname, __FILE__, FPS_SRCDIR_STRLENMAX);
	fps.md->sourceline = __LINE__;


    // ===========================
    // ALLOCATE FPS ENTRIES
    // ===========================

    void *pNull = NULL;
    uint64_t FPFLAG;



    if( loopstatus == 0 ) // stop fps
        return RETURN_SUCCESS;

    // =====================================
    // PARAMETER LOGIC AND UPDATE LOOP
    // =====================================
    while ( loopstatus == 1 )
    {
        if( function_parameter_FPCONFloopstep(&fps, CMDmode, &loopstatus) == 1) // Apply logic if update is needed
        {
            // here goes the logic

            functionparameter_CheckParametersAll(&fps);  // check all parameter values
        }
    }
    function_parameter_FPCONFexit( &fps, &SMfd );


    return RETURN_SUCCESS;
}





//
// run loop process
//
errno_t AOloopControl_acquireCalib_Measure_WFSrespC_RUN(
    char *fpsname
)
{
    // ===========================
    // CONNECT TO FPS
    // ===========================
    int SMfd = -1;
    FUNCTION_PARAMETER_STRUCT fps;
    if(function_parameter_struct_connect(fpsname, &fps, FPSCONNECT_RUN, &SMfd) == -1)
    {
        printf("ERROR: fps \"%s\" does not exist -> running without FPS interface\n", fpsname);
        return RETURN_FAILURE;
    }



    // ===============================
    // GET FUNCTION PARAMETER VALUES
    // ===============================


    // ===============================
    // RUN LOOP
    // ===============================
    int loopOK = 1;
    while( loopOK == 1 )
    {
        // here we compute what we need...
        //
        // Note that some mechanism is required to set loopOK to 0 when MyFunction_Stop() is called
        // This can use a separate shared memory path
    }

	function_parameter_struct_disconnect(&fps, &SMfd);

    return RETURN_SUCCESS;
}














/**
 * ## Purpose
 *
 * Acquire WFS response to a series of DM patterns.\n
 *
 *
 * ## Arguments
 *
 * @param[in]  loop            Loop index
 * @param[in]  delayfr         Integer delay [frame]
 * @param[in]  delayRM1us      Fractional delay [us]
 * @param[in]  NBave           Number of frames averaged per DM state
 * @param[in]  NBexcl          Number of frames excluded
 * @param[in]  IDpokeC_name    Poke pattern
 * @param[out] IDoutC_name     Output cube
 * @param[in]  normalize       Normalize flag
 * @param[in]  AOinitMode      AO structure initialization flag
 * @param[in]  NBcycle         Number of cycles averaged (outer)
 * @param[in]  SequInitMode    Sequence initialization mode bitmask
 *                             0x01  swap pairs every 4 indices
 *                             0x02 adjacent pairs are swapped between cycles
 *
 * AOinitMode = 0:  create AO shared mem struct
 * AOinitMode = 1:  connect only to AO shared mem struct
 *
 * INPUT : DMpoke_name : set of DM patterns
 * OUTPUT : WFSmap_name : WFS response maps
 *
 * USR1 signal will stop acquisition immediately
 * USR2 signal completes current cycles and stops acquisition
 *
 * @return IDoutC
 *
 */

long AOloopControl_acquireCalib_Measure_WFSrespC(
    long        loop,
    long        delayfr,
    long        delayRM1us,
    long        NBave,
    long        NBexcl,
    const char *IDpokeC_name,
    const char *IDoutC_name,
    int         normalize,
    int         AOinitMode,
    long        NBcycle,
    uint32_t         SequInitMode
) {
    int stringmaxlen = 500;

    char fname[stringmaxlen];
    char name[stringmaxlen];
    char command[stringmaxlen];
    uint32_t *sizearray;
    long IDoutC;
    char *ptr;

    long NBiter = 10000; // runs until USR1 signal received
    long iter;
    int r;
    long IDpokeC;
    long NBpoke;
    long framesize;
    char *ptr0; // source
    float *arrayf;

    int ret;
    long imcnt;
    long ii;

    long imcntmax;

    FILE *fp;

    int semindexwfs = 7;







    TESTPOINT("%ld %ld %ld %ld %ld %s %s %d %d %ld %d",
             loop,
             delayfr,
             delayRM1us,
             NBave,
             NBexcl,
             IDpokeC_name,
             IDoutC_name,
             normalize,
             AOinitMode,
             NBcycle,
             SequInitMode);


    int SAVE_RMACQU_ALL = 1; // save all intermediate results
    //  int COMP_RMACQU_AVESTEP = 1;  // group images by time delay step and average accordingly -> get time-resolved RM


    char pinfoname[stringmaxlen];   // short name for the processinfo instance, no spaces, no dot, name should be human-readable
    snprintf(pinfoname, stringmaxlen, "aol%ld-acqRM", loop);

    char pinfodescr[stringmaxlen];
    snprintf(pinfodescr, stringmaxlen, "%s", IDpokeC_name);

    char pinfomsg[stringmaxlen];
    snprintf(pinfomsg, stringmaxlen, "delay=%ld+%ldus ave=%ld excl=%ld cyc=%ld", delayfr, delayRM1us, NBave, NBexcl, NBcycle);

    TESTPOINT(" ");


    PROCESSINFO *processinfo;
    int loopOK = 1;

    TESTPOINT(" ");

    processinfo = processinfo_setup(
                      pinfoname,             // short name for the processinfo instance, no spaces, no dot, name should be human-readable
                      pinfodescr,    // description
                      pinfomsg,  // message on startup
                      __FUNCTION__, __FILE__, __LINE__
                  );

    TESTPOINT(" ");

    // OPTIONAL SETTINGS
    processinfo->MeasureTiming = 1; // Measure timing
    processinfo->RT_priority = 80;  // RT_priority, 0-99. Larger number = higher priority. If <0, ignore


    /** ## DETAILS, STEPS */




    /**
     * If NBcycle is set to zero, then the process should run in an infinite loop.
     * The process will then run until receiving USR1.
     *
     */
    if(NBcycle < 1) {
        NBiter = LONG_MAX;    // runs until USR1 signal received
    } else {
        NBiter = NBcycle;
    }

    TESTPOINT(" ");

    processinfo_WriteMessage(processinfo, "Initializing/Loading memory");


    TESTPOINT(" ");

    sizearray = (uint32_t *) malloc(sizeof(uint32_t) * 3);


    printf("INITIALIZE MEMORY (mode %d, meminit = %d)....\n", AOinitMode, AOloopcontrol_meminit);
    fflush(stdout);

    TESTPOINT(" ");
    /*    if(AOloopcontrol_meminit == 0) {
    		TESTPOINT(" ");
            AOloopControl_InitializeMemory(AOinitMode);
        }
        TESTPOINT(" ");

        AOloopControl_loadconfigure(LOOPNUMBER, 1, 2);
        TESTPOINT(" ");
    */


    processinfo_WriteMessage(processinfo, "Connecting to DM RM channel");
    printf("Importing DM response matrix channel shared memory ...\n");
    fflush(stdout);

    char dmRMname[100];
    sprintf(dmRMname, "aol%ld_dmRM", loop);
    long ID_dmRM = read_sharedmem_image(dmRMname);
    if(ID_dmRM == -1) {
        processinfo_error(processinfo, "ERROR: cannot connect to DM response matrix");
        loopOK = 0;
        return RETURN_FAILURE;
    }
    long sizexDM = data.image[ID_dmRM].md[0].size[0];
    long sizeyDM = data.image[ID_dmRM].md[0].size[1];
    long sizeDM = sizexDM*sizeyDM;



    processinfo_WriteMessage(processinfo, "Connecting to WFS stream");
    printf("Importing WFS camera image shared memory ... \n");
    fflush(stdout);

    char WFSname[100];
    if(normalize == 1) {
        sprintf(WFSname, "aol%ld_imWFS1", loop);
    }
    else {
        sprintf(WFSname, "aol%ld_imWFS0", loop);
    }
    long ID_wfsim = read_sharedmem_image(WFSname);
    if(ID_wfsim == -1) {
        processinfo_error(processinfo, "ERROR: cannot connect to WFS stream");
        loopOK = 0;
        return RETURN_FAILURE;
    }
    long sizexWFS = data.image[ID_wfsim].md[0].size[0];
    long sizeyWFS = data.image[ID_wfsim].md[0].size[1];
    long sizeWFS = sizexWFS*sizeyWFS;

    TESTPOINT(" ");










    /**
     * A temporary array is created and initialized to hold the WFS response to each poke mode.
     *
     */
    IDpokeC = image_ID(IDpokeC_name);
    if(IDpokeC == -1) {
        snprintf(pinfomsg, stringmaxlen, "ERROR: Cannot load stream %s", IDpokeC_name);
        processinfo_error(processinfo, pinfomsg);
        loopOK = 0;
        return RETURN_FAILURE;
    } else {
        snprintf(pinfomsg,
                 stringmaxlen,
                 "Loaded stream %s [%d %d %d]",
                 IDpokeC_name,
                 data.image[IDpokeC].md[0].size[0],
                 data.image[IDpokeC].md[0].size[1],
                 data.image[IDpokeC].md[0].size[2]);
        processinfo_WriteMessage(processinfo, pinfomsg);
    }


    NBpoke = data.image[IDpokeC].md[0].size[2];
    sizearray[0] = sizexWFS;
    sizearray[1] = sizeyWFS;
    sizearray[2] = NBpoke;
    IDoutC = create_3Dimage_ID(IDoutC_name, sizearray[0], sizearray[1], sizearray[2]);

    TESTPOINT(" ");
    // timing info for pokes
    long NBpokeTotal = (4 + delayfr + (NBave + NBexcl) * NBpoke) * NBiter + 4;
    long pokecnt = 0;
    struct timespec poke_ts;
    long *pokeTime_sec;
    long *pokeTime_nsec;
    long *pokeTime_index;

    pokeTime_sec    = (long *) malloc(sizeof(long) * NBpokeTotal);
    pokeTime_nsec   = (long *) malloc(sizeof(long) * NBpokeTotal);
    pokeTime_index  = (long *) malloc(sizeof(long) * NBpokeTotal);

    TESTPOINT(" ");

    // create one temporary array per time step
    int AveStep;
    long *IDoutCstep = (long *) malloc(sizeof(long) * NBave);
    // long *IDoutCstepCumul = (long *) malloc(sizeof(long) * NBave); // Cumulative
    for(AveStep = 0; AveStep < NBave; AveStep++) {
        char imname[100];
        sprintf(imname, "imoutStep%03d", AveStep);
        IDoutCstep[AveStep] = create_3Dimage_ID(imname, sizearray[0], sizearray[1], sizearray[2]);
        //sprintf(imname, "%s.ave%03d", IDoutC_name, AveStep);
        //IDoutCstepCumul[AveStep] = create_3Dimage_ID(imname, sizearray[0], sizearray[1], sizearray[2]);
    }

    /*if(COMP_RMACQU_AVESTEP == 1) {
        for(AveStep = 0; AveStep < NBave; AveStep++) {
            char imname[100];
            sprintf(imname, "%s.ave%03d", IDoutC_name, AveStep);
            IDoutCstepCumul[AveStep] = create_3Dimage_ID(imname, sizearray[0], sizearray[1], sizearray[2]);
        }
    }*/

    TESTPOINT(" ");

    // Check that DM size matches poke file
    if(data.image[IDpokeC].md[0].size[0]*data.image[IDpokeC].md[0].size[1] != data.image[ID_dmRM].md[0].size[0]*data.image[ID_dmRM].md[0].size[1]) {
        sprintf(pinfomsg, "ERROR: DM [%ld] and Poke [%ld] mismatch",
                (long) data.image[ID_dmRM].md[0].size[0]*data.image[ID_dmRM].md[0].size[1],
                (long) data.image[IDpokeC].md[0].size[0]*data.image[IDpokeC].md[0].size[1]);
        processinfo_error(processinfo, pinfomsg);
        loopOK = 0;
        list_image_ID();
        return RETURN_FAILURE;
    }


    uint_fast16_t PokeIndex;   // Mode to be poked
    for(PokeIndex = 0; PokeIndex < NBpoke; PokeIndex++)
        for(ii = 0; ii < sizeWFS; ii++) {
            data.image[IDoutC].array.F[PokeIndex * sizeWFS + ii] = 0.0;
            for(AveStep = 0; AveStep < NBave; AveStep++) {
                data.image[IDoutCstep[AveStep]].array.F[PokeIndex * sizeWFS + ii] = 0.0;
            }
        }

    TESTPOINT(" ");



    /**
     * WFS frames will arrive in aol_imWFS1
     *
     */
    /*    if(sprintf(name, "aol%ld_imWFS1", loop) < 1) {
            printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");
        }
        long ID_imWFS1 = read_sharedmem_image(name);
        if(ID_imWFS1 == -1){
    		ID_imWFS1 = create_image_ID(name, 2, sizearray, _DATATYPE_FLOAT, 1, 0);
    	}
    */



    /**
     * A temporary array is created to hold the DM command
     */
    arrayf = (float *) malloc(sizeof(float) * sizeDM);
    for(ii = 0; ii < sizeDM; ii++) {
        arrayf[ii] = 0.0;
    }



    TESTPOINT(" ");


    iter = 0;


    ptr0 = (char *) data.image[IDpokeC].array.F;
    framesize = sizeof(float) * sizexDM * sizeyDM;

    printf("STARTING response measurement...\n");
    fflush(stdout);

    //list_image_ID();

    /**
     * Memory is allocated to arrays
     */
    imcntmax          = (4 + delayfr + (NBave + NBexcl) * NBpoke) * NBiter + 4;

    long *array_iter;
    array_iter        = (long *) malloc(sizeof(long) * imcntmax); // Cycle number

    int  *array_poke;
    array_poke        = (int *)  malloc(sizeof(int) * imcntmax); // Did we poke DM during this time interval ?

    int  *array_accum;
    array_accum       = (int *)  malloc(sizeof(int) * imcntmax); // Does frame count toward accumulated signal ?

    long *array_kk;
    array_kk          = (long *) malloc(sizeof(long) * imcntmax); // frame index within poke mode acquisition

    // frame counter within poke mode acquisition, starts negative
    // becomes positive when accumulating signal
    long *array_kk1;
    array_kk1         = (long *) malloc(sizeof(long) * imcntmax);

    long *array_PokeIndex;
    array_PokeIndex   = (long *) malloc(sizeof(long) * imcntmax); // Poke mode being measured

    long *array_PokeIndex1;
    array_PokeIndex1  = (long *) malloc(sizeof(long) * imcntmax); // Current poke mode on DM


    long *array_PokeIndexMapped;
    array_PokeIndexMapped   = (long *) malloc(sizeof(long) * imcntmax); // Poke mode being measured, index in poke cube

    long *array_PokeIndex1Mapped;
    array_PokeIndex1Mapped  = (long *) malloc(sizeof(long) * imcntmax); // Current poke mode on DM, index in poke cube


    TESTPOINT(" ");

    /**
     * Poke sequence defines the sequence of mode poked
     *
     */
    long *array_PokeSequ;
    array_PokeSequ    = (long *) malloc(sizeof(long) * NBpoke);

    for(PokeIndex = 0; PokeIndex < NBpoke; PokeIndex++) {
        array_PokeSequ[PokeIndex] = PokeIndex;
    }
    if(SequInitMode & 0x01) { // swap pairs every 4 indices
        for(PokeIndex = 0; PokeIndex < NBpoke - 1; PokeIndex += 4) {
            int index0 = PokeIndex;
            int index1 = PokeIndex + 1;

            while(index1 > NBpoke - 1) {
                index1 -= NBpoke;
            }

            // swap sequence pairs
            long tmpPokeMode;
            tmpPokeMode = array_PokeSequ[index0];
            array_PokeSequ[index0] = array_PokeSequ[index1];
            array_PokeSequ[index1] = tmpPokeMode;
        }
    }


    int permut_offset = 0;


    for(imcnt = 0; imcnt < imcntmax; imcnt++) {
        array_poke[imcnt]  = 0;
        array_accum[imcnt] = 0;
    }

    imcnt = 0;

    TESTPOINT(" ");

    // ==================================
    // STARTING LOOP
    // ==================================
    processinfo_loopstart(processinfo); // Notify processinfo that we are entering loop

    sprintf(pinfomsg, "%d  Starting loop, NBcycle = %ld", loopOK, NBcycle);
    processinfo_WriteMessage(processinfo, pinfomsg);

    long loopcnt = 0;

    /**
     * The outermost loop increments the measurement cycle.
     * Signal is to be averaged among cycles. Each measurement cycle repeats the same sequence.
     *
     */

    while(loopOK == 1) {
        TESTPOINT(" ");

        loopOK = processinfo_loopstep(processinfo);

        TESTPOINT(" ");

        processinfo_exec_start(processinfo);

        TESTPOINT(" ");

        printf("NBpoke=%ld # %3ld/%3ld (%6ld/%6ld)\n", NBpoke, iter, NBiter, imcnt, imcntmax);
        fflush(stdout);


        sprintf(pinfomsg, "NBpoke=%ld # %3ld/%3ld (%6ld/%6ld)", NBpoke, iter, NBiter, imcnt, imcntmax);
        processinfo_WriteMessage(processinfo, pinfomsg);



        // RE-ORDER POKE SEQUENCE
        // adjacent pairs are swapped between cycles

        printf("re-ordering poke sequence ... ");
        fflush(stdout);

        if(SequInitMode & 0x02) {
            permut_offset ++;
            if(permut_offset == 2) {
                permut_offset = 0;
            }

            for(PokeIndex = permut_offset; PokeIndex < NBpoke; PokeIndex += 2) {
                int index0 = PokeIndex;
                int index1 = PokeIndex + 1;

                if(index1 > NBpoke - 1) {
                    index1 -= NBpoke;
                }

                // swap sequence pairs
                long tmpPokeMode;
                tmpPokeMode = array_PokeSequ[index0];
                array_PokeSequ[index0] = array_PokeSequ[index1];
                array_PokeSequ[index1] = tmpPokeMode;
            }
        }

        printf("DONE\n");
        fflush(stdout);


        // INITIALIZE WITH FIRST POKE

        long kk  = 0;
        long kk1 = 0;

        uint_fast16_t PokeIndex  = 0;    // Poked mode index
        uint_fast16_t PokeIndex1 = 0;

        uint_fast16_t PokeIndexMapped;   // Poked mode index in original poke cube
        uint_fast16_t PokeIndex1Mapped;

        PokeIndexMapped  = array_PokeSequ[PokeIndex];
        PokeIndex1Mapped = array_PokeSequ[PokeIndex1];

        if((PokeIndex1Mapped < 0) || (PokeIndex1Mapped > NBpoke - 1)) {
            printf("ERROR: PokeIndex1Mapped = %ld is outside range 0 - %ld\n", PokeIndex1Mapped, NBpoke);
            exit(0);
        }


        usleep(delayRM1us);
        data.image[ID_dmRM].md[0].write = 1;
        memcpy((void *) data.image[ID_dmRM].array.F, (void *)(ptr0 + PokeIndex1Mapped * framesize), sizeof(float)*sizeDM);
        COREMOD_MEMORY_image_set_sempost_byID(ID_dmRM, -1);
        data.image[ID_dmRM].md[0].cnt1 = PokeIndex1Mapped;
        data.image[ID_dmRM].md[0].cnt0++;
        data.image[ID_dmRM].md[0].write = 0;
        //AOconf[loop].aorun.DMupdatecnt ++;

        clock_gettime(CLOCK_REALTIME, &poke_ts);
        pokeTime_sec[pokecnt] = (long) poke_ts.tv_sec;
        pokeTime_nsec[pokecnt] = (long) poke_ts.tv_nsec;
        pokeTime_index[pokecnt] = PokeIndex1Mapped;
        pokecnt++;
        if(pokecnt > NBpokeTotal - 1) {
            printf("ERROR: pokecnt %ld   / %ld\n", pokecnt, NBpokeTotal);
            exit(0);
        }

        array_poke[imcnt] = 1;


        TESTPOINT(" ");

        // WAIT FOR LOOP DELAY, PRIMING

        array_iter[imcnt]             = iter;
        array_kk[imcnt]               = kk;
        array_kk1[imcnt]              = kk1;
        array_PokeIndex[imcnt]        = PokeIndex;
        array_PokeIndex1[imcnt]       = PokeIndex1;
        array_PokeIndexMapped[imcnt]  = PokeIndexMapped;
        array_PokeIndex1Mapped[imcnt] = PokeIndex1Mapped;
        imcnt ++;

        TESTPOINT(" ");
        //Read_cam_frame(loop, 1, normalize, 0, 0);
        ImageStreamIO_semwait(&data.image[ID_wfsim], semindexwfs);
        TESTPOINT(" ");

        COREMOD_MEMORY_image_set_sempost_byID(ID_dmRM, -1);
        data.image[ID_dmRM].md[0].cnt0++;

        TESTPOINT(" ");



        // read delayfr frames (priming)
        for(kk = 0; kk < delayfr; kk++) {
            array_iter[imcnt]             = iter;
            array_kk[imcnt]               = kk;
            array_kk1[imcnt]              = kk1;
            array_PokeIndex[imcnt]        = PokeIndex;
            array_PokeIndex1[imcnt]       = PokeIndex1;
            array_PokeIndexMapped[imcnt]  = PokeIndexMapped;
            array_PokeIndex1Mapped[imcnt] = PokeIndex1Mapped;
            imcnt ++;

            //Read_cam_frame(loop, 1, normalize, 0, 0);
            ImageStreamIO_semwait(&data.image[ID_wfsim], semindexwfs);

            kk1++;
            if(kk1 == NBave) {
                kk1 = -NBexcl;
                PokeIndex1++;

                if(PokeIndex1 > NBpoke - 1) {
                    PokeIndex1 -= NBpoke;
                }

                PokeIndex1Mapped  = array_PokeSequ[PokeIndex1];

                if((PokeIndex1Mapped < 0) || (PokeIndex1Mapped > NBpoke - 1)) {
                    printf("ERROR: PokeIndex1Mapped = %ld is outside range 0 - %ld\n", PokeIndex1Mapped, NBpoke);
                    exit(0);
                }

                // POKE
                usleep(delayRM1us);
                data.image[ID_dmRM].md[0].write = 1;
                memcpy((void *) data.image[ID_dmRM].array.F, (void *)(ptr0 + PokeIndex1Mapped * framesize), sizeof(float)*sizeDM);
                COREMOD_MEMORY_image_set_sempost_byID(ID_dmRM, -1);
                data.image[ID_dmRM].md[0].cnt1 = PokeIndex1Mapped;
                data.image[ID_dmRM].md[0].cnt0++;
                data.image[ID_dmRM].md[0].write = 0;
                //AOconf[loop].aorun.DMupdatecnt ++;

                clock_gettime(CLOCK_REALTIME, &poke_ts);
                pokeTime_sec[pokecnt] = (long) poke_ts.tv_sec;
                pokeTime_nsec[pokecnt] = (long) poke_ts.tv_nsec;
                pokeTime_index[pokecnt] = PokeIndex1Mapped;
                pokecnt++;
                if(pokecnt > NBpokeTotal - 1) {
                    printf("ERROR: pokecnt %ld   / %ld\n", pokecnt, NBpokeTotal);
                    exit(0);
                }

                array_poke[imcnt] = 1;
            }
        }


        TESTPOINT(" ");

        /**
         * First inner loop increment poke mode
         *
         */
        while((PokeIndex < NBpoke) && (data.signal_USR1 == 0)) {
            // INTEGRATION

            for(kk = 0; kk < NBave + NBexcl; kk++) {
                array_iter[imcnt]             = iter;
                array_kk[imcnt]               = kk;
                array_kk1[imcnt]              = kk1;
                array_PokeIndex[imcnt]        = PokeIndex;
                array_PokeIndex1[imcnt]       = PokeIndex1;
                array_PokeIndexMapped[imcnt]  = PokeIndexMapped;
                array_PokeIndex1Mapped[imcnt] = PokeIndex1Mapped;
                imcnt ++;



                //Read_cam_frame(loop, 1, normalize, 0, 0);
                ImageStreamIO_semwait(&data.image[ID_wfsim], semindexwfs);


                if(kk < NBave) { // Capture signal
                    //  for(ii=0; ii<sizeWFS; ii++)
                    //     data.image[IDoutC].array.F[PokeIndexMapped*sizeWFS+ii] += data.image[ID_imWFS1].array.F[ii];
                    ptr = (char *) data.image[IDoutCstep[kk]].array.F;
                    ptr += sizeof(float) * PokeIndexMapped * sizeWFS;
                    memcpy(ptr, data.image[ID_wfsim].array.F, sizeof(float)*sizeWFS); //NEW
                    array_accum[imcnt] = 1;
                }
                kk1++;
                if(kk1 == NBave) { // wait for NBexcl excluded frames. We poke after delayRM1us
                    kk1 = -NBexcl;
                    PokeIndex1++;

                    if(PokeIndex1 > NBpoke - 1) {
                        PokeIndex1 -= NBpoke;
                    }

                    PokeIndex1Mapped  = array_PokeSequ[PokeIndex1];

                    usleep(delayRM1us);


                    // POKE
                    data.image[ID_dmRM].md[0].write = 1;
                    memcpy((void *)(data.image[ID_dmRM].array.F), (void *)(ptr0 + PokeIndex1Mapped * framesize), sizeof(float)*sizeDM);
                    COREMOD_MEMORY_image_set_sempost_byID(ID_dmRM, -1);
                    data.image[ID_dmRM].md[0].cnt1 = PokeIndex1Mapped;
                    data.image[ID_dmRM].md[0].cnt0++;
                    data.image[ID_dmRM].md[0].write = 0;
                    //AOconf[loop].aorun.DMupdatecnt ++;

                    clock_gettime(CLOCK_REALTIME, &poke_ts);
                    pokeTime_sec[pokecnt] = (long) poke_ts.tv_sec;
                    pokeTime_nsec[pokecnt] = (long) poke_ts.tv_nsec;
                    pokeTime_index[pokecnt] = PokeIndex1Mapped;
                    pokecnt++;
                    if(pokecnt > NBpokeTotal - 1) {
                        printf("ERROR: pokecnt %ld   / %ld\n", pokecnt, NBpokeTotal);
                        exit(0);
                    }

                    array_poke[imcnt] = 1;
                }
            }

            PokeIndex++;
            PokeIndexMapped  = array_PokeSequ[PokeIndex];
        }


        for(ii = 0; ii < sizeDM; ii++) {
            arrayf[ii] = 0.0;
        }


        TESTPOINT(" ");

        // zero DM channel

        usleep(delayRM1us);
        data.image[ID_dmRM].md[0].write = 1;
        memcpy((void *)(data.image[ID_dmRM].array.F), (void *)(arrayf), sizeof(float)*sizeDM);
        COREMOD_MEMORY_image_set_sempost_byID(ID_dmRM, -1);
        data.image[ID_dmRM].md[0].cnt1 = 0;
        data.image[ID_dmRM].md[0].cnt0++;
        data.image[ID_dmRM].md[0].write = 0;
        //AOconf[loop].aorun.DMupdatecnt ++;

        clock_gettime(CLOCK_REALTIME, &poke_ts);
        pokeTime_sec[pokecnt] = (long) poke_ts.tv_sec;
        pokeTime_nsec[pokecnt] = (long) poke_ts.tv_nsec;
        pokeTime_index[pokecnt] = -1;
        pokecnt++;
        if(pokecnt > NBpokeTotal - 1) {
            printf("ERROR: pokecnt %ld   / %ld\n", pokecnt, NBpokeTotal);
            exit(0);
        }

        array_poke[imcnt] = 1;


        printf("Combining results ... ");
        fflush(stdout);

        for(AveStep = 0; AveStep < NBave; AveStep++) { // sum over all values AveStep
            for(PokeIndexMapped = 0; PokeIndexMapped < NBpoke ; PokeIndexMapped++) {
                for(ii = 0; ii < sizeWFS; ii++) {
                    data.image[IDoutC].array.F[PokeIndexMapped * sizeWFS + ii] += data.image[IDoutCstep[AveStep]].array.F[PokeIndexMapped * sizeWFS + ii];
                }

                /*if(COMP_RMACQU_AVESTEP == 1) {   // sum over iterations
                    for(ii = 0; ii < AOconf[loop].WFSim.sizeWFS; ii++) {
                        data.image[IDoutCstepCumul[AveStep]].array.F[PokeIndexMapped * AOconf[loop].WFSim.sizeWFS + ii] += data.image[IDoutCstep[AveStep]].array.F[PokeIndexMapped * AOconf[loop].WFSim.sizeWFS + ii];
                    }
                }*/
            }
        }

        printf("DONE\n");
        fflush(stdout);


        TESTPOINT(" ");

        if(SAVE_RMACQU_ALL == 1) { // Save all intermediate result
            char tmpfname[200];
            FILE *fplog;
            int retv;

            retv = system("mkdir -p tmpRMacqu");

            fplog = fopen("tmpRMacqu/RMacqulog.txt", "w");
            fprintf(fplog, "%-20s  %ld\n", "loop", loop);
            fprintf(fplog, "%-20s  %ld\n", "delayfr", delayfr);
            fprintf(fplog, "%-20s  %ld\n", "delayRM1us", delayRM1us);
            fprintf(fplog, "%-20s  %ld\n", "NBave", NBave);
            fprintf(fplog, "%-20s  %ld\n", "NBexcl", NBexcl);
            fprintf(fplog, "%-20s  %s\n", "IDpokeC_name", IDpokeC_name);
            fprintf(fplog, "%-20s  %s\n", "IIDoutC_name", IDoutC_name);
            fprintf(fplog, "%-20s  %d\n", "normalize", normalize);
            fprintf(fplog, "%-20s  %d\n", "AOinitMode", AOinitMode);
            fprintf(fplog, "%-20s  %ld\n", "NBcycle", NBcycle);
            fprintf(fplog, "%-20s  %d\n", "SequInitMode", SequInitMode);
            fclose(fplog);


            // Save individual time step within averaging for high temporal resolution
            for(AveStep = 0; AveStep < NBave; AveStep++) { // save to disk IDoutCstep[AveStep]
                char imname[100];
                sprintf(imname, "imoutStep%03d", AveStep);

                sprintf(tmpfname, "!tmpRMacqu/%s.tstep%03d.iter%03ld.fits", IDoutC_name, AveStep, iter);
                //list_image_ID();
                printf("SAVING %s -> %s ... ", imname, tmpfname);
                fflush(stdout);
                save_fits(imname, tmpfname);
                printf("DONE\n");
                fflush(stdout);
            }

        }

        iter++;

        TESTPOINT(" ");


        // process signals, increment loop counter
        processinfo_exec_end(processinfo);

        loopcnt++;




        if(iter == NBiter) {
            sprintf(pinfomsg, "Reached iteration %ld -> stop", NBiter);
            processinfo_WriteMessage(processinfo, pinfomsg);
            loopOK = 0;
        }
        if(data.signal_USR1 == 1) {
            sprintf(pinfomsg, "received USR1-> stop");
            processinfo_WriteMessage(processinfo, pinfomsg);
            loopOK = 0;
        }
        if(data.signal_USR2 == 1) {
            sprintf(pinfomsg, "received USR2-> stop");
            processinfo_WriteMessage(processinfo, pinfomsg);
            loopOK = 0;
        }

        TESTPOINT(" ");

    } // end of iteration loop


    free(arrayf);

    free(sizearray);


    TESTPOINT(" ");

    processinfo_cleanExit(processinfo);

    TESTPOINT(" ");

    for(PokeIndex = 0; PokeIndex < NBpoke; PokeIndex++)
        for(ii = 0; ii < sizeWFS; ii++) {
            data.image[IDoutC].array.F[PokeIndex * sizeWFS + ii] /= NBave * iter;
        }

    TESTPOINT(" ");

    // print poke log
    int retv;
    retv = system("mkdir -p tmpRMacqu");
    fp = fopen("./tmpRMacqu/RMpokelog.txt", "w");
    for(imcnt = 0; imcnt < imcntmax; imcnt++) {
        fprintf(fp, "%6ld %3ld    %1d %1d     %6ld  %6ld     %4ld %4ld   %4ld %4ld     %3ld %3ld %3ld\n",
                imcnt,
                array_iter[imcnt],
                array_poke[imcnt],
                array_accum[imcnt],
                array_kk[imcnt],
                array_kk1[imcnt],
                array_PokeIndex[imcnt],
                array_PokeIndex1[imcnt],
                array_PokeIndexMapped[imcnt],
                array_PokeIndex1Mapped[imcnt],
                NBpoke,
                NBexcl,
                NBave
               );
    }
    fclose(fp);

    TESTPOINT(" ");

    printf("Writing poke timing to file ... ");
    fflush(stdout);

    fp = fopen("./tmpRMacqu/RMpokeTiming.txt", "w");
    double ftime0 = pokeTime_sec[0] + 1.0e-9 * pokeTime_nsec[0];
    double ftime;
    for(ii = 0; ii < pokecnt; ii++) {
        ftime = pokeTime_sec[ii] + 1.0e-9 * pokeTime_nsec[ii];
        fprintf(fp, "%5ld  %16ld.%09ld  %5ld  %12.9lf\n", ii, pokeTime_sec[ii], pokeTime_nsec[ii], pokeTime_index[ii], ftime - ftime0);
        ftime0 = ftime;
    }
    fclose(fp);

    printf("DONE\n");
    fflush(stdout);


    TESTPOINT(" ");

    free(IDoutCstep);

    free(array_iter);
    free(array_accum);
    free(array_poke);
    free(array_kk);
    free(array_kk1);
    free(array_PokeIndex);
    free(array_PokeIndex1);
    free(array_PokeIndexMapped);
    free(array_PokeIndex1Mapped);
    free(array_PokeSequ);

    free(pokeTime_sec);
    free(pokeTime_nsec);
    free(pokeTime_index);

    TESTPOINT(" ");

    return(IDoutC);
}












errno_t AOcontrolLoop_acquireCalib_Measure_WFS_linResponse_FPCONF(
    char *fpsname,
    uint32_t CMDmode
)
{
    uint16_t loopstatus;


	// TEST
	static unsigned long connection_count_1 = 0;
	static unsigned long connection_count_2 = 0;

    // ===========================
    // SETUP FPS
    // ===========================
    int SMfd = -1;
    FUNCTION_PARAMETER_STRUCT fps =
        function_parameter_FPCONFsetup(fpsname, CMDmode, &loopstatus, &SMfd);
    strncpy(fps.md->sourcefname, __FILE__, FPS_SRCDIR_STRLENMAX);
    fps.md->sourceline = __LINE__;

	int SMfd_mlat = -1;
	int SMfd_DMcomb = -1;


    // ===========================
    // ALLOCATE FPS ENTRIES IF NOT ALREADY EXIST
    // ===========================
    void *pNull = NULL;
    uint64_t FPFLAG;


    long loop_default[4] = { 0, 0, 10, 0 };
    long fpi_loop = function_parameter_add_entry(&fps, ".loop",
                    "loop index",
                    FPTYPE_INT64, FPFLAG_DEFAULT_INPUT, &loop_default);





    double ampl_default[4] = { 0.01, 0.000001, 1.0, 0.01 };
    long fpi_ampl = function_parameter_add_entry(&fps, ".ampl",
                    "RM poke amplitude",
                    FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &ampl_default);

    long delayfr_default[4] = { 2, 0, 10, 2 };
    long fpi_delayfr = function_parameter_add_entry(&fps, ".delayfr",
                       "Frame delay, whole part",
                       FPTYPE_INT64, FPFLAG_DEFAULT_INPUT, &delayfr_default);

    long delayRM1us_default[4] = { 100, 0, 1000000, 100 };
    long fpi_delayRM1us = function_parameter_add_entry(&fps, ".delayRM1us",
                          "Sub-frame delay [us]",
                          FPTYPE_INT64, FPFLAG_DEFAULT_INPUT, &delayRM1us_default);

    long NBave_default[4] = { 5, 1, 1000, 5 };
    long fpi_NBave = function_parameter_add_entry(&fps, ".NBave",
                     "Number of frames averaged for a single poke measurement",
                     FPTYPE_INT64, FPFLAG_DEFAULT_INPUT, &NBave_default);

    long NBexcl_default[4] = { 1, 0, 100, 1 };
    long fpi_NBexcl = function_parameter_add_entry(&fps, ".NBexcl",
                      "Number of frames excluded",
                      FPTYPE_INT64, FPFLAG_DEFAULT_INPUT, &NBexcl_default);

    long NBcycle_default[4] = { 10, 1, 1000, 10 };
    long fpi_NBcycle = function_parameter_add_entry(&fps, ".NBcycle",
                       "Number of measurement cycles to be repeated",
                       FPTYPE_INT64, FPFLAG_DEFAULT_INPUT, &NBcycle_default);

    long NBinnerCycle_default[4] = { 10, 1, 1000, 10 };
    long fpi_NBinnerCycle = function_parameter_add_entry(&fps, ".NBinnerCycle",
                            "Number of inner cycles (how many consecutive times should a single +/- poke be repeated)",
                            FPTYPE_INT64, FPFLAG_DEFAULT_INPUT, &NBinnerCycle_default);


    long AOinitMode_default[4] = { 0, 0, 2, 0 };
    long fpi_AOinitMode = function_parameter_add_entry(&fps, ".AOinitMode",
                          "AO initialization mode",
                          FPTYPE_INT64, FPFLAG_DEFAULT_INPUT, &AOinitMode_default);




    long fpi_MaskMode = function_parameter_add_entry(&fps, ".MaskMode",
                        "Mask mode, DM and WFS",
                        FPTYPE_ONOFF, FPFLAG_DEFAULT_INPUT, pNull);

    double maskRMp0_default[4] = { 0.2, 0.0, 1.0, 0.2 };
    long fpi_maskRMp0 = function_parameter_add_entry(&fps, ".DMmask.RMp0",
                        "DM mask, first percentile point",
                        FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &maskRMp0_default);

    double maskRMc0_default[4] = { 1.0, 0.0, 5.0, 1.0 };
    long fpi_maskRMc0 = function_parameter_add_entry(&fps, ".DMmask.RMc0",
                        "DM mask, first percentile value coefficient",
                        FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &maskRMc0_default);

    double maskRMp1_default[4] = { 0.5, 0.0, 1.0, 0.5 };
    long fpi_maskRMp1 = function_parameter_add_entry(&fps, ".DMmask.RMp1",
                        "DM mask, second percentile point",
                        FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &maskRMp1_default);

    double maskRMc1_default[4] = { 0.5, 0.0, 5.0, 0.5 };
    long fpi_maskRMc1 = function_parameter_add_entry(&fps, ".DMmask.RMc1",
                        "DM mask, second percentile value coefficient",
                        FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &maskRMc1_default);

    double DMproxrad_default[4] = { 2.5, 0.0, 10.0, 2.5 };
    long fpi_DMproxrad = function_parameter_add_entry(&fps, ".DMmask.proxrad",
                         "DM actuator proximity radius",
                         FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &DMproxrad_default);



    double maskWFSp0_default[4] = { 0.2, 0.0, 1.0, 0.2 };
    long fpi_maskWFSp0 = function_parameter_add_entry(&fps, ".WFSmask.RMp0",
                         "WFS mask, first percentile point",
                         FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &maskWFSp0_default);

    double maskWFSc0_default[4] = { 1.0, 0.0, 5.0, 1.0 };
    long fpi_maskWFSc0 = function_parameter_add_entry(&fps, ".WFSmask.RMc0",
                         "WFS mask, first percentile value coefficient",
                         FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &maskWFSc0_default);

    double maskWFSp1_default[4] = { 0.6, 0.0, 1.0, 0.6 };
    long fpi_maskWFSp1 = function_parameter_add_entry(&fps, ".WFSmask.RMp1",
                         "WFS mask, second percentile point",
                         FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &maskWFSp1_default);

    double maskWFSc1_default[4] = { 0.6, 0.0, 5.0, 0.6 };
    long fpi_maskWFSc1 = function_parameter_add_entry(&fps, ".WFSmask.RMc1",
                         "WFS mask, second percentile value coefficient",
                         FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &maskWFSc1_default);





    // input streams
    FPFLAG = FPFLAG_DEFAULT_INPUT | FPFLAG_FILE_RUN_REQUIRED;
    long fpi_filename_pokeC     = function_parameter_add_entry(&fps, ".fn_pokeC",
                                  "Poke sequence cube",
                                  FPTYPE_FITSFILENAME, FPFLAG, pNull);

    FPFLAG = FPFLAG_DEFAULT_INPUT | FPFLAG_FILE_RUN_REQUIRED;
    long fpi_filename_DMRMmask = function_parameter_add_entry(&fps, ".fn_RMDMmask",
                                 "RM active DM actuators mask",
                                 FPTYPE_FITSFILENAME, FPFLAG, pNull);
    functionparameter_SetParamValue_STRING(&fps, ".fn_RMDMmask", "./conf/RM_DMmask.fits");



    // output files and dir
    long fpi_out_dirname      = function_parameter_add_entry(&fps, ".out.dirname",
                                "output directory",
                                FPTYPE_DIRNAME, FPFLAG_DEFAULT_OUTPUT, pNull);

    long fpi_out_label      = function_parameter_add_entry(&fps, ".out.label",
                              "output label",
                              FPTYPE_STRING, FPFLAG_DEFAULT_OUTPUT, pNull);

    long fpi_out_timestring    = function_parameter_add_entry(&fps, ".out.timestring",
                                 "output timestring",
                                 FPTYPE_STRING, FPFLAG_DEFAULT_OUTPUT, pNull);





    long fpi_filename_respC       = function_parameter_add_entry(&fps, ".out.fn_respC",
                                    "output response matrix",
                                    FPTYPE_FILENAME, FPFLAG_DEFAULT_OUTPUT, pNull);

    long fpi_filename_wfsref      = function_parameter_add_entry(&fps, ".out.fn_wfsref",
                                    "output WFS reference",
                                    FPTYPE_FILENAME, FPFLAG_DEFAULT_OUTPUT, pNull);





    // on/off parameters
    long fpi_normalize = function_parameter_add_entry(&fps, ".normalize",
                         "Normalize WFS frames",
                         FPTYPE_ONOFF, FPFLAG_DEFAULT_INPUT, pNull);

    long fpi_Hpokemode = function_parameter_add_entry(&fps, ".Hpoke",
                         "Hadamard Poke mode",
                         FPTYPE_ONOFF, FPFLAG_DEFAULT_INPUT, pNull);


    long fpi_autoTiming = function_parameter_add_entry(&fps, ".autoTiming",
                          "Auto Timing",
                          FPTYPE_ONOFF, FPFLAG_DEFAULT_INPUT, pNull);


    // Compute Actions
    long fpi_comp_RM_DMmask = function_parameter_add_entry(&fps, ".compDMmask",
                              "(re-)compute RM DM mask",
                              FPTYPE_ONOFF, FPFLAG_DEFAULT_INPUT, pNull);

    long fpi_comp_RM_Mpoke = function_parameter_add_entry(&fps, ".compMpoke",
                             "(re-)compute poke matrices",
                             FPTYPE_ONOFF, FPFLAG_DEFAULT_INPUT, pNull);


    double LOmaxCPA_default[4] = { 3.0, 1.0, 30.0, 3.0 };
    long fpi_LOmaxCPA = function_parameter_add_entry(&fps, ".LOmaxCPA",
                        "Low orders max CPA",
                        FPTYPE_FLOAT64, FPFLAG_DEFAULT_INPUT, &LOmaxCPA_default);


    // External scripts (post)
    long fpi_exec_post_RMdecode = function_parameter_add_entry(&fps, ".exec.RMdecode",
                                  "RM decode script",
                                  FPTYPE_EXECFILENAME, FPFLAG_DEFAULT_INPUT|FPFLAG_FILE_RUN_REQUIRED, pNull);

    long fpi_exec_post_mkDMWFSmasks = function_parameter_add_entry(&fps, ".exec.mkDMWFSmasks",
                                      "Make DM and WFS masks",
                                      FPTYPE_EXECFILENAME, FPFLAG_DEFAULT_INPUT|FPFLAG_FILE_RUN_REQUIRED, pNull);

    long fpi_exec_post_mkDMslaveact = function_parameter_add_entry(&fps, ".exec.mkDMslaveact",
                                      "Make DM slaved actuators",
                                      FPTYPE_EXECFILENAME, FPFLAG_DEFAULT_INPUT|FPFLAG_FILE_RUN_REQUIRED, pNull);

    long fpi_exec_post_mkLODMmodes = function_parameter_add_entry(&fps, ".exec.mkLODMmodes",
                                     "Make DM low order modes",
                                     FPTYPE_EXECFILENAME, FPFLAG_DEFAULT_INPUT|FPFLAG_FILE_RUN_REQUIRED, pNull);




    long fpi_FPS_mlat = function_parameter_add_entry(&fps, ".FPS_mlat",
                        "FPS mlat",
                        FPTYPE_FPSNAME, FPFLAG_DEFAULT_INPUT|FPFLAG_FPS_RUN_REQUIRED, pNull);
    FUNCTION_PARAMETER_STRUCT FPS_mlat;
    fps.parray[fpi_FPS_mlat].info.fps.FPSNBparamMAX = 0;

    long fpi_FPS_DMcomb = function_parameter_add_entry(&fps, ".FPS_DMcomb",
                          "FPS DMcomb",
                          FPTYPE_FPSNAME, FPFLAG_DEFAULT_INPUT|FPFLAG_FPS_RUN_REQUIRED, pNull);
    FUNCTION_PARAMETER_STRUCT FPS_DMcomb;
    fps.parray[fpi_FPS_DMcomb].info.fps.FPSNBparamMAX = 0;


    if( loopstatus == 0 ) // stop fps
        return RETURN_SUCCESS;





    // =====================================
    // PARAMETER LOGIC AND UPDATE LOOP
    // =====================================
    while ( loopstatus == 1 )
    {
        usleep(50);

        if( function_parameter_FPCONFloopstep(&fps, CMDmode, &loopstatus) == 1) // Apply logic if update is needed
        {
			
			printf("======== connecting to aux FPS ============\n");//TBE
			fflush(stdout);
			
            //
            //  Connect to aux FPS
            //


            if ( fps.parray[fpi_FPS_mlat].info.fps.FPSNBparamMAX < 1 ) {
                functionparameter_ConnectExternalFPS(&fps, fpi_FPS_mlat, &FPS_mlat, &SMfd_mlat);
                connection_count_1 ++;
                printf("---- [%s %d] mlat connection count = %lu\n", __FILE__, __LINE__, connection_count_1);  //TBE
                fflush(stdout);
            }

            if ( fps.parray[fpi_FPS_DMcomb].info.fps.FPSNBparamMAX  < 1 ) {
                functionparameter_ConnectExternalFPS(&fps, fpi_FPS_DMcomb, &FPS_DMcomb, &SMfd_DMcomb);
                connection_count_2 ++;
                printf("---- [%s %d] DMcomb connection count = %lu\n", __FILE__, __LINE__, connection_count_2); //TBE
                fflush(stdout);
            }

			printf("======== DONE connecting to aux FPS ============\n");//TBE
			fflush(stdout);



            //
            // Compute action: make DM RM mask
            //
            if(fps.parray[fpi_comp_RM_DMmask].fpflag & FPFLAG_ONOFF) {
                if ( fps.parray[fpi_FPS_DMcomb].info.fps.FPSNBparamMAX > 0 ) {
                    int DMxsize = functionparameter_GetParamValue_INT64 ( &FPS_DMcomb, ".DMxsize" );
                    int DMysize = functionparameter_GetParamValue_INT64 ( &FPS_DMcomb, ".DMysize" );
                    int DMMODE = functionparameter_GetParamValue_INT64 ( &FPS_DMcomb, ".DMMODE" );

                    if(DMMODE == 0) { // square grid DM
                        make_disk("RMDMmask", DMxsize, DMysize, 0.5*DMxsize, 0.5*DMysize, 0.5*(DMxsize)+0.6);
                    }
                    else // modal DM -> all pixels to 1
                    {
                        make_disk("RMDMmask", DMxsize, DMysize, 0.5*DMxsize, 0.5*DMysize, (DMxsize+DMysize));
                    }

                    save_fl_fits("RMDMmask", "!./conf/RM_DMmask.fits");
                    delete_image_ID("RMDMmask");

                    // set back to OFF
                    fps.parray[fpi_comp_RM_DMmask].fpflag &= ~FPFLAG_ONOFF;
                }
            }


            //
            // Compute action: make Spoke and Hpoke
            //
            if(fps.parray[fpi_comp_RM_Mpoke].fpflag & FPFLAG_ONOFF) {
                if ( fps.parray[fpi_FPS_DMcomb].info.fps.FPSNBparamMAX > 0 ) {
                    int DMxsize = functionparameter_GetParamValue_INT64 ( &FPS_DMcomb, ".DMxsize" );
                    int DMysize = functionparameter_GetParamValue_INT64 ( &FPS_DMcomb, ".DMysize" );
                    int DMMODE = functionparameter_GetParamValue_INT64 ( &FPS_DMcomb, ".DMMODE" );

                    AOloopControl_compTools_mkSimpleZpokeM(DMxsize, DMysize, "Spoke");
                    save_fl_fits("Spoke", "!./conf/Spoke.fits");
                    delete_image_ID("Spoke");

                    load_fits("./conf/RM_DMmask.fits", "RMDMmask", 1);
                    AOloopControl_computeCalib_mkHadamardModes("RMDMmask", "Hpoke");

                    save_fl_fits("Hpoke", "!./conf/Hpoke.fits");
                    save_fl_fits("Hpixindex", "!./conf/Hpixindex.fits");
                    save_fl_fits("Hmat", "!./conf/Hmat.fits");

                    // create compressed files
                    if(system("gzip -kf ./conf/Hmat.fits") != 0) {
                        printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
                    }
                    if(system("gzip -kf ./conf/Hpixindex.fits") != 0) {
                        printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
                    }
                    if(system("gzip -kf ./conf/Hpoke.fits") != 0) {
                        printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
                    }

                    delete_image_ID("RMDMmask");

                    // set back to OFF
                    fps.parray[fpi_comp_RM_Mpoke].fpflag &= ~FPFLAG_ONOFF;
                }


                if(fps.parray[fpi_Hpokemode].fpflag & FPFLAG_ONOFF) {
                    char fname_Hpoke[200];
                    functionparameter_SetParamValue_STRING(&fps, ".fn_pokeC", "./conf/Hpoke.fits");
                }
                else {
                    functionparameter_SetParamValue_STRING(&fps, ".fn_pokeC", "./conf/Spoke.fits");
                }
            }



			printf("======== Import parameters ============\n");//TBE
			fflush(stdout);

            //
            // Import latency measurements and compute timing parameters
            //
            if(fps.parray[fpi_autoTiming].fpflag & FPFLAG_ONOFF) {   // ON state
                if ( fps.parray[fpi_FPS_mlat].info.fps.FPSNBparamMAX > 0 ) {
                    double latfr = functionparameter_GetParamValue_FLOAT64 ( &FPS_mlat, ".out.latencyfr" );
                    double framerateHz = functionparameter_GetParamValue_FLOAT64 ( &FPS_mlat, ".out.framerateHz" );

                    long delayfr = (long) (1000000.0*latfr);

                    // RMdelay = hardwlaten - 0.5 - excl/2
                    double RMdelay = latfr - 0.5 - 0.5 * fps.parray[fpi_NBexcl].val.l[0];

                    int RMdelayfr = ((int) (latfr - 0.5 - 0.5 * fps.parray[fpi_NBexcl].val.l[0] + 10.0)) + 1 - 10;

                    int delayRM1us = (int) ((1.0*RMdelayfr - RMdelay)/framerateHz*1000000.0);

                    if(RMdelay > 0) {
                        fps.parray[fpi_delayfr].val.l[0] = RMdelayfr;
                        fps.parray[fpi_delayRM1us].val.l[0] = delayRM1us;
                    }
                    else
                    {
                        fps.parray[fpi_delayfr].val.l[0] = 0;
                        fps.parray[fpi_delayRM1us].val.l[0] = 0;
                    }
                }
            }
			printf("======== DONE Import parameters ============\n");//TBE
			fflush(stdout);
sleep(1);


			printf("======== Check parameters ============\n");//TBE
			fflush(stdout);
            functionparameter_CheckParametersAll(&fps);  // check all parameter values

			printf("LOOP END =================\n");
			fflush(stdout);

                }
        
        
    }

    if ( fps.parray[fpi_FPS_mlat].info.fps.FPSNBparamMAX > 0 ) {
        function_parameter_struct_disconnect( &FPS_mlat, &SMfd_mlat );
    }

    if ( fps.parray[fpi_FPS_DMcomb].info.fps.FPSNBparamMAX > 0 ) {
        function_parameter_struct_disconnect( &FPS_DMcomb, &SMfd_DMcomb );
    }

    function_parameter_FPCONFexit( &fps, &SMfd );


    return RETURN_SUCCESS;
}








int_fast8_t AOcontrolLoop_acquireCalib_Measure_WFS_linResponse_RUN(
    char *fpsname
) {
    // ===========================
    // CONNECT TO FPS
    // ===========================
    int SMfd = -1;
    FUNCTION_PARAMETER_STRUCT fps;
    if(function_parameter_struct_connect(fpsname, &fps, FPSCONNECT_RUN, &SMfd) == -1) {
        printf("ERROR: fps \"%s\" does not exist -> running without FPS interface\n", fpsname);
        return RETURN_FAILURE;
    }


    printf("TEST POINT LINE %d\n", __LINE__);
    fflush(stdout);


    // ===============================
    // GET FUNCTION PARAMETER VALUES
    // ===============================

    long loop        = functionparameter_GetParamValue_INT64(&fps, ".loop");
    float ampl       = functionparameter_GetParamValue_FLOAT64(&fps, ".ampl");
    long delayfr     = functionparameter_GetParamValue_INT64(&fps, ".delayfr");
    long delayRM1us  = functionparameter_GetParamValue_INT64(&fps, ".delayRM1us");
    long NBave       = functionparameter_GetParamValue_INT64(&fps, ".NBave");
    long NBexcl      = functionparameter_GetParamValue_INT64(&fps, ".NBexcl");
    long NBcycle     = functionparameter_GetParamValue_INT64(&fps, ".NBcycle");
    long NBinnerCycle = functionparameter_GetParamValue_INT64(&fps, ".NBinnerCycle");

    printf("TEST POINT LINE %d\n", __LINE__);
    fflush(stdout);



    char pokeC_filename[FUNCTION_PARAMETER_STRMAXLEN];
    strncpy(pokeC_filename,  functionparameter_GetParamPtr_STRING(&fps, ".fn_pokeC"),  FUNCTION_PARAMETER_STRMAXLEN);

    char respC_filename[FUNCTION_PARAMETER_STRMAXLEN];
    strncpy(respC_filename,  functionparameter_GetParamPtr_STRING(&fps, ".out.fn_respC"),  FUNCTION_PARAMETER_STRMAXLEN);
    char respC_sname[] = "respC";

    char wfsref_filename[FUNCTION_PARAMETER_STRMAXLEN];
    strncpy(wfsref_filename,  functionparameter_GetParamPtr_STRING(&fps, ".out.fn_wfsref"),  FUNCTION_PARAMETER_STRMAXLEN);
    char wfsref_sname[] = "wfsref";

    printf("TEST POINT LINE %d\n", __LINE__);
    fflush(stdout);




    uint64_t *FPFLAG_NORMALIZE;
    FPFLAG_NORMALIZE = functionparameter_GetParamPtr_fpflag(&fps, ".normalize");

    uint64_t *FPFLAG_HPOKE;
    FPFLAG_HPOKE = functionparameter_GetParamPtr_fpflag(&fps, ".Hpoke");

	uint64_t *FPFLAG_MASKMODE;
	FPFLAG_MASKMODE = functionparameter_GetParamPtr_fpflag(&fps, ".MaskMode");
	
	
	

    int AOinitMode = functionparameter_GetParamValue_INT64(&fps, ".AOinitMode");


    char execRMdecode[FUNCTION_PARAMETER_STRMAXLEN];
    strncpy(execRMdecode, functionparameter_GetParamPtr_STRING(&fps, ".exec.RMdecode"),  FUNCTION_PARAMETER_STRMAXLEN);

    char execmkDMWFSmasks[FUNCTION_PARAMETER_STRMAXLEN];
    strncpy(execmkDMWFSmasks, functionparameter_GetParamPtr_STRING(&fps, ".exec.mkDMWFSmasks"),  FUNCTION_PARAMETER_STRMAXLEN);

    char execmkDMslaveact[FUNCTION_PARAMETER_STRMAXLEN];
    strncpy(execmkDMslaveact, functionparameter_GetParamPtr_STRING(&fps, ".exec.mkDMslaveact"),  FUNCTION_PARAMETER_STRMAXLEN);
    
    char execmkLODMmodes[FUNCTION_PARAMETER_STRMAXLEN];
    strncpy(execmkLODMmodes, functionparameter_GetParamPtr_STRING(&fps, ".exec.mkLODMmodes"),  FUNCTION_PARAMETER_STRMAXLEN);



	char outdirname[FUNCTION_PARAMETER_STRMAXLEN];
    strncpy(outdirname, functionparameter_GetParamPtr_STRING(&fps, ".out.dirname"),  FUNCTION_PARAMETER_STRMAXLEN);



    long NBinnerCycleC;
    if(NBinnerCycle < 1)
        NBinnerCycleC = 1;
    else
        NBinnerCycleC = NBinnerCycle;


    printf("TEST POINT LINE %d\n", __LINE__);
    fflush(stdout);

    int SHUFFLE = 1;

    uint32_t act;
    char *ptra;
    char *ptrb;
    char *ptra0;
    char *ptrb0;
    uint64_t pix;
    long IDwfsresp2a, IDwfsresp2b;
    uint64_t wfsxsize, wfsysize, wfsxysize;
    long IDrespC;
    long IDwfsref;

    char pokeC_name[] = "pokeC";
    long IDpokeC = load_fits(pokeC_filename, pokeC_name, 1);
    if(IDpokeC == -1) {
        printf("ERROR: cannot load image %s\n", pokeC_filename);
        exit(0);
    }

    uint32_t dmxsize = data.image[IDpokeC].md[0].size[0];
    uint32_t dmysize = data.image[IDpokeC].md[0].size[1];
    uint32_t dmxysize = dmxsize*dmysize;
    uint64_t NBpoke = data.image[IDpokeC].md[0].size[2];

    int *pokearray = (int*) malloc(sizeof(int)*NBpoke); // shuffled array

    printf("TEST POINT LINE %d    NBpoke = %ld\n", __LINE__, NBpoke);
    fflush(stdout);

    int p;
    for(p=0; p<NBpoke; p++)
        pokearray[p] = p;
    if(SHUFFLE == 1)
    {
        int rindex;

        for(rindex=0; rindex<NBpoke; rindex++)
        {
            int p1;
            int p2;
            int tmpp;

            p1 = (int) (ran1()*NBpoke);
            if(p1>NBpoke-1)
                p1 = NBpoke-1;

            p2 = (int) (ran1()*NBpoke);
            if(p2>NBpoke-1)
                p2 = NBpoke-1;

            tmpp = pokearray[p1];
            pokearray[p1] = pokearray[p2];
            pokearray[p2] = tmpp;

        }
    }

    printf("TEST POINT LINE %d\n", __LINE__);
    fflush(stdout);

    // **************************************************************
    // *************** PREPARE POKE CUBES ***************************
    // **************************************************************

    //
    // Poke cubes will be written to dmpokeC2a and dmpokeC2b




    uint32_t NBpoke2 = 2*NBpoke*NBinnerCycleC + 4; // add zero frame before and after


    long IDpokeC2a = create_3Dimage_ID("dmpokeC2a", dmxsize, dmysize, NBpoke2);
    long IDpokeC2b = create_3Dimage_ID("dmpokeC2b", dmxsize, dmysize, NBpoke2);




    // set start and end frames to zero

    for(act=0; act<dmxysize; act++)
    {
        data.image[IDpokeC2a].array.F[act] = 0.0;
        data.image[IDpokeC2a].array.F[dmxysize + act] = 0.0;
        data.image[IDpokeC2a].array.F[dmxysize*(2*NBpoke*NBinnerCycleC+2) + act] = 0.0;
        data.image[IDpokeC2a].array.F[dmxysize*(2*NBpoke*NBinnerCycleC+3) + act] = 0.0;
    }

    ptra0 = (char*) data.image[IDpokeC2a].array.F;
    ptrb0 = (char*) data.image[IDpokeC2b].array.F;
    memcpy((void *) ptrb0, (void *) ptra0, sizeof(float)*dmxysize);

    ptrb = ptrb0 + sizeof(float)*dmxysize;
    memcpy((void *) ptrb, (void *) ptra0, sizeof(float)*dmxysize);

    ptrb = ptrb0 + sizeof(float)*dmxysize*(2*NBpoke*NBinnerCycleC+2);
    memcpy((void *) ptrb, (void *) ptra0, sizeof(float)*dmxysize);

    ptrb = ptrb0 + sizeof(float)*dmxysize*(2*NBpoke*NBinnerCycleC+2);
    memcpy((void *) ptrb, (void *) ptra0, sizeof(float)*dmxysize);



    int *pokesign = (int*) malloc(sizeof(int)*NBpoke);

    int pokesigntmp = 1;

    int pokeindex;
    pokeindex = 2; // accounts for first two zero pokes

    for(int poke=0; poke<NBpoke; poke++)
    {
        int innercycle;
        for(innercycle=0; innercycle < NBinnerCycleC; innercycle++)
        {
            // note
            // old indices were 2*poke+2 and 2*poke+3

            for(act=0; act<dmxysize; act++)
                data.image[IDpokeC2a].array.F[dmxysize*(pokeindex) + act]   =  ampl*data.image[IDpokeC].array.F[dmxysize*pokearray[poke]+act];
            for(act=0; act<dmxysize; act++)
                data.image[IDpokeC2a].array.F[dmxysize*(pokeindex+1) + act] = -ampl*data.image[IDpokeC].array.F[dmxysize*pokearray[poke]+act];


            // swap one pair out of two in cube IDpokeC2b
            pokesign[poke] = pokesigntmp;
            if(pokesign[poke]==1)  // do not swap
            {
                ptra = ptra0 + sizeof(float)*dmxysize*(pokeindex);
                ptrb = ptrb0 + sizeof(float)*dmxysize*(pokeindex);
                memcpy((void *) ptrb, (void *) ptra, sizeof(float)*dmxysize);

                ptra = ptra0 + sizeof(float)*dmxysize*(pokeindex+1);
                ptrb = ptrb0 + sizeof(float)*dmxysize*(pokeindex+1);
                memcpy((void *) ptrb, (void *) ptra, sizeof(float)*dmxysize);
            }
            else  // do swap
            {
                ptra = ptra0 + sizeof(float)*dmxysize*(pokeindex);
                ptrb = ptrb0 + sizeof(float)*dmxysize*(pokeindex+1);
                memcpy((void *) ptrb, (void *) ptra, sizeof(float)*dmxysize);

                ptra = ptra0 + sizeof(float)*dmxysize*(pokeindex+1);
                ptrb = ptrb0 + sizeof(float)*dmxysize*(pokeindex);
                memcpy((void *) ptrb, (void *) ptra, sizeof(float)*dmxysize);
            }

            pokeindex += 2;
        }

        if(pokesign[poke]==1)
            pokesigntmp = -1;
        else
            pokesigntmp = 1;
        //pokesigntmp = 1; // no inversion
    }

    int retv;
    char command[500];
    sprintf(command, "mkdir -p %s/tmpRMacqu", outdirname);
    retv = system(command);

	char fname[200];
	sprintf(fname, "!%s/tmpRMacqu/test_dmpokeC2a.fits", outdirname);
    save_fits("dmpokeC2a", fname);
    
    sprintf(fname, "!%s/tmpRMacqu/test_dmpokeC2b.fits", outdirname);
    save_fits("dmpokeC2b", fname);


    printf("NBpoke = %ld\n", NBpoke);
    fflush(stdout);

    // ******************************************************************
    // *************** COPY POKE INFO TO tmpRMacqu **********************
    // ******************************************************************

    if( (*FPFLAG_HPOKE) & FPFLAG_ONOFF ) {
        char command[500];

        sprintf(command, "cp %s %s/tmpRMacqu/RMpokeCube.fits", pokeC_filename, outdirname);
        if(system(command) != 0) {
            printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
        }

        sprintf(command, "cp %s %s/RMpokeCube.fits", pokeC_filename, outdirname);
        if(system(command) != 0) {
            printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
        }


        sprintf(command, "cp conf/Hmat.fits %s/tmpRMacqu/RMmat.fits", outdirname);
        if(system(command) != 0) {
            printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
        }

        sprintf(command, "cp conf/Hpixindex.fits %s/tmpRMacqu/RMpixindex.fits", outdirname);
        if(system(command) != 0) {
            printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
        }
    }
    else
    {
        char command[500];

        sprintf(command, "cp %s %s/tmpRMacqu/RMpokeCube.fits", pokeC_filename, outdirname);
        if(system(command) != 0) {
            printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
        }

        sprintf(command, "cp %s %s/RMpokeCube.fits", pokeC_filename, outdirname);
        if(system(command) != 0) {
            printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
        }

    }


    // ******************************************************************
    // *************** EXECUTE POKE SEQUENCES ***************************
    // ******************************************************************

    // Positive direction sequence
    int normalizeflag = 0;
    if( (*FPFLAG_NORMALIZE) & FPFLAG_ONOFF ) {
        normalizeflag = 1;
    }

    AOloopControl_acquireCalib_Measure_WFSrespC(loop, delayfr, delayRM1us, NBave, NBexcl, "dmpokeC2a", "wfsresp2a", normalizeflag, AOinitMode, (long) (NBcycle/2), (uint32_t) 0x02);



    // Negative direction sequence
    AOloopControl_acquireCalib_Measure_WFSrespC(loop, delayfr, delayRM1us, NBave, NBexcl, "dmpokeC2b", "wfsresp2b", normalizeflag, AOinitMode, (long) (NBcycle/2), (uint32_t) 0x02);




    printf("Acquisition complete\n");
    fflush(stdout);



    //	save_fits("wfsresp2", "!tmp/test_wfsresp2.fits");





    // ******************************************************************
    // *************** PROCESS DATA CUBES *******************************
    // ******************************************************************

    // process data cube
    int FileOK = 1;
    int IterNumber = -1;
    int AveStep = 0;

    list_image_ID();

    while(FileOK == 1) // keep processing until there is no more file to process
    {
        char imnameout_respC[100];
        char imnameout_wfsref[100];

        char imnameout_respC_A[100];
        char imnameout_wfsref_A[100];

        char imnameout_respC_B[100];
        char imnameout_wfsref_B[100];


        if(IterNumber == -1) // Process the global average
        {
            printf("Processing global average\n");

            IDwfsresp2a = image_ID("wfsresp2a");
            IDwfsresp2b = image_ID("wfsresp2b");
            sprintf(imnameout_respC, "%s", respC_sname);
            sprintf(imnameout_wfsref, "%s", wfsref_sname);

            sprintf(imnameout_respC_A, "%s_A", respC_sname);
            sprintf(imnameout_wfsref_A, "%s_A", wfsref_sname);

            sprintf(imnameout_respC_B, "%s_B", respC_sname);
            sprintf(imnameout_wfsref_B, "%s_B", wfsref_sname);
        }
        else
        {
            char wfsresp2aname[100];
            char wfsresp2bname[100];
            char tmpfname[500];


            printf("Processing AveStep %3d  Iter %3d ...\n", AveStep, IterNumber);
            fflush(stdout);

            sprintf(wfsresp2aname, "wfsresp2a.snap");
            sprintf(wfsresp2bname, "wfsresp2b.snap");

            delete_image_ID(wfsresp2aname);
            delete_image_ID(wfsresp2bname);

            sprintf(tmpfname, "%s/tmpRMacqu/wfsresp2a.tstep%03d.iter%03d.fits", outdirname, AveStep, IterNumber);
            IDwfsresp2a = load_fits(tmpfname, wfsresp2aname, 1);

            sprintf(tmpfname, "%s/tmpRMacqu/wfsresp2b.tstep%03d.iter%03d.fits", outdirname, AveStep, IterNumber);
            IDwfsresp2b = load_fits(tmpfname, wfsresp2bname, 1);


            sprintf(imnameout_respC, "%s.tstep%03d.iter%03d", respC_sname, AveStep, IterNumber);
            sprintf(imnameout_wfsref, "%s.tstep%03d.iter%03d", wfsref_sname, AveStep, IterNumber);

            sprintf(imnameout_respC_A, "%s_A.tstep%03d.iter%03d", respC_sname, AveStep, IterNumber);
            sprintf(imnameout_wfsref_A, "%s_A.tstep%03d.iter%03d", wfsref_sname, AveStep, IterNumber);

            sprintf(imnameout_respC_B, "%s_B.tstep%03d.iter%03d", respC_sname, AveStep, IterNumber);
            sprintf(imnameout_wfsref_B, "%s_B.tstep%03d.iter%03d", wfsref_sname, AveStep, IterNumber);
        }

        if( (IDwfsresp2a == -1) || (IDwfsresp2b == -1))
        {
            printf("No file(s) to process\n");
            FileOK = 0;
        }
        else
        {
            long IDrespC_A;
            long IDwfsref_A;
            long IDrespC_B;
            long IDwfsref_B;

            printf("Processing files\n");

            wfsxsize = data.image[IDwfsresp2a].md[0].size[0];
            wfsysize = data.image[IDwfsresp2a].md[0].size[1];
            wfsxysize = wfsxsize*wfsysize;

            IDrespC = create_3Dimage_ID(imnameout_respC, wfsxsize, wfsysize, NBpoke);
            IDwfsref = create_2Dimage_ID(imnameout_wfsref, wfsxsize, wfsysize);

            IDrespC_A = create_3Dimage_ID(imnameout_respC_A, wfsxsize, wfsysize, NBpoke);
            IDwfsref_A = create_2Dimage_ID(imnameout_wfsref_A, wfsxsize, wfsysize);

            IDrespC_B = create_3Dimage_ID(imnameout_respC_B, wfsxsize, wfsysize, NBpoke);
            IDwfsref_B = create_2Dimage_ID(imnameout_wfsref_B, wfsxsize, wfsysize);

            pokeindex = 2;
            for(int poke=0; poke<NBpoke; poke++)
            {
                int innercycle;
                for(innercycle=0; innercycle < NBinnerCycleC; innercycle++)
                {
                    float valA;
                    float valB;

                    // Sum response
                    for(pix=0; pix<wfsxysize; pix++)
                    {
                        // pattern A
                        valA = (data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex) + pix] - data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex+1) + pix])/2.0/ampl/NBinnerCycleC;
                        data.image[IDrespC].array.F[wfsxysize*pokearray[poke] + pix] += 0.5*valA;
                        data.image[IDrespC_A].array.F[wfsxysize*pokearray[poke] + pix] += 1.0*valA;

                        // pattern B
                        valB = pokesign[poke]*(data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex) + pix] - data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex+1) + pix])/2.0/ampl/NBinnerCycleC;
                        data.image[IDrespC].array.F[wfsxysize*pokearray[poke] + pix] += 0.5*valB;
                        data.image[IDrespC_B].array.F[wfsxysize*pokearray[poke] + pix] += 1.0*valB;
                    }

                    // Sum reference
                    for(pix=0; pix<wfsxysize; pix++)
                    {
                        // pattern A
                        data.image[IDwfsref].array.F[pix] += (data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex) + pix] + data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex+1) + pix])/(2*NBpoke)/NBinnerCycleC;

                        data.image[IDwfsref_A].array.F[pix] += 2.0*(data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex) + pix] + data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex+1) + pix])/(2*NBpoke)/NBinnerCycleC;

                        // pattern B
                        data.image[IDwfsref].array.F[pix] += (data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex) + pix] + data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex+1) + pix])/(2*NBpoke)/NBinnerCycleC;

                        data.image[IDwfsref_B].array.F[pix] += 2.0*(data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex) + pix] + data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex+1) + pix])/(2*NBpoke)/NBinnerCycleC;
                    }

                    pokeindex += 2;
                }

            }


            if(IterNumber > -1)
            {
                char filename_respC[100];
                char filename_wfsref[100];


                printf(" [saving ... ");
                fflush(stdout);

                sprintf(filename_respC, "!%s/tmpRMacqu/respM.tstep%03d.iter%03d.fits", outdirname, AveStep, IterNumber);
                sprintf(filename_wfsref, "!%s/tmpRMacqu/wfsref.tstep%03d.iter%03d.fits", outdirname, AveStep, IterNumber);
                save_fits(imnameout_respC, filename_respC);
                save_fits(imnameout_wfsref, filename_wfsref);
                delete_image_ID(imnameout_respC);
                delete_image_ID(imnameout_wfsref);

                sprintf(filename_respC, "!%s/tmpRMacqu/respM_A.tstep%03d.iter%03d.fits", outdirname, AveStep, IterNumber);
                sprintf(filename_wfsref, "!%s/tmpRMacqu/wfsref_A.tstep%03d.iter%03d.fits", outdirname, AveStep, IterNumber);
                save_fits(imnameout_respC_A, filename_respC);
                save_fits(imnameout_wfsref_A, filename_wfsref);
                delete_image_ID(imnameout_respC_A);
                delete_image_ID(imnameout_wfsref_A);

                sprintf(filename_respC, "!%s/tmpRMacqu/respM_B.tstep%03d.iter%03d.fits", outdirname, AveStep, IterNumber);
                sprintf(filename_wfsref, "!%s/tmpRMacqu/wfsref_B.tstep%03d.iter%03d.fits", outdirname, AveStep, IterNumber);
                save_fits(imnameout_respC_B, filename_respC);
                save_fits(imnameout_wfsref_B, filename_wfsref);
                delete_image_ID(imnameout_respC_B);
                delete_image_ID(imnameout_wfsref_B);

                printf("done] \n");
                fflush(stdout);

            }
            else // save globals
            {
                char filename_respC[100];
                char filename_wfsref[100];

                sprintf(imnameout_respC, "%s", respC_sname);
                sprintf(filename_respC, "!%s/tmpRMacqu/respM.fits", outdirname);
                save_fits(imnameout_respC, filename_respC);
                sprintf(filename_respC, "!%s/respM.fits", outdirname);
                save_fits(imnameout_respC, filename_respC);

                sprintf(imnameout_wfsref, "%s", wfsref_sname);
                sprintf(filename_wfsref, "!%s/tmpRMacqu/wfsref.fits", outdirname);
                save_fits(imnameout_wfsref, filename_wfsref);
                sprintf(filename_wfsref, "!%s/wfsref.fits", outdirname);
                save_fits(imnameout_wfsref, filename_wfsref);

                sprintf(imnameout_respC_A, "%s_A", respC_sname);
                sprintf(filename_respC, "!%s/tmpRMacqu/respM_A.fits", outdirname);
                save_fits(imnameout_respC_A, filename_respC);

                sprintf(imnameout_wfsref_A, "%s_A", wfsref_sname);
                sprintf(filename_wfsref, "!%s/tmpRMacqu/wfsref_A.fits", outdirname);
                save_fits(imnameout_wfsref_A, filename_wfsref);


                sprintf(imnameout_respC_B, "%s_A", respC_sname);
                sprintf(filename_respC, "!%s/tmpRMacqu/respM_A.fits", outdirname);
                save_fits(imnameout_respC_B, filename_respC);

                sprintf(imnameout_wfsref_B, "%s_A", wfsref_sname);
                sprintf(filename_wfsref, "!%s/tmpRMacqu/wfsref_B.fits", outdirname);
                save_fits(imnameout_wfsref_B, filename_wfsref);




                IterNumber = 0; // start processing iterations
            }

            FileOK = 0; // do not process individual files

            AveStep ++;
            if(AveStep==NBave) {
                AveStep = 0;
                IterNumber ++;
            }
        }

    }




    free(pokesign);
    free(pokearray);

    // run RM decode exec script
    // 
    
    sprintf(command, "%s %s", execRMdecode, fpsname);
    if(system(command) != 0) {
        printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
    }

	sprintf(command, "%s %s", execmkDMWFSmasks, fpsname);
    if(system(command) != 0) {
        printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
    }

	sprintf(command, "%s %s", execmkDMslaveact, fpsname);
    if(system(command) != 0) {
        printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
    }

	sprintf(command, "%s %s", execmkLODMmodes, fpsname);
    if(system(command) != 0) {
        printERROR(__FILE__,__func__,__LINE__, "system() returns non-zero value");
    }


	function_parameter_RUNexit( &fps, &SMfd );

    list_image_ID();

    return RETURN_SUCCESS;
}




long AOloopControl_acquireCalib_Measure_WFS_linResponse(
    long        loop,
    float       ampl,
    long        delayfr,          /// Frame delay [# of frame]
    long        delayRM1us,       /// Sub-frame delay [us]
    long        NBave,            /// Number of frames averaged for a single poke measurement
    long        NBexcl,           /// Number of frames excluded
    const char *IDpokeC_name,
    const char *IDrespC_name,
    const char *IDwfsref_name,
    int         normalize,
    int         AOinitMode,
    long        NBcycle,         /// Number of measurement cycles to be repeated
    long        NBinnerCycle     /// Number of inner cycles (how many consecutive times should a single +/- poke be repeated)
)
{
	char fpsname[200];

    long pindex = (long) getpid();  // index used to differentiate multiple calls to function
    // if we don't have anything more informative, we use PID
	
	int SMfd = -1;
    FUNCTION_PARAMETER_STRUCT fps;

    // create FPS
    sprintf(fpsname, "linresp-%06ld", pindex);
    AOcontrolLoop_acquireCalib_Measure_WFS_linResponse_FPCONF(fpsname, CMDCODE_FPSINIT);

    function_parameter_struct_connect(fpsname, &fps, FPSCONNECT_SIMPLE, &SMfd);

//    functionparameter_SetParamValue_INT64(&fps, ".arg0", arg0);

    function_parameter_struct_disconnect(&fps, &SMfd);

    AOcontrolLoop_acquireCalib_Measure_WFS_linResponse_RUN(fpsname);

	return(0);
}








/**
 * ## Purpose
 *
 *  Measure the WFS linear response to a set of DM patterns
 *
 * This function creates positive and negative maps which are sent by AOloopControl_acquireCalib_Measure_WFSrespC() \n
 * Bleeding from one pattern to the next due to limited DM time response is canceled by using two patterns \n
 *
 *
 * pattern a sequence : +- +- +- +- ... \n
 * pattern b sequence : +- -+ +- -+ ... \n
 *
 */
long AOloopControl_acquireCalib_Measure_WFS_linResponse_old(
    long        loop,
    float       ampl,
    long        delayfr,          /// Frame delay [# of frame]
    long        delayRM1us,       /// Sub-frame delay [us]
    long        NBave,            /// Number of frames averaged for a single poke measurement
    long        NBexcl,           /// Number of frames excluded
    const char *IDpokeC_name,
    const char *IDrespC_name,
    const char *IDwfsref_name,
    int         normalize,
    int         AOinitMode,
    long        NBcycle,         /// Number of measurement cycles to be repeated
    long        NBinnerCycle     /// Number of inner cycles (how many consecutive times should a single +/- poke be repeated)
)
{
    long IDrespC;
    long IDpokeC;
    long dmxsize, dmysize, dmxysize;
    long wfsxsize, wfsysize, wfsxysize;
    long NBpoke, NBpoke2;
    long IDpokeC2a, IDpokeC2b; // poke sequences a, b used to remove time bleeding effects in linear regime
    long IDwfsresp2a, IDwfsresp2b;
    long poke, act, pix;
    long IDwfsref;

    int *pokesign;
    int pokesigntmp;
    char *ptra;
    char *ptrb;
    char *ptra0;
    char *ptrb0;


    long NBinnerCycleC;
    if(NBinnerCycle < 1)
        NBinnerCycleC = 1;
    else
        NBinnerCycleC = NBinnerCycle;



    int SHUFFLE = 1;


    printf("Entering function %s \n", __FUNCTION__);
    fflush(stdout);

    IDpokeC = image_ID(IDpokeC_name);
    dmxsize = data.image[IDpokeC].md[0].size[0];
    dmysize = data.image[IDpokeC].md[0].size[1];
    dmxysize = dmxsize*dmysize;
    NBpoke = data.image[IDpokeC].md[0].size[2];

    int *pokearray = (int*) malloc(sizeof(int)*NBpoke); // shuffled array


    int p;
    for(p=0; p<NBpoke; p++)
        pokearray[p] = p;
    if(SHUFFLE == 1)
    {
        int rindex;

        for(rindex=0; rindex<NBpoke; rindex++)
        {
            int p1;
            int p2;
            int tmpp;

            p1 = (int) (ran1()*NBpoke);
            if(p1>NBpoke-1)
                p1 = NBpoke-1;

            p2 = (int) (ran1()*NBpoke);
            if(p2>NBpoke-1)
                p2 = NBpoke-1;

            tmpp = pokearray[p1];
            pokearray[p1] = pokearray[p2];
            pokearray[p2] = tmpp;

        }
    }


    // **************************************************************
    // *************** PREPARE POKE CUBES ***************************
    // **************************************************************

    //
    // Poke cubes will be written to dmpokeC2a and dmpokeC2b




    NBpoke2 = 2*NBpoke*NBinnerCycleC + 4; // add zero frame before and after


    IDpokeC2a = create_3Dimage_ID("dmpokeC2a", dmxsize, dmysize, NBpoke2);
    IDpokeC2b = create_3Dimage_ID("dmpokeC2b", dmxsize, dmysize, NBpoke2);




    // set start and end frames to zero

    for(act=0; act<dmxysize; act++)
    {
        data.image[IDpokeC2a].array.F[act] = 0.0;
        data.image[IDpokeC2a].array.F[dmxysize + act] = 0.0;
        data.image[IDpokeC2a].array.F[dmxysize*(2*NBpoke*NBinnerCycleC+2) + act] = 0.0;
        data.image[IDpokeC2a].array.F[dmxysize*(2*NBpoke*NBinnerCycleC+3) + act] = 0.0;
    }

    ptra0 = (char*) data.image[IDpokeC2a].array.F;
    ptrb0 = (char*) data.image[IDpokeC2b].array.F;
    memcpy((void *) ptrb0, (void *) ptra0, sizeof(float)*dmxysize);

    ptrb = ptrb0 + sizeof(float)*dmxysize;
    memcpy((void *) ptrb, (void *) ptra0, sizeof(float)*dmxysize);

    ptrb = ptrb0 + sizeof(float)*dmxysize*(2*NBpoke*NBinnerCycleC+2);
    memcpy((void *) ptrb, (void *) ptra0, sizeof(float)*dmxysize);

    ptrb = ptrb0 + sizeof(float)*dmxysize*(2*NBpoke*NBinnerCycleC+2);
    memcpy((void *) ptrb, (void *) ptra0, sizeof(float)*dmxysize);



    pokesign = (int*) malloc(sizeof(int)*NBpoke);

    pokesigntmp = 1;

    int pokeindex;
    pokeindex = 2; // accounts for first two zero pokes

    for(poke=0; poke<NBpoke; poke++)
    {
        int innercycle;
        for(innercycle=0; innercycle < NBinnerCycleC; innercycle++)
        {
            // note
            // old indices were 2*poke+2 and 2*poke+3

            for(act=0; act<dmxysize; act++)
                data.image[IDpokeC2a].array.F[dmxysize*(pokeindex) + act]   =  ampl*data.image[IDpokeC].array.F[dmxysize*pokearray[poke]+act];
            for(act=0; act<dmxysize; act++)
                data.image[IDpokeC2a].array.F[dmxysize*(pokeindex+1) + act] = -ampl*data.image[IDpokeC].array.F[dmxysize*pokearray[poke]+act];


            // swap one pair out of two in cube IDpokeC2b
            pokesign[poke] = pokesigntmp;
            if(pokesign[poke]==1)  // do not swap
            {
                ptra = ptra0 + sizeof(float)*dmxysize*(pokeindex);
                ptrb = ptrb0 + sizeof(float)*dmxysize*(pokeindex);
                memcpy((void *) ptrb, (void *) ptra, sizeof(float)*dmxysize);

                ptra = ptra0 + sizeof(float)*dmxysize*(pokeindex+1);
                ptrb = ptrb0 + sizeof(float)*dmxysize*(pokeindex+1);
                memcpy((void *) ptrb, (void *) ptra, sizeof(float)*dmxysize);
            }
            else  // do swap
            {
                ptra = ptra0 + sizeof(float)*dmxysize*(pokeindex);
                ptrb = ptrb0 + sizeof(float)*dmxysize*(pokeindex+1);
                memcpy((void *) ptrb, (void *) ptra, sizeof(float)*dmxysize);

                ptra = ptra0 + sizeof(float)*dmxysize*(pokeindex+1);
                ptrb = ptrb0 + sizeof(float)*dmxysize*(pokeindex);
                memcpy((void *) ptrb, (void *) ptra, sizeof(float)*dmxysize);
            }

            pokeindex += 2;
        }

        if(pokesign[poke]==1)
            pokesigntmp = -1;
        else
            pokesigntmp = 1;
        //pokesigntmp = 1; // no inversion
    }

    int retv;
    retv = system("mkdir -p tmpRMacqu");


    save_fits("dmpokeC2a", "!tmpRMacqu/test_dmpokeC2a.fits");
    save_fits("dmpokeC2b", "!tmpRMacqu/test_dmpokeC2b.fits");


    printf("NBpoke = %ld\n", NBpoke);
    fflush(stdout);





    // ******************************************************************
    // *************** EXECUTE POKE SEQUENCES ***************************
    // ******************************************************************

    // Positive direction sequence
    AOloopControl_acquireCalib_Measure_WFSrespC(loop, delayfr, delayRM1us, NBave, NBexcl, "dmpokeC2a", "wfsresp2a", normalize, AOinitMode, (long) (NBcycle/2), (uint32_t) 0x02);



    // Negative direction sequence
    AOloopControl_acquireCalib_Measure_WFSrespC(loop, delayfr, delayRM1us, NBave, NBexcl, "dmpokeC2b", "wfsresp2b", normalize, AOinitMode, (long) (NBcycle/2), (uint32_t) 0x02);




    printf("Acquisition complete\n");
    fflush(stdout);



    //	save_fits("wfsresp2", "!tmp/test_wfsresp2.fits");





    // ******************************************************************
    // *************** PROCESS DATA CUBES *******************************
    // ******************************************************************

    // process data cube
    int FileOK = 1;
    int IterNumber = -1;
    int AveStep = 0;

    while(FileOK == 1) // keep processing until there is no more file to process
    {
        char imnameout_respC[100];
        char imnameout_wfsref[100];

        char imnameout_respC_A[100];
        char imnameout_wfsref_A[100];

        char imnameout_respC_B[100];
        char imnameout_wfsref_B[100];


        if(IterNumber == -1) // Process the global average
        {
            IDwfsresp2a = image_ID("wfsresp2a");
            IDwfsresp2b = image_ID("wfsresp2b");
            sprintf(imnameout_respC, "%s", IDrespC_name);
            sprintf(imnameout_wfsref, "%s", IDwfsref_name);

            sprintf(imnameout_respC_A, "%s_A", IDrespC_name);
            sprintf(imnameout_wfsref_A, "%s_A", IDwfsref_name);
            
            sprintf(imnameout_respC_B, "%s_B", IDrespC_name);
            sprintf(imnameout_wfsref_B, "%s_B", IDwfsref_name);            
        }
        else
        {
            char wfsresp2aname[100];
            char wfsresp2bname[100];
            char tmpfname[500];
            
            
            printf("Processing AveStep %3d  Iter %3d ...\n", AveStep, IterNumber);
            fflush(stdout);

            sprintf(wfsresp2aname, "wfsresp2a.snap");
            sprintf(wfsresp2bname, "wfsresp2b.snap");
            
            delete_image_ID(wfsresp2aname);
            delete_image_ID(wfsresp2bname);

            sprintf(tmpfname, "tmpRMacqu/wfsresp2a.tstep%03d.iter%03d.fits", AveStep, IterNumber);
            IDwfsresp2a = load_fits(tmpfname, wfsresp2aname, 1);

            sprintf(tmpfname, "tmpRMacqu/wfsresp2b.tstep%03d.iter%03d.fits", AveStep, IterNumber);
            IDwfsresp2b = load_fits(tmpfname, wfsresp2bname, 1);


            sprintf(imnameout_respC, "%s.tstep%03d.iter%03d", IDrespC_name, AveStep, IterNumber);
            sprintf(imnameout_wfsref, "%s.tstep%03d.iter%03d", IDwfsref_name, AveStep, IterNumber);

            sprintf(imnameout_respC_A, "%s_A.tstep%03d.iter%03d", IDrespC_name, AveStep, IterNumber);
            sprintf(imnameout_wfsref_A, "%s_A.tstep%03d.iter%03d", IDwfsref_name, AveStep, IterNumber);
            
            sprintf(imnameout_respC_B, "%s_B.tstep%03d.iter%03d", IDrespC_name, AveStep, IterNumber);
            sprintf(imnameout_wfsref_B, "%s_B.tstep%03d.iter%03d", IDwfsref_name, AveStep, IterNumber);
        }

        if( (IDwfsresp2a == -1) || (IDwfsresp2b == -1))
        {
            FileOK = 0;
        }
        else
        {
			long IDrespC_A;
			long IDwfsref_A;
			long IDrespC_B;
			long IDwfsref_B;
			
            wfsxsize = data.image[IDwfsresp2a].md[0].size[0];
            wfsysize = data.image[IDwfsresp2a].md[0].size[1];
            wfsxysize = wfsxsize*wfsysize;

            IDrespC = create_3Dimage_ID(imnameout_respC, wfsxsize, wfsysize, NBpoke);
            IDwfsref = create_2Dimage_ID(imnameout_wfsref, wfsxsize, wfsysize);

            IDrespC_A = create_3Dimage_ID(imnameout_respC_A, wfsxsize, wfsysize, NBpoke);
            IDwfsref_A = create_2Dimage_ID(imnameout_wfsref_A, wfsxsize, wfsysize);

            IDrespC_B = create_3Dimage_ID(imnameout_respC_B, wfsxsize, wfsysize, NBpoke);
            IDwfsref_B = create_2Dimage_ID(imnameout_wfsref_B, wfsxsize, wfsysize);

            pokeindex = 2;
            for(poke=0; poke<NBpoke; poke++)
            {
                int innercycle;
                for(innercycle=0; innercycle < NBinnerCycleC; innercycle++)
                {
					float valA;
					float valB;
					
                    // Sum response
                    for(pix=0; pix<wfsxysize; pix++)
                    {
                        // pattern A
                        valA = (data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex) + pix] - data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex+1) + pix])/2.0/ampl/NBinnerCycleC;
                        data.image[IDrespC].array.F[wfsxysize*pokearray[poke] + pix] += 0.5*valA;
                        data.image[IDrespC_A].array.F[wfsxysize*pokearray[poke] + pix] += 1.0*valA;
                                                
                        // pattern B
                        valB = pokesign[poke]*(data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex) + pix] - data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex+1) + pix])/2.0/ampl/NBinnerCycleC;
                        data.image[IDrespC].array.F[wfsxysize*pokearray[poke] + pix] += 0.5*valB;
                        data.image[IDrespC_B].array.F[wfsxysize*pokearray[poke] + pix] += 1.0*valB;                        
                    }

                    // Sum reference
                    for(pix=0; pix<wfsxysize; pix++)
                    {
						// pattern A
                        data.image[IDwfsref].array.F[pix] += (data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex) + pix] + data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex+1) + pix])/(2*NBpoke)/NBinnerCycleC;
                        
                        data.image[IDwfsref_A].array.F[pix] += 2.0*(data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex) + pix] + data.image[IDwfsresp2a].array.F[wfsxysize*(pokeindex+1) + pix])/(2*NBpoke)/NBinnerCycleC;
                        
                        // pattern B
                        data.image[IDwfsref].array.F[pix] += (data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex) + pix] + data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex+1) + pix])/(2*NBpoke)/NBinnerCycleC;
                        
                        data.image[IDwfsref_B].array.F[pix] += 2.0*(data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex) + pix] + data.image[IDwfsresp2b].array.F[wfsxysize*(pokeindex+1) + pix])/(2*NBpoke)/NBinnerCycleC;
                    }

                    pokeindex += 2;
                }

            }


            if(IterNumber > -1)
            {
                char filename_respC[100];
                char filename_wfsref[100];


				printf(" [saving ... ");
				fflush(stdout);

                sprintf(filename_respC, "!tmpRMacqu/respM.tstep%03d.iter%03d.fits", AveStep, IterNumber);
                sprintf(filename_wfsref, "!tmpRMacqu/wfsref.tstep%03d.iter%03d.fits", AveStep, IterNumber);
                save_fits(imnameout_respC, filename_respC);
                save_fits(imnameout_wfsref, filename_wfsref);                
                delete_image_ID(imnameout_respC);
                delete_image_ID(imnameout_wfsref);
                
                sprintf(filename_respC, "!tmpRMacqu/respM_A.tstep%03d.iter%03d.fits", AveStep, IterNumber);
                sprintf(filename_wfsref, "!tmpRMacqu/wfsref_A.tstep%03d.iter%03d.fits", AveStep, IterNumber);
                save_fits(imnameout_respC_A, filename_respC);
                save_fits(imnameout_wfsref_A, filename_wfsref);                
                delete_image_ID(imnameout_respC_A);
                delete_image_ID(imnameout_wfsref_A);                
                
                sprintf(filename_respC, "!tmpRMacqu/respM_B.tstep%03d.iter%03d.fits", AveStep, IterNumber);
                sprintf(filename_wfsref, "!tmpRMacqu/wfsref_B.tstep%03d.iter%03d.fits", AveStep, IterNumber);
                save_fits(imnameout_respC_B, filename_respC);
                save_fits(imnameout_wfsref_B, filename_wfsref);                
                delete_image_ID(imnameout_respC_B);
                delete_image_ID(imnameout_wfsref_B);
                
                printf("done] \n");
                fflush(stdout);
                
            }
            else
                IterNumber = 0; // start processing iterations

			FileOK = 0; // do not process individual files

            AveStep ++;
            if(AveStep==NBave) {
                AveStep = 0;
                IterNumber ++;
            }
        }

    }


    free(pokesign);
    free(pokearray);

    return(IDrespC);
}







/** Measures zonal response matrix
 * -> collapses it to DM response map and WFS response map
 * (both maps show amplitude of actuator effect on WFS)
 *
 * mode :
 *  0: compute WFSmap and DMmap
 *  1: compute WFSmap, DMmap, WFSmask and DMmask  -> images wfsmask and dmmask
 * NOTE can take custom poke matrix (loaded in image name RMpokeCube)
 *
 * ASYNC = 1  -> record ALL frames and assemble the RM off-line
 *
 * AOinitMode = 0:  create AO shared mem struct
 * AOinitMode = 1:  connect only to AO shared mem struct
 *  */

long AOloopControl_acquireCalib_Measure_zonalRM(
    long loop,
    double ampl,
    long delayfr,
    long delayRM1us,
    long NBave,
    long NBexcl,
    const char *zrespm_name,
    const char *WFSref0_name,
    const char *WFSmap_name,
    const char *DMmap_name,
    long mode,
    int normalize,
    int AOinitMode,
    long NBcycle
)
{
    long ID_WFSmap, ID_WFSref0, ID_WFSref2, ID_DMmap, IDmapcube, IDzrespm, IDzrespmn, ID_WFSref0n,  ID_WFSref2n;
    long act, j, ii, kk;
    double value;
    float *arrayf;
    char fname[200];
    char name[200];
    char command[200];
    long IDpos, IDneg;
    float tot, v1, rms;
    uint32_t *sizearray;

    long NBiter = LONG_MAX; // runs until USR1 signal received
    long iter;
    float *arraypix;
    long i;
    long istart, iend, icnt;
    long cntn;
    double tmpv;

    long ID_WFSmask, ID_DMmask;
    float lim;
    double total;
    int r;
    long IDzrespfp, IDzrespfm;
    long IDpokeC;
    long NBpoke;

    int RT_priority = 80; //any number from 0-99
    struct sched_param schedpar;
    int ret;
    long act2;

    long *actarray;
    long poke, poke1, poke2;




    schedpar.sched_priority = RT_priority;
#ifndef __MACH__
    sched_setscheduler(0, SCHED_FIFO, &schedpar);
#endif


    if(NBcycle < 1)
        NBiter = LONG_MAX; // runs until USR1 signal received
    else
        NBiter = NBcycle;



    arraypix = (float*) malloc(sizeof(float)*NBiter);
    sizearray = (uint32_t*) malloc(sizeof(uint32_t)*3);





    printf("INITIALIZE MEMORY (mode %d)....\n", AOinitMode);
    fflush(stdout);



    if(AOloopcontrol_meminit==0)
        AOloopControl_InitializeMemory(AOinitMode);
    fflush(stdout);



    //  sprintf(fname, "./conf/AOloop.conf");

    printf("LOAD/CONFIGURE loop ...\n");
    fflush(stdout);


    AOloopControl_loadconfigure(LOOPNUMBER, 1, 2);



    printf("Importing DM response matrix channel shared memory ...\n");
    fflush(stdout);
    aoloopcontrol_var.aoconfID_dmRM = read_sharedmem_image(AOconf[loop].DMctrl.dmRMname);



    printf("Importing WFS camera image shared memory ... \n");
    fflush(stdout);
    aoloopcontrol_var.aoconfID_wfsim = read_sharedmem_image(AOconf[loop].WFSim.WFSname);



    if(sprintf(name, "aol%ld_imWFS1RM", loop) < 1)
        printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");

    sizearray[0] = AOconf[loop].WFSim.sizexWFS;
    sizearray[1] = AOconf[loop].WFSim.sizeyWFS;
    printf("WFS size = %ld %ld\n", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS);
    fflush(stdout);
    aoloopcontrol_var.aoconfID_imWFS1 = create_image_ID(name, 2, sizearray, _DATATYPE_FLOAT, 1, 0);


    arrayf = (float*) malloc(sizeof(float)*AOconf[loop].DMctrl.sizeDM);

    sizearray[0] = AOconf[loop].DMctrl.sizexDM;
    sizearray[1] = AOconf[loop].DMctrl.sizeyDM;
    ID_DMmap = create_image_ID(DMmap_name, 2, sizearray, _DATATYPE_FLOAT, 1, 5);


    IDpos = create_2Dimage_ID("wfsposim", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS);
    IDneg = create_2Dimage_ID("wfsnegim", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS);



    IDpokeC = image_ID("RMpokeCube");
    if(IDpokeC==-1)
    {
        IDpokeC = create_3Dimage_ID("RMpokeCube", AOconf[loop].DMctrl.sizexDM, AOconf[loop].DMctrl.sizeyDM, AOconf[loop].DMctrl.sizexDM*AOconf[loop].DMctrl.sizeyDM);
        for(act=0; act<AOconf[loop].DMctrl.sizexDM*AOconf[loop].DMctrl.sizeyDM; act++)
        {
            for(ii=0; ii<AOconf[loop].DMctrl.sizexDM*AOconf[loop].DMctrl.sizeyDM; ii++)
                data.image[IDpokeC].array.F[act*AOconf[loop].DMctrl.sizexDM*AOconf[loop].DMctrl.sizeyDM+ii] = 0.0;
            data.image[IDpokeC].array.F[act*AOconf[loop].DMctrl.sizexDM*AOconf[loop].DMctrl.sizeyDM+act] = 1.0;
        }
        //        save_fits("RMpokeCube", "!./conf/RMpokeCube.fits");
        save_fits("RMpokeCube", "!./conf/zRMpokeCube.fits");

        NBpoke = data.image[IDpokeC].md[0].size[2];
    }
    else
    {
        //NBpoke = AOconf[loop].DMctrl.sizeDM;
        NBpoke = data.image[IDpokeC].md[0].size[2];
    }

    //    save_fits("RMpokeCube", "!./conf/test1_RMpokeCube.fits");

    if(sprintf(command, "echo \"%ld\" > RM_NBpoke.txt\n", NBpoke) < 1)
        printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");
    if(system(command) != 0)
        printERROR(__FILE__, __func__, __LINE__, "system() returns non-zero value");

    if(sprintf(command, "echo \"%ld\" > test_RM_NBpoke.txt\n", NBpoke) < 1)
        printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");
    if(system(command) != 0)
        printERROR(__FILE__, __func__, __LINE__, "system() returns non-zero value");

    //    sleep(10);

    sizearray[0] = AOconf[loop].WFSim.sizexWFS;
    sizearray[1] = AOconf[loop].WFSim.sizeyWFS;
    sizearray[2] = NBpoke; //AOconf[loop].DMctrl.sizeDM;

    actarray = (long*) malloc(sizeof(long)*NBpoke);

    ID_WFSmap = create_image_ID(WFSmap_name, 2, sizearray, _DATATYPE_FLOAT, 1, 5);
    ID_WFSref0 = create_image_ID("tmpwfsref0", 2, sizearray, _DATATYPE_FLOAT, 1, 5);
    ID_WFSref2 = create_image_ID("tmpwfsref2", 2, sizearray, _DATATYPE_FLOAT, 1, 5);
    ID_WFSref0n = create_image_ID(WFSref0_name, 2, sizearray, _DATATYPE_FLOAT, 1, 5);
    ID_WFSref2n = create_image_ID("tmpwfsimrms", 2, sizearray, _DATATYPE_FLOAT, 1, 5);
    IDzrespm = create_image_ID("zrespm", 3, sizearray, _DATATYPE_FLOAT, 0, 5); // Zonal response matrix
    IDzrespmn = create_image_ID(zrespm_name, 3, sizearray, _DATATYPE_FLOAT, 0, 5); // Zonal response matrix normalized

    IDzrespfp = create_image_ID("zrespfp", 3, sizearray, _DATATYPE_FLOAT, 0, 5); // positive poke image
    IDzrespfm = create_image_ID("zrespfm", 3, sizearray, _DATATYPE_FLOAT, 0, 5); // negative poke image

    if(mode>0)
    {
        sizearray[0] = AOconf[loop].WFSim.sizexWFS;
        sizearray[1] = AOconf[loop].WFSim.sizeyWFS;
        ID_WFSmask = create_image_ID("wfsmask", 2, sizearray, _DATATYPE_FLOAT, 1, 5);

        sizearray[0] = AOconf[loop].DMctrl.sizexDM;
        sizearray[1] = AOconf[loop].DMctrl.sizeyDM;
        ID_DMmask = create_image_ID("dmmask", 2, sizearray, _DATATYPE_FLOAT, 1, 5);
    }


    cntn = 0;
    iter = 0;


    printf("Clearing directory files\n");
    fflush(stdout);

    //    for(iter=0; iter<NBiter; iter++)
    if(system("mkdir -p zresptmp") != 0)
        printERROR(__FILE__, __func__, __LINE__, "system() returns non-zero value");

    if(system("rm ./zresptmp/LO*.fits") != 0)
        printERROR(__FILE__, __func__, __LINE__, "system() returns non-zero value");

    if(sprintf(command, "echo %ld > ./zresptmp/%s_nbiter.txt", iter, zrespm_name) < 1)
        printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");

    if(system(command) != 0)
        printERROR(__FILE__, __func__, __LINE__, "system() returns non-zero value");


    printf("STARTING RM...\n");
    fflush(stdout);

    while((iter<NBiter)&&(data.signal_USR1==0))
    {
        printf("iteration # %8ld    \n", iter);
        fflush(stdout);


        // permut actarray
        for(poke=0; poke<NBpoke; poke++)
            actarray[poke] = poke;

        for(poke=0; poke<NBpoke; poke++)
        {
            poke1 = (long) (ran1()*NBpoke);
            if(poke1>=NBpoke)
                poke1 = NBpoke-1;
            if(poke!=poke1)
            {
                poke2 = actarray[poke1];
                actarray[poke1] = actarray[poke];
                actarray[poke] = poke2;
            }
        }



        for(poke=0; poke<NBpoke; poke++)
            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                data.image[IDzrespm].array.F[poke*AOconf[loop].WFSim.sizeWFS+ii] = 0.0;


        act = 0;

        long kk1 = 0;
        int PokeSign = 1;
        long act1 = 0;



        // initialize with first positive poke
        for(j=0; j<AOconf[loop].DMctrl.sizeDM; j++)
            arrayf[j] = ampl*data.image[IDpokeC].array.F[actarray[act1]*AOconf[loop].DMctrl.sizeDM+j];


        usleep(delayRM1us);
        data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].write = 1;
        memcpy (data.image[aoloopcontrol_var.aoconfID_dmRM].array.F, arrayf, sizeof(float)*AOconf[loop].DMctrl.sizeDM);
        data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].cnt0++;
        data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].write = 0;
        AOconf[loop].aorun.DMupdatecnt ++;



        // WAIT FOR LOOP DELAY, PRIMING
        Read_cam_frame(loop, 1, normalize, 0, 1);

        // read delayfr frames
        for(kk=0; kk<delayfr; kk++)
        {
            Read_cam_frame(loop, 1, normalize, 0, 0);
            kk1++;
            if(kk1==NBave)
            {
                kk1 = -NBexcl;
                if(PokeSign==1)
                    PokeSign = -1;
                else
                {
                    act1++;
                    PokeSign = 1;
                }

                if(act1>NBpoke-1)
                    act1 = NBpoke-1;
                // POKE
                for(j=0; j<AOconf[loop].DMctrl.sizeDM; j++)
                    arrayf[j] = ampl*PokeSign*data.image[IDpokeC].array.F[actarray[act1]*AOconf[loop].DMctrl.sizeDM+j];

                usleep(delayRM1us);
                data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].write = 1;
                memcpy (data.image[aoloopcontrol_var.aoconfID_dmRM].array.F, arrayf, sizeof(float)*AOconf[loop].DMctrl.sizeDM);
                data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].cnt0++;
                data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].write = 0;
                AOconf[loop].aorun.DMupdatecnt ++;
            }
        }





        while ((act < NBpoke)&&(data.signal_USR1==0))
        {
            //	printf("act = %6ld   NBpoke = %6ld\n", act, NBpoke);
            //	fflush(stdout);
            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
            {
                data.image[IDpos].array.F[ii] = 0.0;
                data.image[IDneg].array.F[ii] = 0.0;
            }

            // POSITIVE INTEGRATION
            //  printf("POSITIVE INTEGRATION\n");
            //  fflush(stdout);
            for(kk=0; kk<NBave+NBexcl; kk++)
            {
                Read_cam_frame(loop, 1, normalize, 0, 0);
                if(kk<NBave)
                    for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                        data.image[IDpos].array.F[ii] += data.image[aoloopcontrol_var.aoconfID_imWFS1].array.F[ii];
                kk1++;
                if(kk1==NBave)
                {
                    kk1 = -NBexcl;
                    if(PokeSign==1)
                        PokeSign = -1;
                    else
                    {
                        act1++;
                        PokeSign = 1;
                    }
                    if(act1>NBpoke-1)
                        act1 = NBpoke-1;
                    // POKE
                    for(j=0; j<AOconf[loop].DMctrl.sizeDM; j++)
                        arrayf[j] = ampl*PokeSign*data.image[IDpokeC].array.F[actarray[act1]*AOconf[loop].DMctrl.sizeDM+j];

                    usleep(delayRM1us);
                    data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].write = 1;
                    memcpy (data.image[aoloopcontrol_var.aoconfID_dmRM].array.F, arrayf, sizeof(float)*AOconf[loop].DMctrl.sizeDM);
                    data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].cnt0++;
                    data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].write = 0;
                    AOconf[loop].aorun.DMupdatecnt ++;
                }
            }


            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
            {
                data.image[IDzrespm].array.F[actarray[act]*AOconf[loop].WFSim.sizeWFS+ii] += data.image[IDpos].array.F[ii];
                data.image[IDzrespfp].array.F[actarray[act]*AOconf[loop].WFSim.sizeWFS+ii] = data.image[IDpos].array.F[ii];
                data.image[ID_WFSref0].array.F[ii] += data.image[IDpos].array.F[ii];
                data.image[ID_WFSref2].array.F[ii] += data.image[IDpos].array.F[ii]*data.image[IDpos].array.F[ii];
            }

            // NEGATIVE INTEGRATION
            //   printf("NEGATIVE INTEGRATION\n");
            //   fflush(stdout);
            for(kk=0; kk<NBave+NBexcl; kk++)
            {
                Read_cam_frame(loop, 1, normalize, 0, 0);
                if(kk<NBave)
                    for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                        data.image[IDneg].array.F[ii] += data.image[aoloopcontrol_var.aoconfID_imWFS1].array.F[ii];
                kk1++;
                if(kk1==NBave)
                {
                    kk1 = -NBexcl;
                    if(PokeSign==1)
                        PokeSign = -1;
                    else
                    {
                        act1++;
                        PokeSign = 1;
                    }
                    if(act1>NBpoke-1)
                        act1 = NBpoke-1;
                    // POKE
                    for(j=0; j<AOconf[loop].DMctrl.sizeDM; j++)
                        arrayf[j] = ampl*PokeSign*data.image[IDpokeC].array.F[actarray[act1]*AOconf[loop].DMctrl.sizeDM+j];

                    usleep(delayRM1us);
                    data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].write = 1;
                    memcpy (data.image[aoloopcontrol_var.aoconfID_dmRM].array.F, arrayf, sizeof(float)*AOconf[loop].DMctrl.sizeDM);
                    data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].cnt0++;
                    data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].write = 0;
                    AOconf[loop].aorun.DMupdatecnt ++;
                }
            }

            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
            {
                data.image[IDzrespm].array.F[actarray[act]*AOconf[loop].WFSim.sizeWFS+ii] -= data.image[IDneg].array.F[ii];
                data.image[IDzrespfm].array.F[actarray[act]*AOconf[loop].WFSim.sizeWFS+ii] = data.image[IDneg].array.F[ii];
                data.image[ID_WFSref0].array.F[ii] += data.image[IDneg].array.F[ii];
                data.image[ID_WFSref2].array.F[ii] += data.image[IDneg].array.F[ii] * data.image[IDneg].array.F[ii];
            }

            act++;
        }
        cntn = 2*NBave; // Number of images


        for(j=0; j<AOconf[loop].DMctrl.sizeDM; j++)
            arrayf[j] = 0.0;

        usleep(delayRM1us);
        data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].write = 1;
        memcpy (data.image[aoloopcontrol_var.aoconfID_dmRM].array.F, arrayf, sizeof(float)*AOconf[loop].DMctrl.sizeDM);
        data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].cnt0++;
        data.image[aoloopcontrol_var.aoconfID_dmRM].md[0].write = 0;
        AOconf[loop].aorun.DMupdatecnt ++;


        if(data.signal_USR1==0) // keep looping
        {
            for(act=0; act<NBpoke; act++)
                for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                    data.image[IDzrespmn].array.F[act*AOconf[loop].WFSim.sizeWFS+ii] = data.image[IDzrespm].array.F[actarray[act]*AOconf[loop].WFSim.sizeWFS+ii]/ampl/cntn;
            if(sprintf(fname, "!./zresptmp/%s_%03ld.fits", zrespm_name, iter) < 1)
                printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");
            save_fits(zrespm_name, fname);

            for(act=0; act<NBpoke; act++)
                for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                {
                    data.image[IDzrespfp].array.F[act*AOconf[loop].WFSim.sizeWFS+ii] /= NBave;
                    data.image[IDzrespfm].array.F[act*AOconf[loop].WFSim.sizeWFS+ii] /= NBave;
                }

            if(sprintf(fname, "!./zresptmp/%s_pos_%03ld.fits", zrespm_name, iter) < 1)
                printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");

            save_fits("zrespfp", fname);

            if(sprintf(fname, "!./zresptmp/%s_neg_%03ld.fits", zrespm_name, iter) < 1)
                printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");

            save_fits("zrespfm", fname);

            total = 0.0;
            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
            {
                data.image[ID_WFSref2n].array.F[ii] = sqrt((data.image[ID_WFSref2].array.F[ii] - data.image[ID_WFSref0].array.F[ii]*data.image[ID_WFSref0].array.F[ii])/NBave/cntn);
                data.image[ID_WFSref0n].array.F[ii] = data.image[ID_WFSref0].array.F[ii]/NBave/cntn;
                total += data.image[ID_WFSref0n].array.F[ii];
            }



            if(normalize==1)
            {
                for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                {
                    data.image[ID_WFSref0n].array.F[ii] /= total;
                    data.image[ID_WFSref2n].array.F[ii] /= total;
                }
            }
            else
            {
                for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                {
                    data.image[ID_WFSref0n].array.F[ii] /= NBave;
                    data.image[ID_WFSref2n].array.F[ii] /= NBave;
                }
            }

            if(sprintf(fname, "!./zresptmp/%s_%03ld.fits", WFSref0_name, iter) < 1)
                printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");

            save_fits(WFSref0_name, fname);

            if(sprintf(fname, "!./zresptmp/wfsimRMS.fits") < 1)
                printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");

            save_fits("wfsimrms", fname);


            if(mode!=3)
            {
                for(poke=0; poke<NBpoke; poke++)
                {
                    rms = 0.0;
                    for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                    {
                        tmpv = data.image[IDzrespmn].array.F[poke*AOconf[loop].WFSim.sizeWFS+ii];
                        rms += tmpv*tmpv;
                    }
                    data.image[ID_DMmap].array.F[act] = rms;
                }

                if(sprintf(fname, "!./zresptmp/%s_%03ld.fits", DMmap_name, iter) < 1)
                    printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");

                save_fits(DMmap_name, fname);


                for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                {
                    rms = 0.0;
                    for(poke=0; poke<NBpoke; poke++)
                    {
                        tmpv = data.image[IDzrespmn].array.F[poke*AOconf[loop].WFSim.sizeWFS+ii];
                        rms += tmpv*tmpv;
                    }
                    data.image[ID_WFSmap].array.F[ii] = rms;
                }

                if(sprintf(fname, "!./zresptmp/%s_%03ld.fits", zrespm_name, iter) < 1)
                    printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");

                save_fits(WFSmap_name, fname);

                if(mode>0) // compute WFSmask and DMmask
                {
                    // WFSmask : select pixels >40% of 85-percentile
                    lim = 0.4*img_percentile(WFSmap_name, 0.7);
                    for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                    {
                        if(data.image[ID_WFSmap].array.F[ii]<lim)
                            data.image[ID_WFSmask].array.F[ii] = 0.0;
                        else
                            data.image[ID_WFSmask].array.F[ii] = 1.0;
                    }

                    // DMmask: select pixels >10% of 50-percentile
                    lim = 0.1*img_percentile(DMmap_name, 0.5);
                    for(act=0; act<AOconf[loop].DMctrl.sizeDM; act++)
                    {
                        if(data.image[ID_DMmap].array.F[act]<lim)
                            data.image[ID_DMmask].array.F[act] = 0.0;
                        else
                            data.image[ID_DMmask].array.F[act] = 1.0;
                    }
                }
            }
            iter++;
            if(sprintf(command, "echo %ld > ./zresptmp/%s_nbiter.txt", iter, zrespm_name) < 1)
                printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");

            if(system(command) != 0)
                printERROR(__FILE__, __func__, __LINE__, "system() returns non-zero value");
        }
    } // end of iteration loop

    free(arrayf);
    free(sizearray);
    free(arraypix);

    free(actarray);

    delete_image_ID("tmpwfsref0");


    return(ID_WFSmap);
}






/** measures response matrix AND reference */
// scan delay up to fDelay

int_fast8_t AOloopControl_acquireCalib_Measure_Resp_Matrix(
    long loop,
    long NbAve,
    float amp,
    long nbloop,
    long fDelay,
    long NBiter
)
{
    long NBloops;
    long kloop;
    long delayus = 10000; // delay in us
    long ii, i, imax;
    int Verbose = 0;
    long k1, k, k2;
    char fname[200];
    char name0[200];
    char name[200];

    long kk;
    long RespMatNBframes;
    long IDrmc;
    long kc;

    long IDeigenmodes;

    long double RMsig;
    long double RMsigold;
    long kc0;
    FILE *fp;
    long NBexcl = 2; // number of frames excluded between DM mode changes
    long kc0min, kc0max;
    long IDrmtest;
    int vOK;

    long iter;
    long IDrmi;
    float beta = 0.0;
    float gain = 0.0001;
    long IDrmcumul;
    long IDrefi;
    long IDrefcumul;

    uint32_t *sizearray;

    long IDrespM;
    long IDwfsref0;

    long IDoptsignal; // optical signal for each mode, cumulative
    long IDoptsignaln; // optical signal for each mode, normalize
    long IDmcoeff; // multiplicative gain to amplify low-oder modes
    long IDoptcnt;
    double rmsval;
    char signame[200];

    double normcoeff, normcoeffcnt;


    int AdjustAmplitude = 0;
    char command[2000];

    float valave;
    long IDrmc1;



    RMACQUISITION = 1;


    printf("ACQUIRE RESPONSE MATRIX - loop = %ld, NbAve = %ld, amp = %f, nbloop = %ld, fDelay = %ld, NBiter = %ld\n", loop, NbAve, amp, nbloop, fDelay, NBiter);

    sizearray = (uint32_t*) malloc(sizeof(uint32_t)*3);



    if(AOloopcontrol_meminit==0)
        AOloopControl_InitializeMemory(0);
    //   sprintf(fname, "./conf/AOloop.conf");
    AOloopControl_loadconfigure(LOOPNUMBER, 1, 10);


    // create output
    IDwfsref0 = create_2Dimage_ID("refwfsacq", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS);
    IDrespM = create_3Dimage_ID("respmacq", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS, AOconf[loop].AOpmodecoeffs.NBDMmodes);




    IDoptsignal = create_2Dimage_ID("optsig", AOconf[loop].AOpmodecoeffs.NBDMmodes, 1);
    IDoptsignaln = create_2Dimage_ID("optsign", AOconf[loop].AOpmodecoeffs.NBDMmodes, 1);
    IDmcoeff = create_2Dimage_ID("mcoeff", AOconf[loop].AOpmodecoeffs.NBDMmodes, 1);
    IDoptcnt = create_2Dimage_ID("optsigcnt", AOconf[loop].AOpmodecoeffs.NBDMmodes, 1);

    for(k=0; k<AOconf[loop].AOpmodecoeffs.NBDMmodes; k++)
    {
        data.image[IDoptcnt].array.F[k] = 0.0;
        data.image[IDoptsignal].array.F[k] = 0.0;
        data.image[IDoptsignaln].array.F[k] = 0.0;
        data.image[IDmcoeff].array.F[k] = 1.0;
    }


    RespMatNBframes = 2*AOconf[loop].AOpmodecoeffs.NBDMmodes*NbAve;  // *nbloop
    printf("%ld frames total\n", RespMatNBframes);
    fflush(stdout);

    IDrmc = create_3Dimage_ID("RMcube", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS, RespMatNBframes); // this is the main cube





    IDrmi = create_3Dimage_ID("RMiter", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS, AOconf[loop].AOpmodecoeffs.NBDMmodes);    // Response matrix for 1 iteration
    IDrmcumul = create_3Dimage_ID("RMcumul", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS, AOconf[loop].AOpmodecoeffs.NBDMmodes);  // Cumulative Response matrix

    IDrefi = create_2Dimage_ID("REFiter", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS);
    IDrefcumul = create_2Dimage_ID("REFcumul", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS);



    /// local arrays for image acquision
    //	aoloopcontrol_var.aoconfID_wfsim = create_2Dimage_ID("RMwfs", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS);
    aoloopcontrol_var.aoconfID_imWFS0 = create_2Dimage_ID("RMwfs0", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS);
    aoloopcontrol_var.aoconfID_imWFS1 = create_2Dimage_ID("RMwfs1", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS);
    aoloopcontrol_var.aoconfID_imWFS1 = create_2Dimage_ID("RMwfs2", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS);


    aoloopcontrol_var.aoconfID_cmd_modesRM = create_2Dimage_ID("RMmodesloc", AOconf[loop].AOpmodecoeffs.NBDMmodes, 1);


    for(iter=0; iter<NBiter; iter++)
    {
        if (file_exists("stopRM.txt"))
        {
            if(system("rm stopRM.txt") != 0)
                printERROR(__FILE__, __func__, __LINE__, "system() returns non-zero value");

            iter = NBiter;
        }
        else
        {
            NBloops = nbloop;


            // initialize reference to zero
            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                data.image[IDrefi].array.F[ii] = 0.0;

            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS*RespMatNBframes; ii++)
                data.image[IDrmc].array.F[ii] = 0.0;


            //            printf("\n");
            //            printf("Testing (in measure_resp_matrix function) :,  NBloops = %ld, NBmode = %ld\n",  NBloops, AOconf[loop].AOpmodecoeffs.NBDMmodes);
            //            fflush(stdout);
            //            sleep(1);

            for(k2 = 0; k2 < AOconf[loop].AOpmodecoeffs.NBDMmodes; k2++)
                data.image[aoloopcontrol_var.aoconfID_cmd_modesRM].array.F[k2] = 0.0;



            // set DM to last mode, neg
            k1 = AOconf[loop].AOpmodecoeffs.NBDMmodes-1;
            data.image[aoloopcontrol_var.aoconfID_cmd_modesRM].array.F[k1] = -amp*data.image[IDmcoeff].array.F[k1];
            set_DM_modesRM(loop);


            usleep(delayus);

            for (kloop = 0; kloop < NBloops; kloop++)
            {
                kc = 0;
                if(Verbose)
                {
                    printf("\n Loop %ld / %ld (%f)\n", kloop, NBloops, amp);
                    fflush(stdout);
                }


                for(k1 = 0; k1 < AOconf[loop].AOpmodecoeffs.NBDMmodes; k1++)
                {
                    for(k2 = 0; k2 < AOconf[loop].AOpmodecoeffs.NBDMmodes; k2++)
                        data.image[aoloopcontrol_var.aoconfID_cmd_modesRM].array.F[k2] = 0.0;

                    // positive
                    data.image[aoloopcontrol_var.aoconfID_cmd_modesRM].array.F[k1] = amp*data.image[IDmcoeff].array.F[k1];
                    set_DM_modesRM(loop);




                    for(kk=0; kk<NbAve; kk++)
                    {
                        Read_cam_frame(loop, 1, 1, 0, 0);


                        for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                        {
                            data.image[IDrefi].array.F[ii] += data.image[aoloopcontrol_var.aoconfID_imWFS1].array.F[ii];
                            data.image[IDrmc].array.F[kc*AOconf[loop].WFSim.sizeWFS+ii] += data.image[aoloopcontrol_var.aoconfID_imWFS1].array.F[ii];
                        }
                        kc++;
                    }


                    // negative
                    data.image[aoloopcontrol_var.aoconfID_cmd_modesRM].array.F[k1] = 0.0-amp*data.image[IDmcoeff].array.F[k1];
                    set_DM_modesRM(loop);



                    for(kk=0; kk<NbAve; kk++)
                    {
                        Read_cam_frame(loop, 1, 1, 0, 0);

                        for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                        {
                            data.image[IDrefi].array.F[ii] += data.image[aoloopcontrol_var.aoconfID_imWFS1].array.F[ii];
                            data.image[IDrmc].array.F[kc*AOconf[loop].WFSim.sizeWFS+ii] += data.image[aoloopcontrol_var.aoconfID_imWFS1].array.F[ii];
                        }
                        kc++;
                    }
                }
            }

            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS*RespMatNBframes; ii++)
                data.image[IDrmc].array.F[ii] /= NBloops;


            // set DM to zero
            for(k2 = 0; k2 < AOconf[loop].AOpmodecoeffs.NBDMmodes; k2++)
                data.image[aoloopcontrol_var.aoconfID_cmd_modesRM].array.F[k2] = 0.0;
            set_DM_modesRM(loop);

            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                data.image[IDrefi].array.F[ii] /= RespMatNBframes*NBloops;



            // SAVE RMCUBE
            //    save_fits("RMcube", "!RMcube.fits");

            // remove average
            if(1)
            {
                IDrmc1 = create_3Dimage_ID("RMcube1", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS, RespMatNBframes); // this is the main cube, average removed

                for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                {
                    valave = 0.0;
                    for(kc=0; kc<RespMatNBframes; kc++)
                        valave += data.image[IDrmc].array.F[kc*AOconf[loop].WFSim.sizeWFS+ii];
                    valave /= RespMatNBframes;
                    for(kc=0; kc<RespMatNBframes; kc++)
                        data.image[IDrmc1].array.F[kc*AOconf[loop].WFSim.sizeWFS+ii] = data.image[IDrmc].array.F[kc*AOconf[loop].WFSim.sizeWFS+ii] - valave;
                }
                save_fits("RMcube1", "!RMcube1.fits");
            }




            IDrmtest = create_3Dimage_ID("rmtest", AOconf[loop].WFSim.sizexWFS, AOconf[loop].WFSim.sizeyWFS, AOconf[loop].AOpmodecoeffs.NBDMmodes);


            kc0 = fDelay;

            // initialize RM to zero
            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                for(k=0; k<AOconf[loop].AOpmodecoeffs.NBDMmodes; k++)
                    data.image[IDrmtest].array.F[k*AOconf[loop].WFSim.sizeWFS+ii] = 0.0;

            // initialize reference to zero
            kc = kc0;

            for(k1 = 0; k1 < AOconf[loop].AOpmodecoeffs.NBDMmodes; k1++)
            {
                // positive
                kc += NBexcl;
                if(kc > data.image[IDrmc].md[0].size[2]-1)
                    kc -= data.image[IDrmc].md[0].size[2];
                for(kk=NBexcl; kk<NbAve-NBexcl; kk++)
                {
                    for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                    {
                        data.image[IDrmtest].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii] += data.image[IDrmc].array.F[kc*AOconf[loop].WFSim.sizeWFS+ii];
                        //     data.image[IDrmc].array.F[kc*AOconf[loop].WFSim.sizeWFS+ii] += 1.0;
                    }
                    kc++;
                    if(kc > data.image[IDrmc].md[0].size[2]-1)
                        kc -= data.image[IDrmc].md[0].size[2];
                }
                kc+=NBexcl;
                if(kc > data.image[IDrmc].md[0].size[2]-1)
                    kc -= data.image[IDrmc].md[0].size[2];

                // negative
                kc+=NBexcl;
                if(kc > data.image[IDrmc].md[0].size[2]-1)
                    kc -= data.image[IDrmc].md[0].size[2];
                for(kk=NBexcl; kk<NbAve-NBexcl; kk++)
                {
                    for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                    {
                        data.image[IDrmtest].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii] -= data.image[IDrmc].array.F[kc*AOconf[loop].WFSim.sizeWFS+ii];
                        //  data.image[IDrmc].array.F[kc*AOconf[loop].WFSim.sizeWFS+ii] -= 1.0;
                    }
                    kc++;
                    if(kc > data.image[IDrmc].md[0].size[2]-1)
                        kc -= data.image[IDrmc].md[0].size[2];
                }
                kc+=NBexcl;
                if(kc > data.image[IDrmc].md[0].size[2]-1)
                    kc -= data.image[IDrmc].md[0].size[2];
            }

            //  save_fits("RMcube", "!RMcube2.fits");
            //  exit(0);
            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                for(k1=0; k1<AOconf[loop].AOpmodecoeffs.NBDMmodes; k1++)
                    data.image[IDrmi].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii] = data.image[IDrmtest].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii];


            //        save_fl_fits("rmtest", "!rmtest.fits");
            delete_image_ID("rmtest");




            printf("%ld %ld  %ld  %ld\n", IDrefcumul, IDrmcumul, IDwfsref0, IDrespM);


            beta = (1.0-gain)*beta + gain;
            for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
            {
                data.image[IDrefcumul].array.F[ii] = (1.0-gain)*data.image[IDrefcumul].array.F[ii] + gain*data.image[IDrefi].array.F[ii];

                data.image[IDwfsref0].array.F[ii] = data.image[IDrefcumul].array.F[ii]/beta;



                for(k1=0; k1<AOconf[loop].AOpmodecoeffs.NBDMmodes; k1++)
                {
                    data.image[IDrmcumul].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii] = (1.0-gain)*data.image[IDrmcumul].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii] + gain*data.image[IDrmi].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii];
                    data.image[IDrespM].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii] = data.image[IDrmcumul].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii]/beta;
                }
            }

            for(k1=0; k1<AOconf[loop].AOpmodecoeffs.NBDMmodes; k1++)
            {
                rmsval = 0.0;
                for(ii=0; ii<AOconf[loop].WFSim.sizeWFS; ii++)
                    rmsval += data.image[IDrespM].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii]*data.image[IDrespM].array.F[k1*AOconf[loop].WFSim.sizeWFS+ii];

                data.image[IDoptsignal].array.F[k1] += rmsval;
                data.image[IDoptcnt].array.F[k1] += 1.0;

                data.image[IDoptsignaln].array.F[k1] = data.image[IDoptsignal].array.F[k1]/data.image[IDoptcnt].array.F[k1];
            }
            save_fits("optsignaln","!./tmp/RM_optsign.fits");

            if(sprintf(signame, "./tmp/RM_optsign_%06ld.txt", iter) < 1)
                printERROR(__FILE__, __func__, __LINE__, "sprintf wrote <1 char");

            normcoeff = 0.0;
            normcoeffcnt = 0.0;
            for(k1=AOconf[loop].AOpmodecoeffs.NBDMmodes/2; k1<AOconf[loop].AOpmodecoeffs.NBDMmodes; k1++)
            {
                normcoeff += data.image[IDoptsignaln].array.F[k1];
                normcoeffcnt += 1.0;
            }
            normcoeff /= normcoeffcnt;



            if(AdjustAmplitude==1)
                for(k1=0; k1<AOconf[loop].AOpmodecoeffs.NBDMmodes; k1++)
                {
                    data.image[IDmcoeff].array.F[k1] = 0.8*data.image[IDmcoeff].array.F[k1] + 0.2/(data.image[IDoptsignaln].array.F[k1]/normcoeff);
                    if(data.image[IDmcoeff].array.F[k1]>5.0)
                        data.image[IDmcoeff].array.F[k1] = 5.0;
                }

            fp = fopen(signame, "w");
            for(k1=0; k1<AOconf[loop].AOpmodecoeffs.NBDMmodes; k1++)
                fprintf(fp, "%ld  %g  %g  %g\n", k1, data.image[IDoptsignaln].array.F[k1], data.image[IDoptcnt].array.F[k1], data.image[IDmcoeff].array.F[k1]*amp);
            fclose(fp);
            if(system("cp ./tmp/RM_outsign%06ld.txt ./tmp/RM_outsign.txt") != 0)
                printERROR(__FILE__, __func__, __LINE__, "system() returns non-zero value");

            save_fits("refwfsacq", "!./tmp/refwfs.fits");
            save_fits("respmacq", "!./tmp/respM.fits");
        }
    }


    fp = fopen("./tmp/rmparams.txt", "w");
    fprintf(fp, "%5ld       NbAve: number of WFS frames per averaging\n", NbAve);
    fprintf(fp, "%f	        amp: nominal DM amplitude (RMS)\n", amp);
    fprintf(fp, "%ld        iter: number of iterations\n", iter);
    fprintf(fp, "%ld        nbloop: number of loops per iteration\n", nbloop);
    fprintf(fp, "%ld        fDelay: delay number of frames\n", fDelay);
    fclose(fp);



    printf("Done\n");
    free(sizearray);


    return(0);
}






//
// Measure fast Modal response matrix
//
// HardwareLag [s]
//
// ampl [um]
//

long AOloopControl_acquireCalib_RespMatrix_Fast(
    const char *DMmodes_name,
    const char *dmRM_name,
    const char *imWFS_name,
    long semtrig,
    float HardwareLag,
    float loopfrequ,
    float ampl,
    const char *outname
)
{
    long IDout;
    long IDmodes;
    long IDmodes1; // muplitiples by ampl, + and -
    long IDdmRM;
    long IDwfs;
    long IDbuff;
    long ii, kk;

    long HardwareLag_int;
    float HardwareLag_frac;
    float WFSperiod;

    long NBmodes;
    long dmxsize, dmysize, dmxysize, wfsxsize, wfsysize, wfsxysize;
    long twait;

    int RT_priority = 80; //any number from 0-99
    struct sched_param schedpar;

    char *ptr0;
    long dmframesize;
    long wfsframesize;
    char *ptrs0;
    char *ptrs1;




    WFSperiod = 1.0/loopfrequ;
    HardwareLag_int = (long) (HardwareLag/WFSperiod);
    HardwareLag_frac = HardwareLag - WFSperiod*HardwareLag_int; // [s]

    twait = (long) (1.0e6 * ( (0.5*WFSperiod) - HardwareLag_frac ) );



    IDmodes = image_ID(DMmodes_name);
    dmxsize = data.image[IDmodes].md[0].size[0];
    dmysize = data.image[IDmodes].md[0].size[1];
    NBmodes = data.image[IDmodes].md[0].size[2];
    dmxysize = dmxsize*dmysize;




    IDmodes1 = image_ID("_tmpmodes");
    if(IDmodes1 == -1)
        IDmodes1 = create_3Dimage_ID("_tmpmodes", dmxsize, dmysize, 2*NBmodes);

    for(kk=0; kk<NBmodes; kk++)
    {
        for(ii=0; ii<dmxysize; ii++)
        {
            data.image[IDmodes1].array.F[2*kk*dmxysize+ii] =  ampl * data.image[IDmodes].array.F[kk*dmxysize+ii];
            data.image[IDmodes1].array.F[(2*kk+1)*dmxysize+ii] =  -ampl * data.image[IDmodes].array.F[kk*dmxysize+ii];
        }
    }



    IDdmRM = image_ID(dmRM_name);

    IDwfs = image_ID(imWFS_name);
    wfsxsize = data.image[IDwfs].md[0].size[0];
    wfsysize = data.image[IDwfs].md[0].size[1];
    wfsxysize = wfsxsize*wfsysize;

    IDbuff = image_ID("RMbuff");
    if(IDbuff == -1)
        IDbuff = create_3Dimage_ID("RMbuff", wfsxsize, wfsysize, 2*NBmodes + HardwareLag_int + 1);

    dmframesize = sizeof(float)*dmxysize;
    wfsframesize = sizeof(float)*wfsxysize;

    schedpar.sched_priority = RT_priority;
#ifndef __MACH__
    int r;

    r = seteuid(data.euid); //This goes up to maximum privileges
    sched_setscheduler(0, SCHED_FIFO, &schedpar); //other option is SCHED_RR, might be faster
    r = seteuid(data.ruid);//Go back to normal privileges
#endif

    ptr0 = (char*) data.image[IDmodes1].array.F;
    ptrs0 = (char*) data.image[IDbuff].array.F;

    // flush semaphore
    while(sem_trywait(data.image[IDwfs].semptr[semtrig])==0) {}


    for(kk=0; kk<NBmodes; kk++)
    {
        char *ptr1;

        sem_wait(data.image[IDwfs].semptr[semtrig]);
        ptrs1 = ptrs0 + wfsxysize*(2*kk);
        memcpy(ptrs1, data.image[IDwfs].array.F, wfsframesize);
        usleep(twait);

        // apply positive mode poke
        ptr1 = ptr0 + (2*kk)*dmframesize;
        data.image[IDdmRM].md[0].write = 1;
        memcpy(data.image[IDdmRM].array.F, ptr1, dmframesize);
        COREMOD_MEMORY_image_set_sempost_byID(IDdmRM, -1);
        data.image[IDdmRM].md[0].cnt0++;
        data.image[IDdmRM].md[0].write = 0;



        sem_wait(data.image[IDwfs].semptr[semtrig]);
        ptrs1 = ptrs0 + wfsxysize*(2*kk+1);
        memcpy(ptrs1, data.image[IDwfs].array.F, wfsframesize);
        usleep(twait);

        // apply negative mode poke
        ptr1 = ptr0 + (2*kk+1)*dmframesize;
        data.image[IDdmRM].md[0].write = 1;
        memcpy(data.image[IDdmRM].array.F, ptr1, dmframesize);
        COREMOD_MEMORY_image_set_sempost_byID(IDdmRM, -1);
        data.image[IDdmRM].md[0].cnt0++;
        data.image[IDdmRM].md[0].write = 0;
    }

    for(kk=0; kk<HardwareLag_int + 1; kk++)
    {
        sem_wait(data.image[IDwfs].semptr[semtrig]);
        ptrs1 = ptrs0 + wfsxysize*(2*NBmodes+kk);
        memcpy(ptrs1, data.image[IDwfs].array.F, wfsframesize);
        usleep(twait);

        // apply zero poke
        data.image[IDdmRM].md[0].write = 1;
        memset(data.image[IDdmRM].array.F, 0, dmframesize);
        COREMOD_MEMORY_image_set_sempost_byID(IDdmRM, -1);
        data.image[IDdmRM].md[0].cnt0++;
        data.image[IDdmRM].md[0].write = 0;
    }


    IDout = create_3Dimage_ID(outname, wfsxsize, wfsysize, NBmodes);
    for(kk=0; kk<NBmodes; kk++)
    {
        long buffindex;

        buffindex = 2*kk + HardwareLag_int;
        for(ii=0; ii<wfsxysize; ii++)
        {
            data.image[IDout].array.F[kk*wfsxysize + ii] = ( data.image[IDbuff].array.F[(buffindex)*wfsxysize + ii] - data.image[IDbuff].array.F[(buffindex+1)*wfsxysize + ii] ) / ampl;
        }

    }

    return(IDout);
}



/** 
 * 
 * ## Purpose
 * 
 * Processes a series of RMs that are overlapping in time
 * 
 * 
 * ## Arguments
 * 
 * Input RMs are named imrespC_000, imrespC_001 etc...\n
 * They are assumed to be regularly spaced in time\n
 * In each of these RM cubes, the z axis is time, increments by 1 frame\n
 * 
 * The reference is built by integrating from time refstart to refend as measured from the first RMsequence
 * 
 */

long AOloopControl_acquireCalib_RMseries_deinterlace(
	int NBRM,         // Number of RM sequences, each offset in time be 1/NBRM frame
	int refstart,     // start of reference time interval
	int refend,       // end of reference time interval
	char *IDout_name,
	int dmode,        // dimension mode. 0: classical, RM is 2D, 3rd dim is time. 1: RM is 3D, integer time step YYY encoded in image name: imrespC_XXX_YYY
	int NBtstep       // if dmode=1, number of RM time steps YYY
)
{
	long IDout;
	long xsizeWFS, ysizeWFS, sizeDM;
	long xsize, ysize, zsize, xysize;
	long rmCindex;
	
	
	long * IDRMarray = (long*) malloc(sizeof(long)*NBRM);
	
	if(dmode == 1)
	{
		long ID = image_ID("imrespC_000_000");
		xsizeWFS = data.image[ID].md[0].size[0];
		ysizeWFS = data.image[ID].md[0].size[1];
		sizeDM = data.image[ID].md[0].size[2];
	}
	
	for(rmCindex=0; rmCindex<NBRM; rmCindex++)
	{
		char rmCname[100];
		sprintf(rmCname, "imrespC_%03ld", rmCindex);
		
		
		if(dmode == 0)
			IDRMarray[rmCindex] = image_ID(rmCname);
		else
		{
			// assemble sequence from individual RM cubes
			char rmCfname[100];
			
			
			sprintf(rmCfname, "!imrespC_%03ld.fits", rmCindex);
			
			IDRMarray[rmCindex] = create_3Dimage_ID(rmCname, xsizeWFS*ysizeWFS, sizeDM, NBtstep);
			int tstep;
			for(tstep=0; tstep<NBtstep; tstep++)
			{
				char rmCname1[100];
				long ii, jj;
				
				sprintf(rmCname1, "imrespC_%03ld_%03d", rmCindex, tstep);
				long ID = image_ID(rmCname1);
				
				if(ID != -1)
					printf("Processing image %s\n", rmCname1);
				else
					{
						printf("ERROR: cannot find image %s\n", rmCname1);
						exit(0);
					}
				for(jj=0; jj<sizeDM; jj++)
					for(ii=0; ii<xsizeWFS*ysizeWFS; ii++)
					{
						data.image[IDRMarray[rmCindex]].array.F[(xsizeWFS*ysizeWFS*sizeDM)*tstep + jj*xsizeWFS*ysizeWFS + ii] = data.image[ID].array.F[xsizeWFS*ysizeWFS*jj+ii];					
					}
				delete_image_ID(rmCname1);
			}
			
			//save_fits(rmCname, rmCfname);			
		}
	}
	
	list_image_ID();
	
	xsize = data.image[IDRMarray[0]].md[0].size[0];
	ysize = data.image[IDRMarray[0]].md[0].size[1];
	zsize = data.image[IDRMarray[0]].md[0].size[2];
	xysize = xsize*ysize;



	
	
	
	// Set RMS to 1
	double *RMSarray;
	RMSarray = (double*) malloc(sizeof(double)*NBRM);
	for(rmCindex=0; rmCindex<NBRM; rmCindex++)
	{
		RMSarray[rmCindex] = 0;
		
		long imindex;
		for(imindex=0;imindex<zsize;imindex++)
		{
			float tstart0, tend0;
			
			tstart0 = 1.0*imindex;
			tend0 = tstart0+1.0;
			
			if((tstart0>refstart) && (tend0<refend))
			{
				long ii;
				
				for(ii=0;ii<xysize;ii++)
				{
					double tmpf;
				
					tmpf = data.image[IDRMarray[rmCindex]].array.F[imindex*xysize+ii];
					RMSarray[rmCindex] += tmpf*tmpf;
				}
			}
		}
		printf("RMS %ld = %g\n", rmCindex, RMSarray[rmCindex]);
		
		for(imindex=0;imindex<zsize;imindex++)
		{
			long ii;
			for(ii=0;ii<xysize;ii++)
				data.image[IDRMarray[rmCindex]].array.F[imindex*xysize+ii] /= sqrt(RMSarray[rmCindex]);
		}
	}



	// Compute reference and measure RMS
	long IDref = create_2Dimage_ID("imrespRef", xsize, ysize);
	long cntref = 0;
	
	for(rmCindex=0; rmCindex<NBRM; rmCindex++)
	{
		RMSarray[rmCindex] = 0;
		
		long imindex;
		for(imindex=0;imindex<zsize;imindex++)
		{
			float tstart, tend;
			float tstart0, tend0;
			
			tstart0 = 1.0*imindex;
			tend0 = tstart0+1.0;
			tstart = 1.0*imindex + 1.0*rmCindex/NBRM;
			tend = tstart+1.0;
			
			if((tstart0>refstart) && (tend0<refend))
			{
				long ii;
				
				for(ii=0;ii<xysize;ii++)
				{
					double tmpf;
				
					tmpf = data.image[IDRMarray[rmCindex]].array.F[imindex*xysize+ii];
					RMSarray[rmCindex] += tmpf*tmpf;
				}
			}
			
			if( (tstart>refstart) && (tend<refend) )
				{
					long ii;
					
					for(ii=0;ii<xysize;ii++)
						data.image[IDref].array.F[ii] += data.image[IDRMarray[rmCindex]].array.F[imindex*xysize+ii];
					cntref++;
				}
		}
		printf("RMS %ld = %g\n", rmCindex, RMSarray[rmCindex]);
	}
	
	free(RMSarray);
	
	if(cntref>0)
	{
		long ii;
		for(ii=0;ii<xysize;ii++)
			data.image[IDref].array.F[ii] /= cntref;
		}
		
	
	
	
	long zsizeout = NBRM*refstart;
	
	IDout = create_3Dimage_ID(IDout_name, xsize, ysize, zsizeout);
	
	float * coeffarray = (float*) malloc(sizeof(float)*zsizeout*NBRM*refstart); 
	
	
	for(rmCindex=0; rmCindex<NBRM; rmCindex++)
	{
		long rmCindex1 = rmCindex+1;
		if(rmCindex1 == NBRM)
			rmCindex1 = 0;
		
		long imindex;
		for(imindex=0;imindex<zsize;imindex++)
		{
			float tstart0;
			long imindex_run;
			
			imindex_run = imindex;
			tstart0 = 1.0*imindex_run + (1.0*rmCindex)/NBRM;
	
			
			// slice index in output
			long outindex = rmCindex + NBRM*imindex;
			
			
			
			
			if(outindex < zsizeout)
			{
				printf("Output Frame %5ld  [ f%ld in s%ld ]", outindex, imindex, rmCindex );
				
				while((tstart0<refstart)&&(imindex_run<zsize))
					{
						long ii;
						long imindex1_run;
						
						for (ii=0;ii<xysize;ii++)
							data.image[IDout].array.F[outindex*xysize+ii] += data.image[IDRMarray[rmCindex]].array.F[imindex_run*xysize+ii];
						
						if(rmCindex1 == 0)
							imindex1_run = imindex_run + 1;
						else
							imindex1_run = imindex_run;
						
						for (ii=0;ii<xysize;ii++)
							data.image[IDout].array.F[outindex*xysize+ii] -= data.image[IDRMarray[rmCindex1]].array.F[imindex1_run*xysize+ii];
					
					
						printf(" [ + f%ld s%ld  - f%ld s%ld ]", imindex_run, rmCindex, imindex1_run, rmCindex1 );
					
						imindex_run += 1;
						tstart0 = 1.0*imindex_run + (1.0*rmCindex)/NBRM;
					}
					
				long ii;
				for (ii=0;ii<xysize;ii++)
					data.image[IDout].array.F[outindex*xysize+ii] += 1.0/NBRM * data.image[IDref].array.F[ii];
							
				printf("\n");
			}
			
		}
	}

	
	free(IDRMarray);
	
	
	if(dmode == 1)
	{
		long act = 820;
		long IDactRM;
		
		IDactRM = create_3Dimage_ID("actRM", xsizeWFS, ysizeWFS, zsizeout);
		long kk;
		for(kk=0;kk<zsizeout;kk++)
		{
			long ii;
			for(ii=0;ii<xsizeWFS*ysizeWFS;ii++)
				data.image[IDactRM].array.F[kk*xsizeWFS*ysizeWFS+ii] = data.image[IDout].array.F[kk*xysize + act*xsize + ii];
		}
		
	}
	
	
	return(IDout);
}



















