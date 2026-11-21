// NavReadyPCGCore - the automation-test flags shared by every NavReady test file.

#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NavReadyTests
{
	/** Editor and game contexts, one filter (ProductFilter): the macro requires exactly one filter flag. */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

#endif // WITH_DEV_AUTOMATION_TESTS
