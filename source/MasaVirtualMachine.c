#include "MasaVirtualMachine.h"
#include "wr/WRMemory.h"
#include "wr/WRError.h"
#include "wr/WRThread.h"
#include <stdatomic.h>
#include <time.h>
#include "wr/WRCompile.h"
#include <stdio.h>


#define UNDERLYING_THREAD_MAIN NULL
#define MAIN_CORE_INDEX 0
#define CORE_OPERATIONS_PER_BATCH (10'000)


// Types.
typedef struct MasaIntegerUnitStruct
{
    unsigned char RegisterA[MASA_BYTE_COUNT];
    unsigned char RegisterB[MASA_BYTE_COUNT];
    unsigned char Result[MASA_BYTE_COUNT];
    unsigned char DataType;
} MasaIntegerUnit;

typedef struct MasaFloatUnitStruct
{
    unsigned char RegisterA[MASA_BYTE_COUNT];
    unsigned char RegisterB[MASA_BYTE_COUNT];
    unsigned char Result[MASA_BYTE_COUNT];
    unsigned char DataType;
} MasaFloatUnit;

typedef struct MasaRAMStruct
{
    unsigned char* Data;
    size_t Size;
} MasaRAM;

typedef struct MasaCPUCore
{
    size_t MinNanoSecondsPerTick;
    size_t TickCounter;
    size_t ProgramCounter;
    unsigned char Registers[MASA_REGISTERS_COUNT][MASA_BYTE_COUNT];
    unsigned char InstructionRegisters[MASA_INSTRUCTION_REGISTER_COUNT][MASA_BYTE_COUNT];
    MasaIntegerUnit IntegerUnit;
    MasaFloatUnit FloatUnit;

    Thread* UnderlyingThread;
    atomic_bool IsRunning;

    MasaRAM* TargetRAM;
} MasaCPUCore;

typedef struct MasaCPUStruct
{
    MasaCPUCore Cores[MASA_MAX_CORE_COUNT];
    size_t CoreCount;
    size_t MainCoreIndex;
} MasaCPU;

typedef struct MasaVirtualMachineStruct 
{
    MasaRAM RAM;
    MasaCPU CPU;
    bool _isRunning;
} MasaVirtualMachine;


// Static functions.
static Error CreateNullArgumentError(const unsigned char* argumentName)
{
    return Error_Construct3(ErrorCode_IllegalArgument, u8"Argument \"%s\" cannot be null.", argumentName);
}

static Error ConstructMasaCores(MasaCPU* cpu, size_t coreCount, size_t minNanoSecondsPerTick, MasaRAM* ram)
{
    cpu->CoreCount = coreCount;

    for (size_t i = 0; i < coreCount; i++)
    {
        MasaCPUCore* Core = &cpu->Cores[i];
        Memory_Zero(Core, sizeof(*Core));
        Core->MinNanoSecondsPerTick = minNanoSecondsPerTick;
        Core->TargetRAM = ram;
    }

    cpu->MainCoreIndex = MAIN_CORE_INDEX;

    return Error_CreateSuccess();
}

static Error ConstructMasaRAM(MasaRAM* ram, size_t ramByteCount)
{
    ram->Size = ramByteCount;
    ram->Data = Memory_Allocate(ramByteCount);
    Memory_Zero(ram->Data, ramByteCount);
    return Error_CreateSuccess();
}

static Error Masa_FinishConstruction(MasaVirtualMachine* machine, MasaConstructOptions options)
{
    Memory_Zero(machine, sizeof(MasaVirtualMachine));
    
    Error Result = ConstructMasaCores(&machine->CPU,
        options.CoreCount,
        options.MinNanoSecondsPerTick,
        &machine->RAM);
        
    if (Result.Code != ErrorCode_Success)
    {
        return Result;
    }

    Result = ConstructMasaRAM(&machine->RAM, options.RamByteCount);
    if (Result.Code != ErrorCode_Success)
    {
        return Result;
    }

    return Error_CreateSuccess();
}

static void MasaCore_ExecuteSingleCycle(MasaCPUCore* core)
{
    UNUSED(core);
}

static void MasaCore_MainLoop(MasaCPUCore* core)
{
    double MaxElapsedTimeSeconds = (double)core->MinNanoSecondsPerTick / (double)NANOSECONDS_IN_SECOND;
    double ElapsedTimeSeconds = 0.0;
    clock_t StartTime = clock();

    do
    {
        for (size_t i = 0; i < CORE_OPERATIONS_PER_BATCH; i++)
        {
            MasaCore_ExecuteSingleCycle(core);
        }

        core->ProgramCounter += MASA_INSTRUCTION_BYTE_COUNT;
        core->TickCounter++;

        clock_t ElapsedClock = clock() - StartTime;
        if (ElapsedClock <= 0)
        {
            ElapsedClock = 1;
        }
        ElapsedTimeSeconds += (double)ElapsedClock / (double)CLOCKS_PER_SEC;
    } while (ElapsedTimeSeconds <= MaxElapsedTimeSeconds);
}

static void* MasaSideCore_EntryPoint(void* data)
{
    MasaCPUCore* Core = data;

    while (atomic_load(&Core->IsRunning))
    {
        MasaCore_MainLoop(Core);
    }

    return NULL;
}

static Error MasaCore_TryJoin(MasaCPUCore* core)
{
    if (core->UnderlyingThread == UNDERLYING_THREAD_MAIN)
    {
        return Error_CreateSuccess();
    }

    void* ThreadReturnValue;
    Error Result = Thread_Join(core->UnderlyingThread, &ThreadReturnValue);
    return Result;
}



// Functions.
bool Masa_IsRunning(MasaVirtualMachine* machine)
{
    return machine->_isRunning;
}

Error Masa_Create(MasaVirtualMachine** outMachine, MasaConstructOptions options)
{
    if (outMachine == NULL)
    {
        return CreateNullArgumentError(u8"machine");
    }

    *outMachine = NULL;
    if (options.CoreCount == 0)
    {
        return Error_Construct3(ErrorCode_IllegalArgument, 
            u8"Core count cannot be zero.");
    }
    if (options.CoreCount > MASA_MAX_CORE_COUNT)
    {
        return Error_Construct3(ErrorCode_IllegalArgument,
            u8"Core count cannot be greater than %zu.",
            MASA_MAX_CORE_COUNT);
    }
    if (options.RamByteCount == 0)
    {
        return Error_Construct3(ErrorCode_IllegalArgument,
            u8"RAM byte count cannot be zero.");
    }
    if (options.RamByteCount > MASA_MAX_RAM_SIZE_BYTES)
    {
        return Error_Construct3(ErrorCode_IllegalArgument,
            u8"RAM byte count cannot be greater than %zu.",
            MASA_MAX_RAM_SIZE_BYTES);
    }
    if (options.MinNanoSecondsPerTick == 0)
    {
        return Error_Construct3(ErrorCode_IllegalArgument,
            u8"Minimum nanoseconds per tick cannot be zero.");
    }
    if (options.MinNanoSecondsPerTick > MASA_MAX_NANOSECONDS_PER_TICK)
    {
        return Error_Construct3(ErrorCode_IllegalArgument,
            u8"Minimum nanoseconds per tick cannot be greater than %zu.",
            MASA_MAX_NANOSECONDS_PER_TICK);
    }

    MasaVirtualMachine* Machine = Memory_Allocate(sizeof(*Machine));
    Error Result = Masa_FinishConstruction(Machine, options);
    if (Result.Code != ErrorCode_Success)
    {
        Memory_Free(Machine);
        return Result;
    }

    *outMachine = Machine;
    return Error_CreateSuccess();
}

Error Masa_Start(MasaVirtualMachine* machine)
{
    if (machine == NULL)
    {
        return CreateNullArgumentError(u8"machine");
    }
    if (Masa_IsRunning(machine))
    {
        return Error_Construct3(ErrorCode_InvalidState,
            u8"Masa machine is already running");
    }

    
    MasaCPU* CPU = &machine->CPU;
    for (size_t i = 0; i < CPU->CoreCount; i++)
    {
        MasaCPUCore* TargetCore = &CPU->Cores[i];
        if (i == CPU->MainCoreIndex)
        {
            continue;
        }

        Thread_Create(&TargetCore->UnderlyingThread, &MasaSideCore_EntryPoint, TargetCore);
    }

    machine->_isRunning = true;
    return Error_CreateSuccess();
}

Error Masa_Stop(MasaVirtualMachine* machine)
{
    if (machine == NULL)
    {
        return CreateNullArgumentError(u8"machine");
    }
    if (!Masa_IsRunning(machine))
    {
        return Error_Construct3(ErrorCode_InvalidState,
            u8"Masa machine already isn't running");
    }

    MasaCPU* CPU = &machine->CPU;
    for (size_t i = 0; i < CPU->CoreCount; i++)
    {
        MasaCPUCore* TargetCore = &CPU->Cores[i];
        atomic_store(&TargetCore->IsRunning, false);
        Error Result = MasaCore_TryJoin(TargetCore);
        if (Result.Code != ErrorCode_Success)
        {
            return Result;
        }
    }

    return Error_CreateSuccess();
}

Error Masa_Tick(MasaVirtualMachine* machine)
{
    if (machine == NULL)
    {
        return CreateNullArgumentError(u8"machine");
    }
    if (!Masa_IsRunning(machine))
    {
        return Error_Construct3(ErrorCode_InvalidState,
            u8"cannot tick the masa machine because it isn't running");
    }

    MasaCPU* CPU = &machine->CPU;
    MasaCore_MainLoop(&CPU->Cores[CPU->MainCoreIndex]);
    return Error_CreateSuccess();
}

Error Masa_Deconstruct(MasaVirtualMachine* machine)
{
    Error Result;
    if (Masa_IsRunning(machine))
    {
        Result = Masa_Stop(machine);
        if (Result.Code != ErrorCode_Success)
        {
            return Result;
        }
    }

    MasaCPU* CPU = &machine->CPU;
    for (size_t i = 0; i < CPU->CoreCount; i++)
    {
        MasaCPUCore* TargetCore = &CPU->Cores[i];
        if (TargetCore->UnderlyingThread != UNDERLYING_THREAD_MAIN)
        {
            Thread_Deconstruct(TargetCore->UnderlyingThread);
        }
    }

    Memory_Free(machine->RAM.Data);
    Memory_Free(machine);

    return Error_CreateSuccess();
}
