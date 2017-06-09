#include <string.h>
#include <stdbool.h>
#include <Windows.h>
#include "mex.h"
#include "../csim2/solvers.h"
#include "../csim2heap/dllInterface.h"
#define TOOLBOX "csim2"
#define FUNCTION "mexrk4"
#include "../mexshared/mexshared.h"
#define DLLi 0
#define OBJi 1
#define DTi 2
#define Ti 3
#define ICi 4
#define Ui 5
#define U2i 6
#define Oi 7
#define LASTi Oi
#define DLLname "dllPath"
#define OBJname "objName"
#define DTname "dt"
#define Tname "time"
#define ICname "Xi"
#define Uname "input"
#define U2name "input2"
#define Oname "options"
#define SIGNATURE "[ output, state, dstate ] = " FUNCTION "( " DLLname ", " OBJname", " DTname ", " Tname ", " ICname ", " Uname ", " U2name ", <" Oname "> )"

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
	MATLABASSERT(nrhs == LASTi || nrhs == (LASTi+1), "nrhs", "expected 7 to 8 inputs: " SIGNATURE);
	MATLABASSERT(nlhs >= 1 && nlhs <= 3, "nlhs", "expected 1 to 3 outputs: " SIGNATURE);

	char * dll;
	char * obj;
	double dt; // 1 x 1 time step
	mwSize numSteps;
	double * time; // numSteps x 1 time vector
	mwSize numStates;
	double * Xi; // numStates x 1 initial conditions vector
	mwSize numInputs;
	double * U_t; // numSteps x numInputs inputs
	mwSize numInputs2;
	double * U2_t;
	char * options;
	char Empty = '\0';

	dll = assertIsChar(DLLname, prhs[DLLi]);
	obj = assertIsChar(OBJname, prhs[OBJi]);
	dt = assertIsScalar(DTname, prhs[DTi]);
	time = assertIsVector(Tname, prhs[Ti]);
	Xi = assertIsVector(ICname, prhs[ICi]);
	U_t = assertIsMatrix(Uname, prhs[Ui]);
	U2_t = assertIsMatrix(U2name, prhs[U2i]);

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

	numSteps = mxGetNumberOfElements(prhs[Ti]);
	numStates = mxGetNumberOfElements(prhs[ICi]);
	numInputs = mxGetM(prhs[Ui]);
	numInputs2 = mxGetM(prhs[U2i]);

	MATLABASSERTf(onExitFreedllHandle, mxGetN(prhs[Ui]) == numSteps, "inconsistentInput1", "size("Uname", 2) must equal numel("Tname").");
	MATLABASSERTf(onExitFreedllHandle, mxGetN(prhs[U2i]) == numSteps, "inconsistentInput2", "size("U2name", 2) must equal numel("Tname").");
	MATLABASSERTf(onExitFreedllHandle, numInputs == numInputs2, "inconsistentInput", "size("Uname", 1) must equal size("U2name", 1).");

	if (block.numStates != numStates)
	{
		char msg[STR2];
		sprintf_s(msg, STR2, ICname " is expected to have %zu elements.", block.numStates);
		onExitFreedllHandle();
		MATLABERROR("wrongSize_" ICname, msg);
	}

	if (block.numInputs != numInputs)
	{
		char msg[STR2];
		sprintf_s(msg, STR2, Uname " is expected to have %zu columns.", block.numInputs);
		onExitFreedllHandle();
		MATLABERROR("wrongSize_" Uname, msg);
	}

	// done with checking the inputs.
	// set up the outputs:

	bool const outputState = (nlhs >= 2);
	bool const outputdState = (nlhs >= 3);

	double * Y; // numSteps x numOutputs
	double * X; // numSteps x numStates OR 1 x numStates
	double * dX; // numSteps x numStates OR 1 x numStates

	plhs[0] = mxCreateDoubleMatrix(block.numOutputs, numSteps, mxREAL);
	Y = mxGetPr(plhs[0]);

	if (outputState)
	{
		plhs[1] = mxCreateDoubleMatrix(block.numStates, numSteps, mxREAL);
		X = mxGetPr(plhs[1]);
		memcpy(X, Xi, block.numStates * sizeof(double));
	}
	else
	{
		X = mxMalloc(block.numStates * sizeof(double));
	}

	if (outputdState)
	{
		plhs[2] = mxCreateDoubleMatrix(block.numStates, numSteps, mxREAL);
		dX = mxGetPr(plhs[2]);
	}
	else
	{
		dX = mxMalloc(block.numStates * sizeof(double)); // matlab handles freeing this for us. how nice
	}
	double * Xstorage = mxMalloc(block.numStates * 3 * sizeof(double));
	double * const B = &Xstorage[0 * block.numStates];
	double * const C = &Xstorage[1 * block.numStates];
	double * const D = &Xstorage[2 * block.numStates];

	// done with setting up the outputs
	// solve the problem

	double * nextState = X;
	double * currentState = Xi;
	double * currentdState = dX;
	double const * currentInput;
	double const * currentInput2;
	double const * nextInput;

	size_t ic = 0; //current time
	block.h(block.numStates, block.numOutputs, &Y[ic*block.numOutputs], time[ic], currentState, block.storage);
	for (size_t i = 1; i < numSteps; i++)
	{
		ic = i - 1;
		if (outputState)
		{
			currentState = &X[ic*block.numStates];
			nextState = &X[i*block.numStates];
		}
		else
		{
			memcpy(currentState, nextState, block.numStates * sizeof(double));
		}

		if (outputdState)
			currentdState = &dX[ic*block.numStates];

		currentInput = &U_t[ic*block.numInputs];
		currentInput2 = &U2_t[ic*block.numInputs];
		nextInput = &U_t[i*block.numInputs];

		rk4_f_step(block.numStates, block.numInputs, dt, time[ic], nextState, currentdState, B, C, D, currentState, currentInput, currentInput2, nextInput, block.f, block.storage);
		block.h(block.numStates, block.numOutputs, &Y[i*block.numOutputs], time[i], nextState, block.storage);
	}

	if (!outputState)
		mxFree(X);

	if (outputdState)
	{
		ic = numSteps - 1;
		block.f(block.numStates, block.numInputs, &dX[ic*block.numStates], time[ic], nextState, &U_t[ic*block.numInputs], block.storage);
	}
	else
	{
		mxFree(dX); // but the help says to do it anyway...
	}
	mxFree(Xstorage);

	getBlock->destructor(blockp);
	onExitFreedllHandle();
}

