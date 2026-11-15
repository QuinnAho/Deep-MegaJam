// Screenshot support for the harness (`ue.ps1 shot`). Started with -DeepShot=<absolute png path>
// [-DeepShotDelay=<seconds>, default 6] [-DeepShotCmds="cmd1;cmd2"]: waits the delay (real time), runs the console
// commands, requests a screenshot with the HUD to that path, then quits once the file exists (or after a timeout).

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace DeepShot
{
	/** Starts the sequence once per process if the command line asks for it. Safe to call on every StartPlay. */
	DEEP_API void StartIfRequested(UWorld* World);
}
