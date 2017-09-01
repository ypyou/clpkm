/*
  Callback.cpp

  Callback function to set up remain works if it has yet finished

*/



#include "Callback.hpp"
#include "ErrorHandling.hpp"
#include <algorithm>

using namespace CLPKM;



void CLPKM::MetaEnqueue(CallbackData* Work, cl_uint NumWaiting,
                        cl_event* WaitingList) {

	clEvent EventRead = getEvent(NULL);

	// Enqueue kernel and read data
	cl_int Ret = Lookup<OclAPI::clEnqueueNDRangeKernel>()(
			Work->Queue, Work->Kernel, Work->WorkDim, Work->GWO.data(),
			Work->GWS.data(), Work->LWS.data(), NumWaiting, WaitingList,
			&Work->PrevWork[0].get());
	OCL_ASSERT(Ret);

	Ret = Lookup<OclAPI::clEnqueueReadBuffer>()(
		Work->Queue, Work->DeviceHeader.get(), CL_FALSE, 0,
		Work->HostHeader.size() * sizeof(cl_int), Work->HostHeader.data(), 1,
		&Work->PrevWork[0].get(), &EventRead.get());
	OCL_ASSERT(Ret);

	// Set up callback to continue
	Ret = Lookup<OclAPI::clSetEventCallback>()(
			EventRead.get(), CL_COMPLETE, ResumeOrFinish, Work);
	OCL_ASSERT(Ret);

	Ret = clFlush(Work->Queue);
	OCL_ASSERT(Ret);

	// If nothing went south, set to NULL so the guard won't release it
	EventRead.get() = NULL;

	}




void CL_CALLBACK CLPKM::ResumeOrFinish(cl_event Event, cl_int ExecStatus,
                                       void* UserData) try {

	auto* Work = static_cast<CallbackData*>(UserData);
	clEvent ThisEvent = getEvent(Event);
	cl_int Ret = CL_SUCCESS;

	// Step 1
	// Check the status of associated run

	for (size_t Idx : {1, 0}) {
		if (Work->PrevWork[Idx].get() == NULL)
			continue;
		cl_int Status = CL_SUCCESS;
		Ret = Lookup<OclAPI::clGetEventInfo>()(
				Work->PrevWork[Idx].get(), CL_EVENT_COMMAND_EXECUTION_STATUS,
				sizeof(cl_int), &Status, nullptr);
		// Status of the call to clGetEventInfo
		OCL_ASSERT(Ret);
		// Status of the event associated to previous enqueued commands
		OCL_ASSERT(Status);
		// Release so MetaEnqueue can use the slot
		Work->PrevWork[Idx].Release();
		}
	// Status of the event associated to clEnqueueReadBuffer
	OCL_ASSERT(ExecStatus);

	// Step 2
	// If finished
	if (std::none_of(Work->HostHeader.begin(), Work->HostHeader.end(),
	                 [](cl_int S) -> bool { return S; })) {
		Ret = Lookup<OclAPI::clSetUserEventStatus>()(Work->Final.get(), CL_COMPLETE);
		// Note: if the call failed here, following commands are likely to get
		//       stuck forever...
		if (Ret != CL_SUCCESS) {
			getRuntimeKeeper().Log(RuntimeKeeper::loglevel::ERROR,
			                       "\n==CLPKM== clSetUserEventStatus ret'd " PRId32 "\n",
			                        Ret);
			}
		delete Work;
		return;
		}

	// Step 3
	// Set up following run
	MetaEnqueue(Work, 0, nullptr);

	}
catch (const __ocl_error& OclError) {
	auto* Work = static_cast<CallbackData*>(UserData);
	cl_int Ret = Lookup<OclAPI::clSetUserEventStatus>()(
			Work->Final.get(), OclError);
	if (Ret != CL_SUCCESS) {
		getRuntimeKeeper().Log(RuntimeKeeper::loglevel::ERROR,
		                       "\n==CLPKM== clSetUserEventStatus ret'd " PRId32 "\n",
		                       Ret);
		}
	delete Work;
	}