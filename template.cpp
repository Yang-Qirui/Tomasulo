#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINELENGTH 1000 /* 机器指令的最大长度 */
#define MEMSIZE 10000      /* 内存的最大容量     */
#define NUMREGS 32         /* 寄存器数量         */

/*
 * 操作码和功能码定义
 */

#define regRegALU 0 /* 寄存器-寄存器的ALU运算的操作码为0 */
#define LW 35
#define SW 43
#define ADDI 8
#define ANDI 12
#define BEQZ 4
#define J 2
#define HALT 1
#define NOOP 3
#define addFunc 32 /* ALU运算的功能码 */
#define subFunc 34
#define andFunc 36

#define NOOPINSTRUCTION 0x0c000000

/*
 * 执行单元
 */
#define LOAD1 0
#define LOAD2 1
#define STORE1 2
#define STORE2 3
#define INT1 4
#define INT2 5

#define NUMUNITS 6 /* 执行单元数量 */
char *unitname[NUMUNITS] = {"LOAD1",  "LOAD2", "STORE1",
                            "STORE2", "INT1",  "INT2"}; /* 执行单元的名称 */

/*
 * 不同操作所需要的周期数
 */
#define BRANCHEXEC 3 /* 分支操作 */
#define LDEXEC 2 /* Load     */
#define STEXEC 2 /* Store    */
#define INTEXEC 1 /* 整数运算 */

/*
 * 指令状态*/
#define ISSUE 0 /* 发射   */
#define EXECUTING 1 /* 执行   */
#define WRITINGRESULT 2 /* 写结果 */
#define COMMITTING 3 /* 提交   */
char *statename[4] = {"ISSUING", "EXECUTING", "WRITINGRESULT",
                      "COMMTITTING"}; /*  状态名称 */

#define RBSIZE 16 /* ROB有16个单元 */
#define BTBSIZE 8 /* 分支预测缓冲栈有8个单元 */

/*
 * 2 bit 分支预测状态
 */
#define STRONGNOT 0
#define WEAKTAKEN 1
#define WEAKNOT 2
#define STRONGTAKEN 3

/*
 * 分支跳转结果
 */
#define NOTTAKEN 0
#define TAKEN 1

extern "C" {
typedef struct _resStation { /* 保留栈的数据结构 */
  int instr;                 /*    指令    */
  int busy;                  /* 空闲标志位 */
  int Vj;                    /* Vj, Vk 存放操作数 */
  int Vk;
  int Qj;         /* Qj, Qk 存放将会生成结果的执行单元编号 */
  int Qk;         /* 为零则表示对应的V有效 */
  int exTimeLeft; /* 指令执行的剩余时间 */
  int reorderNum; /* 该指令对应的ROB项编号 */
} resStation;

typedef struct _reorderEntry { /* ROB项的数据结构 */
  int busy;                    /* 空闲标志位 */
  int instr;                   /* 指令 */
  int execUnit;                /* 执行单元编号 */
  int instrStatus;             /* 指令的当前状态 */
  int result;                  /* 在提交之前临时存放结果 */
  int storeAddress;            /* store指令的内存地址 */
} reorderEntry;

typedef struct _regResultEntry { /* 寄存器状态的数据结构 */
  int valid;                     /* 1表示寄存器值有效, 否则0 */
  int reorderNum; /* 如果值无效, 记录ROB中哪个项目会提交结果 */
} regResultEntry;

typedef struct _btbEntry { /* 分支预测缓冲栈的数据结构 */
  int valid;               /* 有效位 */
  int branchPC;            /* 分支指令的PC值 */
  int branchTarget;        /* when predict taken, update PC with target */
  int branchPred;          /* 预测：2-bit分支历史 */
} btbEntry;

typedef struct _machineState {       /* 虚拟机状态的数据结构 */
  int pc;                            /* PC */
  int cycles;                        /* 已经过的周期数 */
  resStation reservation[NUMUNITS];  /* 保留栈 */
  reorderEntry reorderBuf[RBSIZE];   /* ROB */
  regResultEntry regResult[NUMREGS]; /* 寄存器状态 */
  btbEntry btBuf[BTBSIZE];           /* 分支预测缓冲栈 */
  int memory[MEMSIZE];               /* 内存   */
  int regFile[NUMREGS];              /* 寄存器 */
  int headRB;
  int tailRB;
  int clear;
  int memorySize;
  int halt;
} machineState;

int field0(int);
int field1(int);
int field2(int);
int opcode(int);

void printInstruction(int);

void printState(machineState *statePtr, int memorySize) {
  int i;

  printf("Cycles: %d\n", statePtr->cycles);

  printf("\t pc=%d\n", statePtr->pc);

  printf("\t Reservation stations:\n");
  for (i = 0; i < NUMUNITS; i++) {
    if (statePtr->reservation[i].busy == 1) {
      printf("\t \t Reservation station %d: ", i);
      if (statePtr->reservation[i].Qj == -1) {
        printf("Vj = %d ", statePtr->reservation[i].Vj);
      } else {
        printf("Qj = '%s' ", unitname[statePtr->reservation[i].Qj]);
      }
      if (statePtr->reservation[i].Qk == -1) {
        printf("Vk = %d ", statePtr->reservation[i].Vk);
      } else {
        printf("Qk = '%s' ", unitname[statePtr->reservation[i].Qk]);
      }
      printf(" ExTimeLeft = %d  RBNum = %d busy = %d\n",
             statePtr->reservation[i].exTimeLeft,
             statePtr->reservation[i].reorderNum,
             statePtr->reservation[i].busy);
    }
  }

  printf("\t Reorder buffers:\n");
  for (i = 0; i < RBSIZE; i++) {
    if (statePtr->reorderBuf[i].busy == 1) {
      printf("\t \t Reorder buffer %d: ", i);
      printf(" instr %d  executionUnit '%s'  state %s  result %d "
             " storeAddress %d\n",
             statePtr->reorderBuf[i].instr,
             unitname[statePtr->reorderBuf[i].execUnit],
             statename[statePtr->reorderBuf[i].instrStatus],
             statePtr->reorderBuf[i].result,
             statePtr->reorderBuf[i].storeAddress);
    }
  }

  printf("\t Register result status:\n");
  for (i = 1; i < NUMREGS; i++) {
    if (!statePtr->regResult[i].valid) {
      printf("\t \t Register %d: ", i);
      printf("waiting for reorder buffer number %d\n",
             statePtr->regResult[i].reorderNum);
    }
  }

  /*
   * [TODO]如果你实现了动态分支预测, 将这里的注释取消
   */

  /*printf("\t Branch target buffer:\n");
  for (i=0; i<BTBSIZE; i++){
    if (statePtr->btBuf[i].valid){
      printf("\t \t Entry %d: PC=%d, Target=%d, Pred=%d\n",
       i, statePtr->btBuf[i].branchPC, statePtr->btBuf[i].branchTarget,
       statePtr->btBuf[i].branchPred);
   }
  }*/

  printf("\t Memory:\n");
  for (i = 0; i < memorySize; i++) {
    printf("\t \t memory[%d] = %d\n", i, statePtr->memory[i]);
  }

  printf("\t Registers:\n");
  for (i = 0; i < NUMREGS; i++) {
    printf("\t \t regFile[%d] = %d\n", i, statePtr->regFile[i]);
  }
}

/*
 *[TODO]
 *这里对指令进行解码，转换成程序可以识别的格式，需要根据指令格式来进行。
 *可以考虑使用高级语言中的位和逻辑运算
 */

int field0(int instruction) {
  /*
   *[TODO]
   *返回指令的第一个寄存器RS1
   */
  int reg1 = (instruction >> 21) & 0x1f;
  return reg1;
}

int field1(int instruction) {
  /*
   *[TODO]
   *返回指令的第二个寄存器，RS2或者Rd
   */
  int reg2 = (instruction >> 16) & 0x1f;
  return reg2;
}

int field2(int instruction) {
  /*
   *[TODO]
   *返回指令的第三个寄存器，Rd
   */
  int reg3 = (instruction >> 11) & 0x1f;
  return reg3;
}

int immediate(int instruction) {
  /*
   *[TODO]
   *返回I型指令的立即数部分
   */
  int imm = instruction & 0xffff;
  return imm;
}

int jumpAddr(int instruction) {
  /*
   *[TODO]
   *返回J型指令的跳转地址
   */
  int addr = instruction & 0x3ffffff;
  return addr;
}

int opcode(int instruction) {
  /*
   *[TODO]
   *返回指令的操作码
   */
  int opcode = (instruction >> 26) & 0x3f;
  return opcode;
}

int func(int instruction) {
  /*
   *[TODO]
   *返回R型指令的功能域
   */
  int func = instruction & 0x7ff;
  return func;
}

void printInstruction(int instr) {
  char opcodeString[10];
  char funcString[11];
  int funcCode;
  int op;

  if (opcode(instr) == regRegALU) {
    funcCode = func(instr);
    if (funcCode == addFunc) {
      strcpy(opcodeString, "add");
    } else if (funcCode == subFunc) {
      strcpy(opcodeString, "sub");
    } else if (funcCode == andFunc) {
      strcpy(opcodeString, "and");
    } else {
      strcpy(opcodeString, "alu");
    }
    printf("%s %d %d %d \n", opcodeString, field0(instr), field1(instr),
           field2(instr));
  } else if (opcode(instr) == LW) {
    strcpy(opcodeString, "lw");
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           immediate(instr));
  } else if (opcode(instr) == SW) {
    strcpy(opcodeString, "sw");
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           immediate(instr));
  } else if (opcode(instr) == ADDI) {
    strcpy(opcodeString, "addi");
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           immediate(instr));
  } else if (opcode(instr) == ANDI) {
    strcpy(opcodeString, "andi");
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           immediate(instr));
  } else if (opcode(instr) == BEQZ) {
    strcpy(opcodeString, "beqz");
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           immediate(instr));
  } else if (opcode(instr) == J) {
    strcpy(opcodeString, "j");
    printf("%s %d\n", opcodeString, jumpAddr(instr));
  } else if (opcode(instr) == HALT) {
    strcpy(opcodeString, "halt");
    printf("%s\n", opcodeString);
  } else if (opcode(instr) == NOOP) {
    strcpy(opcodeString, "noop");
    printf("%s\n", opcodeString);
  } else {
    strcpy(opcodeString, "data");
    printf("%s %d\n", opcodeString, instr);
  }
}

int convertNum16(int num) {
  /* convert an 16 bit number into a 32-bit or 64-bit number */
  if (num & 0x8000) {
    num -= 65536;
  }
  return (num);
}

int convertNum26(int num) {
  /* convert an 26 bit number into a 32-bit or 64-bit number */
  if (num & 0x200000) {
    num -= 67108864;
  }
  return (num);
}

void updateRes(int unit, machineState *statePtr, int value) {
  /*
   *[TODO]
   * 更新保留栈:
   * 将位于公共数据总线上的数据
   * 复制到正在等待它的其他保留栈中去
   */
  for (int i = 0; i < NUMUNITS; i++) {
    if (statePtr->reservation[i].Qj == unit) {
      statePtr->reservation[i].Vj = value;
      statePtr->reservation[i].Qj = -1;
    }
    if (statePtr->reservation[i].Qk == unit) {
      statePtr->reservation[i].Vk = value;
      statePtr->reservation[i].Qk = -1;
    }
  }
}

void issueInstr(int instr, int unit, machineState *statePtr, int reorderNum) {

  /*
   * [TODO]
   * 发射指令:
   * 填写保留栈和ROB项的内容.
   * 注意, 要在所有的字段中写入正确的值.
   * 检查寄存器状态, 相应的在Vj,Vk和Qj,Qk字段中设置正确的值:
   * 对于I类型指令, 设置Qk=0,Vk=0;
   * 对于sw指令, 如果寄存器有效, 将寄存器中的内存基地址保存在Vj中;
   * 对于beqz和j指令, 将当前PC+1的值保存在Vk字段中.
   * 如果指令在提交时会修改寄存器的值, 还需要在这里更新寄存器状态数据结构.
   */
  statePtr->reservation[unit].instr = instr;
  statePtr->reservation[unit].reorderNum = reorderNum;
  statePtr->reservation[unit].busy = 1;
  statePtr->reorderBuf[reorderNum].instr = instr;
  statePtr->reorderBuf[reorderNum].execUnit = unit;
  statePtr->reorderBuf[reorderNum].instrStatus = ISSUE;
  statePtr->reorderBuf[reorderNum].busy = 1;
  int op = opcode(instr);
  if (op == LW) {
    if (statePtr->regResult[field0(instr)].valid == 1) {
      statePtr->reservation[unit].Vj = statePtr->regFile[field0(instr)];
      statePtr->reservation[unit].Qj = -1;
    } else {
      statePtr->reservation[unit].Qj =
          statePtr->regResult[field0(instr)].reorderNum;
    }
    statePtr->reservation[unit].Vk = 0;
    statePtr->reservation[unit].Qk = -1;
    statePtr->reservation[unit].exTimeLeft = LDEXEC;
    statePtr->regResult[field1(instr)].valid = 0;
    statePtr->regResult[field1(instr)].reorderNum = reorderNum;
  } else if (op == SW) {
    if (statePtr->regResult[field0(instr)].valid == 1) {
      statePtr->reservation[unit].Vj = statePtr->regFile[field0(instr)];
      statePtr->reservation[unit].Qj = -1;
    } else {
      statePtr->reservation[unit].Qj =
          statePtr->regResult[field0(instr)].reorderNum;
    }
    if (statePtr->regResult[field1(instr)].valid == 1) {
      statePtr->reservation[unit].Vk = statePtr->regFile[field1(instr)];
      statePtr->reservation[unit].Qk = -1;
    } else {
      statePtr->reservation[unit].Qk =
          statePtr->regResult[field1(instr)].reorderNum;
    }
    statePtr->reservation[unit].exTimeLeft = STEXEC;
  } else if (op == regRegALU) {
    /* R型指令*/
    if (statePtr->regResult[field0(instr)].valid == 1) {
      statePtr->reservation[unit].Vj = statePtr->regFile[field0(instr)];
      statePtr->reservation[unit].Qj = -1;
    } else {
      statePtr->reservation[unit].Qj =
          statePtr->regResult[field0(instr)].reorderNum;
    }
    if (statePtr->regResult[field1(instr)].valid == 1) {
      statePtr->reservation[unit].Vk = statePtr->regFile[field1(instr)];
      statePtr->reservation[unit].Qk = -1;
    } else {
      statePtr->reservation[unit].Qk =
          statePtr->regResult[field1(instr)].reorderNum;
    }
    statePtr->reservation[unit].exTimeLeft = INTEXEC;
    statePtr->regResult[field2(instr)].valid = 0;
    statePtr->regResult[field2(instr)].reorderNum = reorderNum;
  } else if (op == ADDI || op == ANDI) {
    if (statePtr->regResult[field0(instr)].valid == 1) {
      statePtr->reservation[unit].Vj = statePtr->regFile[field0(instr)];
      statePtr->reservation[unit].Qj = -1;
    } else {
      statePtr->reservation[unit].Qj =
          statePtr->regResult[field0(instr)].reorderNum;
    }
    statePtr->reservation[unit].Vk = 0;
    statePtr->reservation[unit].Qk = -1;
    statePtr->reservation[unit].exTimeLeft = INTEXEC;
    statePtr->regResult[field1(instr)].valid = 0;
    statePtr->regResult[field1(instr)].reorderNum = reorderNum;
  } else if (op == BEQZ) {
    if (statePtr->regResult[field0(instr)].valid) {
      statePtr->reservation[unit].Vj = statePtr->regFile[field0(instr)];
      statePtr->reservation[unit].Qj = -1;
    } else {
      statePtr->reservation[unit].Qj =
          statePtr->regResult[field0(instr)].reorderNum;
    }
    statePtr->reservation[unit].Vk = statePtr->pc + 1;
    statePtr->reservation[unit].Qk = -1;
    statePtr->reservation[unit].exTimeLeft = BRANCHEXEC;
  } else if (op == J) {
    statePtr->reservation[unit].Vj = 0;
    statePtr->reservation[unit].Qj = -1;
    statePtr->reservation[unit].Vk = statePtr->pc + 1;
    statePtr->reservation[unit].Qk = -1;
    statePtr->reservation[unit].exTimeLeft = INTEXEC;
  } else {
    statePtr->reservation[unit].Vj = 0;
    statePtr->reservation[unit].Qj = -1;
    statePtr->reservation[unit].Vk = 0;
    statePtr->reservation[unit].Qk = -1;
    statePtr->reservation[unit].exTimeLeft = INTEXEC;
  }
}

int checkReorder(machineState *statePtr) {
  /*
   * [TODO]
   * 在ROB的队尾检查是否有空闲的空间, 如果有, 返回空闲项目的编号.
   * ROB是一个循环队列, 它可以容纳RBSIZE个项目.
   * 新的指令被添加到队列的末尾, 指令提交则是从队首进行的.
   * 当队列的首指针或尾指针到达数组中的最后一项时, 它应滚动到数组的第一项.
   */
  if (statePtr->tailRB - statePtr->headRB == RBSIZE) {
    return -1;
  } else {
    return statePtr->tailRB;
  }
}

int getResult(resStation rStation, machineState *statePtr) {
  int op, immed, function, address;

  /*
   * [TODO]
   * 这个函数负责计算有输出的指令的结果.
   * 你需要完成下面的case语句....
   */

  op = opcode(rStation.instr);
  immed = immediate(rStation.instr);
  function = func(rStation.instr);
  address = jumpAddr(rStation.instr);

  if (op == ANDI) {
    return rStation.Vj & convertNum16(immed);
  } else if (op == ADDI) {
    return rStation.Vj + convertNum16(immed);
  } else if (op == regRegALU) {
    if (function == addFunc) {
      return rStation.Vj + rStation.Vk;
    } else if (function == subFunc) {
      return rStation.Vj - rStation.Vk;
    } else if (function == andFunc) {
      return rStation.Vj & rStation.Vk;
    }
  } else if (op == LW) {
    return statePtr->memory[rStation.Vj + convertNum16(immed)];
  } else if (op == SW) {
    statePtr->reorderBuf[rStation.reorderNum].storeAddress =
        rStation.Vj + convertNum16(immed);
    return rStation.Vk;
  } else if (op == BEQZ) {
    if (rStation.Vj == 0) {
      return rStation.Vk + convertNum16(immed);
    }
    return -1;
  } else if (op == J) {
    return rStation.Vk + convertNum26(address);
  } else {
    return -1;
  }
}

/* 选作内容 */
int getPrediction(machineState *statePtr) {
  /*
   * [TODO]
   * 对给定的PC, 检查分支预测缓冲栈中是否有历史记录
   * 如果有, 返回根据历史信息进行的预测, 否则返回-1
   */
}

/* 选作内容 */
void updateBTB(machineState *statePtr, int branchPC, int targetPC,
               int outcome) {
  /*
   * [TODO]
   * 更新分支预测缓冲栈: 检查是否与缓冲栈中的项目匹配.
   * 如果是, 对2-bit的历史记录进行更新;
   * 如果不是, 将当前的分支语句添加到缓冲栈中去.
   * 如果缓冲栈已满，你需要选择一种替换算法将旧的记录替换出去.
   * 如果当前跳转成功, 将初始的历史状态设置为STRONGTAKEN;
   * 如果不成功, 将历史设置为STRONGNOT
   */
}

/* 选作内容 */
int getTarget(machineState *statePtr, int reorderNum) {
  /*
   * [TODO]
   * 检查分支指令是否已保存在分支预测缓冲栈中:
   * 如果不是, 返回当前pc+1, 这意味着我们预测分支跳转不会成功;
   * 如果在, 并且历史信息为STRONGTAKEN或WEAKTAKEN, 返回跳转的目标地址,
   * 如果历史信息为STRONGNOT或WEAKNOT, 返回当前pc+1.
   */
}

machineState init(char *path) {
  FILE *filePtr;
  int pc, done, instr, i;
  char line[MAXLINELENGTH];
  machineState *statePtr;

  filePtr = fopen(path, "r");
  if (filePtr == NULL) {
    printf("error: can't open file %s", path);
    perror("fopen");
    exit(1);
  }

  statePtr = (machineState *)malloc(sizeof(machineState));

  for (i = 0; i < MEMSIZE; i++) {
    statePtr->memory[i] = 0;
  }
  pc = 16;
  done = 0;
  while (!done) {
    if (fgets(line, MAXLINELENGTH, filePtr) == NULL) {
      done = 1;
    } else {
      if (sscanf(line, "%d", &instr) != 1) {
        printf("error in reading address %d\n", pc);
        exit(1);
      }

      statePtr->memory[pc] = instr;
      printf("memory[%d]=%d\n", pc, statePtr->memory[pc]);
      pc = pc + 1;
    }
  }

  statePtr->memorySize = pc;

  /*
   * 状态初始化
   */

  statePtr->pc = 16;
  statePtr->cycles = 0;
  for (i = 0; i < NUMREGS; i++) {
    statePtr->regFile[i] = 0;
  }
  for (i = 0; i < NUMUNITS; i++) {
    statePtr->reservation[i].busy = 0;
  }
  for (i = 0; i < RBSIZE; i++) {
    statePtr->reorderBuf[i].busy = 0;
  }

  statePtr->headRB = 0;
  statePtr->tailRB = 0;
  statePtr->clear = 0;
  statePtr->halt = 0;

  for (i = 0; i < NUMREGS; i++) {
    statePtr->regResult[i].valid = 1;
  }
  for (i = 0; i < BTBSIZE; i++) {
    statePtr->btBuf[i].valid = 0;
  }
  return *statePtr;
}

machineState run_one_tick(machineState presentPtr) {
  // printf("0");
  machineState newState = presentPtr;
  machineState *statePtr = &presentPtr;
  // printf("1");
  printState(statePtr, statePtr->memorySize);
  if (statePtr->reorderBuf[statePtr->headRB].instrStatus == COMMITTING) {
    if (opcode(statePtr->reorderBuf[statePtr->headRB].instr) == J ||
        (opcode(statePtr->reorderBuf[statePtr->headRB].instr) == BEQZ &&
         statePtr->clear == 1)) {
      /* 条件跳转利用bool确定是否跳 */
      statePtr->pc = statePtr->reorderBuf[statePtr->headRB].result;
      // for (int i = 0; i < RBSIZE;i++){
      //     statePtr->reorderBuf[i].busy = 0;
      // }
      // for (int i = 0; i < NUMUNITS; i++) {
      //   statePtr->reservation[i].busy = 0;
      // }
      memset(statePtr->reorderBuf, 0, sizeof(reorderEntry) * RBSIZE);
      memset(statePtr->reservation, 0, sizeof(resStation) * NUMUNITS);
      for (int i = 0; i < NUMREGS; i++) {
        statePtr->regResult[i].valid = 1;
      }
      statePtr->headRB = 0;
      statePtr->tailRB = 0;
      statePtr->clear = 0;
      return *statePtr;
    } else {
      /* 这里需要再update一次，考虑：先writing
       result然后紧接issue一个有数据相关的指令*/
      int op = opcode(statePtr->reorderBuf[statePtr->headRB].instr);
      if (op == HALT) {
        statePtr->halt = 1;
        return *statePtr;
      } else if (op == LW) {
        int regNum = field1(statePtr->reorderBuf[statePtr->headRB].instr);
        statePtr->regFile[regNum] =
            statePtr->reorderBuf[statePtr->headRB].result;
        if (statePtr->regResult[regNum].reorderNum == statePtr->headRB)
          statePtr->regResult[regNum].valid = 1;
      } else if (op == SW) {
        statePtr->memory[statePtr->reorderBuf[statePtr->headRB].storeAddress] =
            statePtr->reorderBuf[statePtr->headRB].result;
      } else if (op == regRegALU) {
        int regNum = field2(statePtr->reorderBuf[statePtr->headRB].instr);
        statePtr->regFile[regNum] =
            statePtr->reorderBuf[statePtr->headRB].result;
        if (statePtr->regResult[regNum].reorderNum == statePtr->headRB)
          statePtr->regResult[regNum].valid = 1;
      } else if (op == ADDI || op == ANDI) {
        int regNum = field1(statePtr->reorderBuf[statePtr->headRB].instr);
        statePtr->regFile[regNum] =
            statePtr->reorderBuf[statePtr->headRB].result;
        if (statePtr->regResult[regNum].reorderNum == statePtr->headRB)
          statePtr->regResult[regNum].valid = 1;
      }
      updateRes(statePtr->headRB, statePtr,
                statePtr->reorderBuf[statePtr->headRB].result);
      statePtr->reorderBuf[statePtr->headRB].busy = 0;
      statePtr->headRB += 1;
      statePtr->headRB %= RBSIZE;
    }
  }
  // printf("2");
  for (int i = 0; i < RBSIZE; i++) {
    if (statePtr->reorderBuf[i].busy == 1) {
      int status = statePtr->reorderBuf[i].instrStatus;
      if (status == ISSUE) {
        if (statePtr->reservation[statePtr->reorderBuf[i].execUnit].Qj == -1 &&
            statePtr->reservation[statePtr->reorderBuf[i].execUnit].Qk == -1) {
          statePtr->reorderBuf[i].instrStatus = EXECUTING;
        }
      } else if (status == EXECUTING) {
        statePtr->reservation[statePtr->reorderBuf[i].execUnit].exTimeLeft -= 1;
        if (statePtr->reservation[statePtr->reorderBuf[i].execUnit]
                .exTimeLeft == 0) {
          statePtr->reorderBuf[i].result =
              getResult(statePtr->reservation[statePtr->reorderBuf[i].execUnit],
                        statePtr);
          if (statePtr->reorderBuf[i].result != -1 &&
              opcode(statePtr->reorderBuf[i].instr) == BEQZ) {
            statePtr->clear = 1;
          }
          statePtr->reorderBuf[i].instrStatus = WRITINGRESULT;
        }
      } else if (status == WRITINGRESULT) {
        updateRes(i, statePtr, statePtr->reorderBuf[i].result);
        statePtr->reservation[statePtr->reorderBuf[i].execUnit].busy = 0;
        statePtr->reorderBuf[i].instrStatus = COMMITTING;
      }
    }
  }
  // printf("3");
  if (statePtr->pc < statePtr->memorySize) {
    int rob = checkReorder(statePtr);
    if (rob != -1) {
      int newInstr = statePtr->memory[statePtr->pc];
      int op = opcode(newInstr);
      int validRes = 0;
      int resNum = 0;
      if (op == LW) {
        if (statePtr->reservation[0].busy == 0) {
          validRes = 1;
          resNum = 0;
        } else if (statePtr->reservation[1].busy == 0) {
          validRes = 1;
          resNum = 1;
        }
      } else if (op == SW) {
        if (statePtr->reservation[2].busy == 0) {
          validRes = 1;
          resNum = 2;
        } else if (statePtr->reservation[3].busy == 0) {
          validRes = 1;
          resNum = 3;
        }
      } else {
        if (statePtr->reservation[4].busy == 0) {
          validRes = 1;
          resNum = 4;
        } else if (statePtr->reservation[5].busy == 0) {
          validRes = 1;
          resNum = 5;
        }
      }
      if (validRes) {
        issueInstr(newInstr, resNum, statePtr, rob);
        statePtr->tailRB += 1;
        statePtr->tailRB %= RBSIZE;
        statePtr->pc += 1;
      }
    }
  }
  // printf("4");
  statePtr->cycles += 1;
  return *statePtr;
}
}
