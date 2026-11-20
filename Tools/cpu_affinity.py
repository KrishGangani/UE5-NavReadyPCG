"""P08 NavReadyPCG: pin every timed run to the performance cores of this hybrid CPU.

performance_core_mask(), pin_self(), applied_mask() and the KERNEL32 set-up are copied from UE5-UpdateBudget's
Tools/run_bench.py, unchanged in behaviour, so this repository stays standalone. run_bench.py (step S3) imports this
file from the same folder: `python -I` does not put a script's own folder on sys.path, so run_bench.py adds its
folder explicitly first.

How a timed batch is run (the protocol in Benchmarks/README.md):
  1. the batch script calls pin_self(performance_core_mask()) ONCE, before it records the environment or starts
     anything, and refuses to measure if that fails;
  2. it records applied_mask() (read back from Windows) in the environment record;
  3. it starts the REAL game executable (Build\\Windows\\NavReadyPCG\\Binaries\\Win64\\NavReadyPCG.exe, not the bootstrap
     Build\\Windows\\NavReadyPCG.exe) with run_game(): subprocess.Popen, a timeout, and a kill of the process tree in a
     `finally` block. A child process inherits its parent's affinity mask from its first instruction, so the game
     runs pinned for its whole life, engine start-up included.
`cmd /c start "" /wait /affinity ...` is NOT used: it returns exit code 0 whatever the game returned.

Usage: python -I cpu_affinity.py      prints the performance-core mask as hex, e.g. 0xFFFF, or "none".
"""
import ctypes
import struct
import subprocess
import sys

KERNEL32 = None
if sys.platform == "win32":
    KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)
    KERNEL32.GetCurrentProcess.restype = ctypes.c_void_p
    KERNEL32.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    KERNEL32.GetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]


def performance_core_mask() -> int | None:
    """Affinity mask of the performance cores on a hybrid CPU; None on a non-hybrid CPU or off Windows.

    Why: on this i7-14650HX (8 P-cores + 8 E-cores) UE5-UpdateBudget's S3 baseline batch showed runs that sat at two
    stable levels about 1.6x apart, and one run that stepped between them mid-capture: Windows had placed the game
    thread on an E-core. Pinning to the P-cores gives every run what a foreground game normally gets.
    Source of truth: GetLogicalProcessorInformationEx(RelationProcessorCore) reports an EfficiencyClass per physical
    core; the P-cores have the higher class. Only processor group 0 exists on a machine with < 64 logical CPUs.
    """
    if KERNEL32 is None:
        return None
    length = ctypes.c_ulong(0)
    KERNEL32.GetLogicalProcessorInformationEx(0, None, ctypes.byref(length))
    buffer = ctypes.create_string_buffer(length.value)
    if not length.value or not KERNEL32.GetLogicalProcessorInformationEx(0, buffer, ctypes.byref(length)):
        return None
    raw, offset, masks = buffer.raw, 0, {}          # efficiency class -> combined mask of its cores
    while offset < length.value:
        relationship, size = struct.unpack_from("<II", raw, offset)
        if relationship == 0 and size >= 48:                     # RelationProcessorCore
            efficiency_class = raw[offset + 9]                   # PROCESSOR_RELATIONSHIP.EfficiencyClass
            mask, group = struct.unpack_from("<QH", raw, offset + 32)   # GroupMask[0]: KAFFINITY Mask, WORD Group
            if group == 0:
                masks[efficiency_class] = masks.get(efficiency_class, 0) | mask
        offset += size
    return masks[max(masks)] if len(masks) > 1 else None


def pin_self(mask: int) -> bool:
    """Restricts this batch process to the CPUs in mask. Every process it starts afterwards inherits the mask from its
    first instruction, so each game runs pinned for its whole life, engine start-up included."""
    return bool(KERNEL32.SetProcessAffinityMask(KERNEL32.GetCurrentProcess(), mask))


def applied_mask() -> str:
    """The affinity this process actually has (read back from Windows, not the one requested)."""
    if KERNEL32 is None:
        return "not Windows"
    process_mask, system_mask = ctypes.c_size_t(0), ctypes.c_size_t(0)
    KERNEL32.GetProcessAffinityMask(KERNEL32.GetCurrentProcess(), ctypes.byref(process_mask), ctypes.byref(system_mask))
    if process_mask.value == system_mask.value:
        return f"0x{process_mask.value:X} (all logical processors)"
    return f"0x{process_mask.value:X} (performance cores only; hybrid CPU)"


def run_game(command: list[str], timeout_seconds: float) -> int | None:
    """Runs one timed game process (it inherits this process's affinity). Returns its exit code, or None on timeout."""
    process = subprocess.Popen(command)
    try:
        return process.wait(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        return None
    finally:
        if process.poll() is None:   # timeout, Ctrl+C or any other error: never leave a game running into the next run
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)], capture_output=True)


if __name__ == "__main__":
    MASK = performance_core_mask()
    print("none" if MASK is None else f"0x{MASK:X}")
