#ifndef __ASMBUILDER_H__
#define __ASMBUILDER_H__

#include "MachineCode.h"

/*AsmBuilder.h 中为汇编代码构造辅助类。其主要作用就是在中间代码向目标代码进行自顶向下
的转换时，记录当前正在进行转换操作的对象，以进行函数、基本块及指令的插入。*/
class AsmBuilder
{
public:
    MachineUnit* mUnit;  // mahicne unit
    MachineFunction* mFunction; // current machine code function;
    MachineBlock* mBlock; // current machine code block;
    int cmpOpcode; // CmpInstruction opcode, for CondInstruction;
    void setUnit(MachineUnit* unit) { this->mUnit = unit; };
    void setFunction(MachineFunction* func) { this->mFunction = func; };
    void setBlock(MachineBlock* block) { this->mBlock = block; };
    void setCmpOpcode(int opcode) { this->cmpOpcode = opcode; };
    MachineUnit* getUnit() { return this->mUnit; };
    MachineFunction* getFunction() { return this->mFunction; };
    MachineBlock* getBlock() { return this->mBlock; };
    int getCmpOpcode() { return this->cmpOpcode; };
};


#endif