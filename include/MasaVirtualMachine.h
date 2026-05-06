#pragma once
#include <stdint.h>
#include <stddef.h>
#include "wr/WRError.h"
#include "wr/WRMemory.h"


/**
 * Virtual machine called "Masa" which does stuffs.
 * This module was part of an initial idea for ErrDLogiPTC, but was scrapped due to issues with getting it to work.
 * Maybe in the future... :)
 */

#define NANOSECONDS_IN_SECOND (1'000'000'000)

#define MASA_BITS 64
#define MASA_BYTE_COUNT (MASA_BITS / 8)
#define MASA_REGISTERS_COUNT 8
#define MASA_INSTRUCTION_BYTE_COUNT 4
#define MASA_MAX_RAM_SIZE_BYTES ((size_t)1024 * (size_t)1024 * (size_t)1024 * (size_t)4)
#define MASA_MAX_CORE_COUNT 16
#define MASA_MAX_NANOSECONDS_PER_TICK (NANOSECONDS_IN_SECOND * 1)
#define MASA_INSTRUCTION_REGISTER_COUNT 4

#define ITEGER_UNIT_DATA_TYPE_INT8 1
#define ITEGER_UNIT_DATA_TYPE_INT16 2
#define ITEGER_UNIT_DATA_TYPE_INT32 3
#define ITEGER_UNIT_DATA_TYPE_INT64 4
#define ITEGER_UNIT_DATA_TYPE_UINT8 5
#define ITEGER_UNIT_DATA_TYPE_UINT16 6
#define ITEGER_UNIT_DATA_TYPE_UINT32 7
#define ITEGER_UNIT_DATA_TYPE_UINT64 8

#define FLOAT_UNIT_DATA_TYPE_FLOAT 1
#define FLOAT_UNIT_DATA_TYPE_DOUBLE 2

#define OPCODE_NONE 0

#define OPCODE_ADD_INT 1
#define OPCODE_SUB_INT 2
#define OPCODE_MUL_INT 3
#define OPCODE_DIV_INT 4
#define OPCODE_MOD_INT 5

#define OPCODE_



// Types.
typedef struct MasaVirtualMachineStruct MasaVirtualMachine;

typedef struct MasaConstructOptionsStruct
{
    size_t CoreCount;
    size_t RamByteCount;
    size_t MinNanoSecondsPerTick;
} MasaConstructOptions;


// Functions.
static inline MasaConstructOptions MasaConstructOptions_Create(size_t coreCount, size_t ramByteCount, size_t minNanoSecondsPerTick)
{
    return (MasaConstructOptions)
    {
        .CoreCount = coreCount,
        .RamByteCount = ramByteCount,
        .MinNanoSecondsPerTick = minNanoSecondsPerTick,
    };
}


Error Masa_Create(MasaVirtualMachine** outMachine, MasaConstructOptions options);

Error Masa_Start(MasaVirtualMachine* machine);

Error Masa_Stop(MasaVirtualMachine* machine);

Error Masa_Tick(MasaVirtualMachine* machine);

Error Masa_Deconstruct(MasaVirtualMachine* machine);

bool Masa_IsRunning(MasaVirtualMachine* machine);

static inline void Masa_InstructTwoInt(unsigned char* dest, 
    unsigned char opcode,
    unsigned char registerIndexA,
    unsigned char registerIndexB,
    unsigned char dataType)
{
    Memory_Zero(dest, MASA_INSTRUCTION_BYTE_COUNT * 3);
    dest[0] = opcode;
    dest[1] = registerIndexA;
    dest[2] = registerIndexB;
    dest[3] = dataType;
}

static inline void Masa_InstructNoOp(unsigned char* dest)
{
    Memory_Zero(dest, MASA_INSTRUCTION_BYTE_COUNT);
    dest[0] = OPCODE_NONE;
}

static inline void Masa_InstructAddInt(unsigned char* dest, 
    unsigned char registerIndexA,
    unsigned char registerIndexB,
    unsigned char dataType)
{
    Masa_InstructTwoInt(dest, OPCODE_ADD_INT, registerIndexA, registerIndexB, dataType);
}

static inline void Masa_InstructSubInt(unsigned char* dest,  
    unsigned char registerIndexA,
    unsigned char registerIndexB,
    unsigned char dataType)
{
    Masa_InstructTwoInt(dest, OPCODE_SUB_INT, registerIndexA, registerIndexB, dataType);
}

static inline void Masa_InstructMulInt(unsigned char* dest, 
    unsigned char registerIndexA,
    unsigned char registerIndexB,
    unsigned char dataType)
{
    Masa_InstructTwoInt(dest, OPCODE_MUL_INT, registerIndexA, registerIndexB, dataType);
}

static inline void Masa_InstructDivInt(unsigned char* dest, 
    unsigned char registerIndexA,
    unsigned char registerIndexB,
    unsigned char dataType)
{
    Masa_InstructTwoInt(dest, OPCODE_DIV_INT, registerIndexA, registerIndexB, dataType);
}

static inline void Masa_InstructModInt(unsigned char* dest, 
    unsigned char registerIndexA,
    unsigned char registerIndexB,
    unsigned char dataType)
{
    Masa_InstructTwoInt(dest, OPCODE_MOD_INT, registerIndexA, registerIndexB, dataType);
}

