/**
 * RV32I Base Integer Instruction Setに基づき実装されています。
 * 
 * メモリアドレス0x00000000から実行開始するバイナリファイル(.bin)のみ実行可能です。
 */

#include <stdio.h>
#include <stdint.h>

#define MEM_SIZE (64 * 1024 * 1024)

typedef struct {
    uint32_t opcode;
    uint32_t rd;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t funct3;
    uint32_t funct7;
    int32_t  imm;
} INSTRUCTION;

static uint8_t memory[MEM_SIZE];
static uint32_t regs[32];
static uint32_t pc;
int fin;

void init(int argc, char *argv[]) {
    for(int i=0; i<32; i++) regs[i] = 0;
    pc = 0;
    fin = 0;

    FILE *fp;
    fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        printf("%s", "error");
        fin = 1;
        return;
    }

    fread(memory, 1, MEM_SIZE, fp);
    fclose(fp);
}

uint32_t read(uint32_t addr) {
    if(addr + 3 >= MEM_SIZE) return 0;
    uint32_t byte0 = memory[addr];
    uint32_t byte1 = memory[addr + 1];
    uint32_t byte2 = memory[addr + 2];
    uint32_t byte3 = memory[addr + 3];
    return ((byte3 << 24) | (byte2 << 16) | (byte1 <<  8) | byte0);
}

uint32_t fetch(uint32_t ma) {
    return read(ma);
}

INSTRUCTION decode(uint32_t rawInst) {
    INSTRUCTION inst;
    inst.opcode = rawInst & 0x7F;
    inst.rd = (rawInst >> 7) & 0x1F;
    inst.funct3 = (rawInst >> 12) & 0x07;
    inst.rs1 = (rawInst >> 15) & 0x1F;
    inst.rs2 = (rawInst >> 20) & 0x1F;
    inst.funct7 = (rawInst >> 25) & 0x7F;
    inst.imm = 0;

    switch(inst.opcode) {
        // I-type
        case 0x13:
        case 0x03:
        case 0x67:
        case 0x73:
            inst.imm = (int32_t)rawInst >> 20;
            break;

        // S-type
        case 0x23:
            inst.imm = (int32_t)((rawInst >> 25) & 0x7F) << 5 |
                       (int32_t)((rawInst >>  7) & 0x1F);
            if(inst.imm & 0x800) inst.imm |= 0xFFFFF000;
            break;

        // B-type
        case 0x63:
            inst.imm = (int32_t)((rawInst >> 31) & 0x01) << 12 |
                       (int32_t)((rawInst >>  7) & 0x01) << 11 |
                       (int32_t)((rawInst >> 25) & 0x3F) <<  5 |
                       (int32_t)((rawInst >>  8) & 0x0F) <<  1;
            if (inst.imm & 0x1000) inst.imm |= 0xFFFFE000;
            break;

        // U-type
        case 0x17:
        case 0x37:
            inst.imm = rawInst & 0xFFFFF000;
            break;

        // J-type
        case 0x6F:
            inst.imm = (int32_t)((rawInst >> 31) & 0x01) << 20 |
                       (int32_t)((rawInst >> 12) & 0xFF) << 12 |
                       (int32_t)((rawInst >> 20) & 0x01) << 11 |
                       (int32_t)((rawInst >> 21) & 0x3FF) << 1;
            if(inst.imm & 0x100000) inst.imm |= 0xFFE00000;
            break;
    }

    return inst;
}

void execute(INSTRUCTION inst) {
    uint32_t next_pc = pc + 4;

    switch(inst.opcode) {
        case 0x37: // LUI
            regs[inst.rd] = inst.imm;
            break;

        case 0x17: // AUIPC
            regs[inst.rd] = pc + inst.imm;
            break;

        case 0x6F: // JAL
            regs[inst.rd] = pc + 4;
            next_pc = pc + inst.imm;
            break;

        case 0x67: // JALR
            regs[inst.rd] = pc + 4;
            next_pc = (regs[inst.rs1] + inst.imm) & 0xFFFFFFFE;
            break;

        case 0x63: // BRANCH
            if(inst.funct3 == 0x0) { // BEQ
                if(regs[inst.rs1] == regs[inst.rs2]) next_pc = pc + inst.imm;
            } else if(inst.funct3 == 0x1) { // BNE
                if(regs[inst.rs1] != regs[inst.rs2]) next_pc = pc + inst.imm;
            } else if(inst.funct3 == 0x4) { // BLT
                if((int32_t)regs[inst.rs1] < (int32_t)regs[inst.rs2]) next_pc = pc + inst.imm;
            } else if(inst.funct3 == 0x5) { // BGE
                if((int32_t)regs[inst.rs1] >= (int32_t)regs[inst.rs2]) next_pc = pc + inst.imm;
            } else if(inst.funct3 == 0x6) { // BLTU
                if(regs[inst.rs1] < regs[inst.rs2]) next_pc = pc + (uint32_t)inst.imm;    
            } else if(inst.funct3 == 0x7) { // BGEU
                if(regs[inst.rs1] >= regs[inst.rs2]) next_pc = pc + (uint32_t)inst.imm;    
            }
            break;

        case 0x03: // Load
        {
            uint32_t addr = regs[inst.rs1] + inst.imm;
            uint32_t data = read(addr);
            if(inst.funct3 == 0x0) { // LB
                regs[inst.rd] = (int32_t)(int8_t)(data & 0xFF);
            } else if(inst.funct3 == 0x1) { // LH
                regs[inst.rd] = (int32_t)(int16_t)(data & 0xFFFF);
            } else if(inst.funct3 == 0x2) { // LW
                regs[inst.rd] = data;
            } else if(inst.funct3 == 0x4) { // LBU
                regs[inst.rd] = data & 0xFF;
            } else if(inst.funct3 == 0x5) { // LHU
                regs[inst.rd] = data & 0xFFFF;
            }
            break;
        }

        case 0x23: // Store
        {
            uint32_t addr = regs[inst.rs1] + inst.imm;
            uint32_t val;

            if(inst.funct3 == 0x0) { // SB
                val = regs[inst.rs2] & 0xFF;
                memory[addr] = val & 0xFF;
            } else if(inst.funct3 == 0x1) {
                val = regs[inst.rs2] & 0xFFFF; // SH
                memory[addr] = val & 0xFF;
                memory[addr + 1] = (val >>  8) & 0xFF;
            } else if(inst.funct3 == 0x2) { // SW
                val = regs[inst.rs2];
                memory[addr] = val & 0xFF;
                memory[addr + 1] = (val >>  8) & 0xFF;
                memory[addr + 2] = (val >> 16) & 0xFF;
                memory[addr + 3] = (val >> 24) & 0xFF;
            }
            break;
        }

        case 0x13:
            if(inst.funct3 == 0x0) { // ADDI
                regs[inst.rd] = regs[inst.rs1] + inst.imm;
            } else if(inst.funct3 == 0x2) { // SLTI
                if((int32_t)regs[inst.rs1] < inst.imm) regs[inst.rd] = 1;
                else regs[inst.rd] = 0;
            } else if(inst.funct3 == 0x3) { // SLTIU
                if(regs[inst.rs1] < (uint32_t)inst.imm) regs[inst.rd] = 1;
                else regs[inst.rd] = 0;
            } else if(inst.funct3 == 0x4) { // XORI
                regs[inst.rd] = regs[inst.rs1] ^ inst.imm;
            } else if(inst.funct3 == 0x6) { // ORI
                regs[inst.rd] = regs[inst.rs1] | inst.imm;
            } else if(inst.funct3 == 0x7) { // ANDI
                regs[inst.rd] = regs[inst.rs1] & inst.imm;
            } else if(inst.funct3 == 0x1) { // SLLI
                regs[inst.rd] = regs[inst.rs1] << inst.rs2;
            } else if(inst.funct3 == 0x5) {
                if(inst.funct7 == 0x00) // SRLI
                    regs[inst.rd] = regs[inst.rs1] >> inst.rs2;
                else if(inst.funct7 == 0x20) // SRAI
                    regs[inst.rd] = (int32_t)regs[inst.rs1] >> inst.rs2;
            }
            break;

        case 0x33:
            if(inst.funct3 == 0x0) {
                if(inst.funct7 == 0x00) // ADD
                    regs[inst.rd] = regs[inst.rs1] + regs[inst.rs2];
                else if(inst.funct7 == 0x20) // SUB
                    regs[inst.rd] = regs[inst.rs1] - regs[inst.rs2]; 
            } else if(inst.funct3 == 0x1) { // SLL
                regs[inst.rd] = regs[inst.rs1] << (regs[inst.rs2] & 0x1F);
            } else if(inst.funct3 == 0x2) { // SLT
                if((int32_t)regs[inst.rs1] < (int32_t)regs[inst.rs2]) regs[inst.rd] = 1;
                else regs[inst.rd] = 0;
            } else if(inst.funct3 == 0x3) { // SLTU
                if(regs[inst.rs1] < regs[inst.rs2]) regs[inst.rd] = 1;
                else regs[inst.rd] = 0;
            } else if(inst.funct3 == 0x4) { // XOR
                regs[inst.rd] = regs[inst.rs1] ^ regs[inst.rs2];
            } else if(inst.funct3 == 0x5) {
                if(inst.funct7 == 0x00) // SRL
                    regs[inst.rd] = regs[inst.rs1] >> (regs[inst.rs2] & 0x1F);
                else if(inst.funct7 == 0x20) // SRA
                    regs[inst.rd] = (int32_t)regs[inst.rs1] >> (regs[inst.rs2] & 0x1F);
            } else if(inst.funct3 == 0x6) { // OR
                regs[inst.rd] = regs[inst.rs1] | regs[inst.rs2];
            } else if(inst.funct3 == 0x7) { // AND
                regs[inst.rd] = regs[inst.rs1] & regs[inst.rs2];
            }
            break;

        case 0x0F: // FENCE
            break;

        case 0x73:
            if(inst.imm == 0) { // ECALL
                if(regs[17] == 63) { // read
                    if (regs[10] == 0) {  // stdin
                        int n = fread(&memory[regs[11]], 1, regs[12], stdin);
                        regs[10] = n;
                    }
                }
                else if(regs[17] == 64) { // write
                    if(regs[10] == 1) {
                        for(uint32_t i=0;i<regs[12];i++) {
                            putchar(memory[regs[11] + i]);
                        }
                    }
                    regs[10] = regs[12];
                }
                else if(regs[17] == 93) fin = 1; // exit
            } else if(inst.imm == 1) { // EBREAK
                getchar();
            }
            break;

        default:
            break;
    }

    regs[0] = 0;
    pc = next_pc;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("%s", "error");
        return 0;
    }

    init(argc, argv);

    while(1) {
        if(fin) break;

        uint32_t rawInst = fetch(pc);

        INSTRUCTION inst = decode(rawInst);

        execute(inst);
    }

    return 0;
}