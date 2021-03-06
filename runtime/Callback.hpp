/*
  Callback.hpp

  Callback function to set up remain works if it has yet finished

*/

#ifndef __CLPKM__CALLBACK_HPP__
#define __CLPKM__CALLBACK_HPP__

#include "ResourceGuard.hpp"
#include "RuntimeKeeper.hpp"
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <CL/opencl.h>



namespace CLPKM {

struct CallbackData {

	CallbackData() = delete;

	CallbackData(cl_command_queue Q, clKernel&& K, KernelInfo* KI, cl_uint D,
	             std::vector<size_t>&& IGWO, std::vector<size_t>&& IGWS,
	             std::vector<size_t>&& ILWS, size_t IWGS,
	             clMemObj&& DH, clMemObj&& LB, clMemObj&& PB,
	             std::vector<cl_int>&& HM, size_t HO, clEvent&& E, clEvent&& F,
	             std::chrono::high_resolution_clock::time_point TP)
	: Queue(Q), Kernel(std::move(K)), KInfo(KI), WorkDim(D),
	  GWO(std::move(IGWO)), GWS(std::move(IGWS)), LWS(std::move(ILWS)), WorkGrpSize(IWGS),
	  DeviceHeader(std::move(DH)), LocalBuffer(std::move(LB)), PrivateBuffer(std::move(PB)),
	  HostMetadata(std::move(HM)), HeaderOffset(HO), PrevWork{NULL, std::move(E)},
	  Final(std::move(F)), LastCall(TP), Counter(0),
	  Mutex(std::make_unique<std::recursive_mutex>()) { }

	// Shadow queue and kernel to run
	cl_command_queue Queue;
	clKernel         Kernel;
	KernelInfo*      KInfo;

	// Needed for enqueue
	cl_uint WorkDim;
	std::vector<size_t> GWO;
	std::vector<size_t> GWS;
	std::vector<size_t> LWS;
	size_t WorkGrpSize;

	// Record so that we can release the resources
	clMemObj DeviceHeader;
	clMemObj LocalBuffer;
	clMemObj PrivateBuffer;

	// The vector is not necessary here but I don't want to reallocate a buffer
	// HeaderOffset indicates where the header starts
	std::vector<cl_int> HostMetadata;
	const size_t HeaderOffset;

	// MetaEnqueue enqueues two commands once, one for computation and one to
	// read the header, and setting up callback for the later command. The
	// cl_event associated to the later can be retrieved from the arguments to
	// the callback while it's not possible for the first command. As a result,
	// we must save the cl_event to track the status of the first command. Such
	// event is placed at PrevWork[0].
	// In case of first run, PrevWork[1] refers to the command to write header to
	// the device.
	clEvent PrevWork[2];
	clEvent Final;

	// Profiling related stuff
	std::chrono::high_resolution_clock::time_point LastCall;
	unsigned Counter;

	std::vector<std::array<unsigned, 2>> Bucket;

	std::unique_ptr<std::recursive_mutex> Mutex;

	};

void MetaEnqueue(CallbackData* , cl_uint , cl_event* );
void CL_CALLBACK ResumeOrFinish(cl_event , cl_int , void* );

}



#endif
