#include <string.h>
#include <stdbool.h>
#include <Windows.h>
#include "mex.h"
#include "../csim2heap/dllInterface.h"
#define TOOLBOX "csim2"
#define FUNCTION "mexblockoutput"
#include "../mexshared/mexshared.h"
#define DLLi 0
#define OBJi 1
#define Ti 2
#define Xi 3
#define Oi 4
#define LASTi Oi
#define DLLname "dllPath"
#define OBJname "objName"
#define Tname "time"
#define Xname "state"
#define Oname "options"
#define SIGNATURE "[ output ] = " FUNCTION "( " DLLname ", " OBJname", " Tname ", " Xname ", <" Oname "> )"

static HINSTANCE dllHandle = NULL;

static void onExitFreedllHandle(void)
{
	//mexPrintf("...\n");
	if (dllHandle)
	{
		FreeLibrary(dllHandle);
		dllHandle = NULL;
		//mexPrintf("FreeLibrary\n");
	}
}

void mexFunction(
	int nlhs,
	mxArray *plhs[],
	int nrhs,
	const mxArray *prhs[]
)
{
	MATLABASSERT(nrhs == LASTi || nrhs == (LASTi+1), "nrhs", "expected 4 to 5 inputs: " SIGNATURE);
	MATLABASSERT(nlhs == 1, "nlhs", "expected 1 output: " SIGNATURE);

	char const * dll;
	char const * obj;
	double time; // numSteps x 1 time vector
	mwSize numStates;
	double const * X; // numStates x 1 initial conditions vector
	char const * options;
	char Empty = '\0';

	dll = assertIsChar(DLLname, prhs[DLLi]);
	obj = assertIsChar(OBJname, prhs[OBJi]);
	time = assertIsScalar(Tname, prhs[Ti]);
	X = assertIsVector(Xname, prhs[Xi]);

	if (nrhs == (LASTi+1))
		options = assertIsChar(Oname, prhs[Oi]);
	else
		options = &Empty;

	dllHandle = LoadLibrary(dll);
	if (!dllHandle)
	{
		char msg[STR2] = "failed to load dll \"";
		strcat_s(msg, STR2, dll);
		strcat_s(msg, STR2, "\"");
		MATLABERROR("failedToLoadDLL", msg);
	}
	mexAtExit(onExitFreedllHandle);

	struct dllStrictlyProperBlock * getBlock = (struct dllStrictlyProperBlock *)GetProcAddress(dllHandle, obj);
	if (!getBlock)
	{
		char msg[STR2] = "failed to load \"";
		strcat_s(msg, STR2, obj);
		strcat_s(msg, STR2, "\" from dll");
		onExitFreedllHandle();
		MATLABERROR("failedToLoadStrictlyProperBlock", msg);
	}

	struct StrictlyProperBlock * blockp = getBlock->constructor(options);
	if (!blockp)
	{
		char msg[STR2] = "failed to construct \"";
		strcat_s(msg, STR2, obj);
		strcat_s(msg, STR2, "\" from dll");
		onExitFreedllHandle();
		MATLABERROR("failedToConstructStrictlyProperBlock", msg);
	}
	struct StrictlyProperBlock block = *blockp;

	numStates = mxGetNumberOfElements(prhs[Xi]);

	if (block.numStates != numStates)
	{
		char msg[STR2];
		sprintf_s(msg, STR2, Xname " is expected to have %zu elements.", block.numStates);
		onExitFreedllHandle();
		MATLABERROR("wrongSize_" Xname, msg);
	}

	// done with checking the inputs.
	// set up the outputs:

	plhs[0] = mxCreateDoubleMatrix(block.numOutputs, 1, mxREAL);
	double * output = mxGetPr(plhs[0]);

	// done with setting up the outputs
	// solve the problem

	block.h(block.numStates, block.numOutputs, output, time, X, block.storage);

	getBlock->destructor(blockp);
	onExitFreedllHandle();
}
