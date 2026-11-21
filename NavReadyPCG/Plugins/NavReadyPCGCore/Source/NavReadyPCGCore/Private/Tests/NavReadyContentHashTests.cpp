// NavReadyPCGCore - automation tests for the content hash, on hand-made transforms (no world needed).
// Run: Tools\run_tests.bat (editor, headless), filter "NavReadyPCG.Hash".
// Hashes are uint32. TestEqual has no uint32 overload (int32, int64, SIZE_T, float and double make the call ambiguous),
// so equal hashes are checked as int64, which still prints both values on a failure.

#include "NavReadyContentHash.h"
#include "Tests/NavReadyTestFlags.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NavReadyTests
{
	const TCHAR* const Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	/** Three props, values well away from any quantization boundary (so 1e-5 cm of noise can't round differently). */
	TArray<FTransform> ThreeProps()
	{
		return {
			FTransform(FRotator(0.0, 30.0, 0.0), FVector(100012.34, 210.71, 50.0), FVector(0.81)),
			FTransform(FRotator(0.0, 123.0, 0.0), FVector(100415.62, -388.27, 50.0), FVector(1.13)),
			FTransform(FRotator(0.0, 301.0, 0.0), FVector(99604.18, 9.37, 50.0), FVector(0.67)) };
	}

	TArray<uint32> HashAll(const TArray<FTransform>& Props, const TCHAR* Mesh)
	{
		TArray<uint32> Hashes;
		for (const FTransform& Prop : Props)
		{
			Hashes.Add(NavReadyContentHash::HashInstance(Mesh, Prop));
		}
		return Hashes;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavReadyHashOrderTest, "NavReadyPCG.Hash.OrderIndependent", NavReadyTests::Flags)
bool FNavReadyHashOrderTest::RunTest(const FString& Parameters)
{
	using namespace NavReadyTests;
	const TArray<uint32> InOrder = HashAll(ThreeProps(), Cube);
	const TArray<uint32> Reversed = { InOrder[2], InOrder[1], InOrder[0] };

	const FNavReadyCellHash A = NavReadyContentHash::Combine(InOrder);
	const FNavReadyCellHash B = NavReadyContentHash::Combine(Reversed);
	TestEqual(TEXT("content hash ignores order"), static_cast<int64>(A.ContentHash), static_cast<int64>(B.ContentHash));
	TestNotEqual(TEXT("ordered hash sees order"), A.OrderedHash, B.OrderedHash);
	TestEqual(TEXT("instance count"), A.NumInstances, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavReadyHashNoiseTest, "NavReadyPCG.Hash.IgnoresFloatNoise", NavReadyTests::Flags)
bool FNavReadyHashNoiseTest::RunTest(const FString& Parameters)
{
	using namespace NavReadyTests;
	TArray<FTransform> Noisy = ThreeProps();
	for (FTransform& Prop : Noisy)
	{
		Prop.AddToTranslation(FVector(1e-5, -1e-5, 1e-5));  // far below the 1 mm step
		Prop.SetScale3D(Prop.GetScale3D() + FVector(1e-7));   // far below the 1e-3 step
	}
	TestEqual(TEXT("noise below the steps"),
		static_cast<int64>(NavReadyContentHash::Combine(HashAll(Noisy, Cube)).ContentHash),
		static_cast<int64>(NavReadyContentHash::Combine(HashAll(ThreeProps(), Cube)).ContentHash));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavReadyHashRealChangeTest, "NavReadyPCG.Hash.SeesRealChanges", NavReadyTests::Flags)
bool FNavReadyHashRealChangeTest::RunTest(const FString& Parameters)
{
	using namespace NavReadyTests;
	const uint32 Base = NavReadyContentHash::Combine(HashAll(ThreeProps(), Cube)).ContentHash;

	TArray<FTransform> Moved = ThreeProps();
	Moved[1].AddToTranslation(FVector(1.0, 0.0, 0.0)); // 1 cm
	TestNotEqual(TEXT("a 1 cm move changes the hash"), NavReadyContentHash::Combine(HashAll(Moved, Cube)).ContentHash, Base);

	TArray<FTransform> Turned = ThreeProps();
	Turned[0].SetRotation(FQuat(FRotator(0.0, 31.0, 0.0))); // 1 degree
	TestNotEqual(TEXT("a 1 degree turn changes the hash"), NavReadyContentHash::Combine(HashAll(Turned, Cube)).ContentHash, Base);

	TArray<FTransform> Scaled = ThreeProps();
	Scaled[2].SetScale3D(FVector(0.70));
	TestNotEqual(TEXT("a scale change changes the hash"), NavReadyContentHash::Combine(HashAll(Scaled, Cube)).ContentHash, Base);

	TestNotEqual(TEXT("another mesh changes the hash"), NavReadyContentHash::Combine(HashAll(ThreeProps(), Cylinder)).ContentHash, Base);

	TArray<FTransform> Fewer = ThreeProps();
	Fewer.RemoveAt(0);
	TestNotEqual(TEXT("a missing prop changes the hash"), NavReadyContentHash::Combine(HashAll(Fewer, Cube)).ContentHash, Base);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavReadyHashQuatSignTest, "NavReadyPCG.Hash.QuaternionSign", NavReadyTests::Flags)
bool FNavReadyHashQuatSignTest::RunTest(const FString& Parameters)
{
	using namespace NavReadyTests;
	const FQuat Q = FQuat(FRotator(10.0, 200.0, 5.0));
	const FTransform A(Q, FVector(100.0, 200.0, 50.0));
	const FTransform B(FQuat(-Q.X, -Q.Y, -Q.Z, -Q.W), FVector(100.0, 200.0, 50.0)); // the same rotation
	TestEqual(TEXT("q and -q hash the same"), static_cast<int64>(NavReadyContentHash::HashInstance(Cube, A)), static_cast<int64>(NavReadyContentHash::HashInstance(Cube, B)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavReadyHashEmptyTest, "NavReadyPCG.Hash.Empty", NavReadyTests::Flags)
bool FNavReadyHashEmptyTest::RunTest(const FString& Parameters)
{
	const FNavReadyCellHash Empty = NavReadyContentHash::Combine({});
	TestEqual(TEXT("no instances"), Empty.NumInstances, 0);
	TestEqual(TEXT("an empty cell always hashes the same"), static_cast<int64>(Empty.ContentHash), static_cast<int64>(NavReadyContentHash::Combine({}).ContentHash));
	TestNotEqual(TEXT("an empty cell differs from one prop"), Empty.ContentHash,
		NavReadyContentHash::Combine(NavReadyTests::HashAll({ FTransform::Identity }, NavReadyTests::Cube)).ContentHash);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
