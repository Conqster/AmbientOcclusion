#pragma once 

#include "pregl/Core/Log.h"
#include "pregl/Core/Application.h"

#include "PreglRenderer/samples/SampleGfxProgram.h"
#include "pregl/Core/HeapMemAllocationTracking.h"

#include "SSAOProgram.h"

int main()
{
	//SCOPE_MEM_ALLOC_PROFILE("Program");
//OPEN_BLOCK_MEM_TRACKING_PROFILE(Program);
	DEBUG_LOG("Launch PreglRenderer Application Program !!!!!!!!");

	auto app = Application({ "PreglRenderer App", false, {1920, 1080} });


	//auto gfx = new SampleGfxProgram();
	auto gfx = new SSAOProgram();
	gfx->OnInitialise(&app.GetWindow());
	gfx->SetCamera(&app.GetCamera());
	app.AddGfxProgram(gfx);

	app.Run();

	//CLOSE_BLOCK_MEM_TRACKING_PROFILE(Program);
	return 0;
}