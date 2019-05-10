//
// Created by swift on 2019/5/6.
//

#ifndef SANDHOOK_NH_INST_ARM64_H
#define SANDHOOK_NH_INST_ARM64_H

#include "inst_struct_aarch64.h"
#include "../../../asm/instruction.h"
#include "../../../includes/base.h"
#include "../register/register_list_a64.h"


#define INST_A64(X) A64_##X

#define IS_OPCODE(RAW,OP) INST_A64(OP)::is(RAW)


#define DEFINE_IS(X) \
inline static bool is(InstA64& inst) { \
union { \
    InstA64 raw; \
    STRUCT_A64(X) inst; \
} inst_test; \
inst_test.raw = inst; \
return inst_test.inst.opcode == OPCODE_A64(X); \
}

#define DEFINE_IS_EXT(X, EXT_COND) \
inline static bool is(InstA64& inst) { \
union { \
    InstA64 raw; \
    STRUCT_A64(X) inst; \
} inst_test; \
inst_test.raw = inst; \
return inst_test.inst.opcode == OPCODE_A64(X) && EXT_COND; \
}

#define TEST_INST_FIELD(F,V) inst_test.inst.F == V

namespace SandHook {

    namespace Asm {

        template<typename Inst>
        class InstructionA64 : public Instruction<Inst> {
        public:

            InstructionA64() {}

            InstructionA64(Inst *inst) : Instruction<Inst>(inst) {}

            Inst mask(Inst raw) {
                return raw & *(this->get());
            }

            U32 size() override;

            static inline U32 zeroExtend32(unsigned int bits, U32 value) {
                return value << (32 - bits);
            }

            static inline S64 signExtend64(unsigned int bits, U64 value) {
                return ExtractSignedBitfield64(bits - 1, 0, value);
            }

            static inline S32 signExtend32(unsigned int bits, U32 value) {
                return ExtractSignedBitfield32(bits - 1, 0, value);
            }

            bool isPCRelAddressing() {
                return mask(PCRelAddressingFMask) == PCRelAddressingFixed;
            }

            InstType instType() override {
                return A64;
            }

            Arch arch() override {
                return arm64;
            }

        };

        enum AddrMode { Offset, PreIndex, PostIndex, NonAddrMode};

        class Operand {
        public:
            inline explicit Operand(S64 imm)
                    : immediate(imm), reg(&UnknowRegiser), shift(NO_SHIFT), extend(NO_EXTEND), shift_extend_imm(0) {}
            inline Operand(RegisterA64* reg, Shift shift = LSL, int32_t imm = 0)
                    : immediate(0), reg(reg), shift(shift), extend(NO_EXTEND), shift_extend_imm(imm) {}
            inline Operand(RegisterA64* reg, Extend extend, int32_t imm = 0)
                    : immediate(0), reg(reg), shift(NO_SHIFT), extend(extend), shift_extend_imm(imm) {}

            // =====

            bool IsImmediate() const { return reg->is(UnknowRegiser); }
            bool IsShiftedRegister() const { return  (shift != NO_SHIFT); }
            bool IsExtendedRegister() const { return (extend != NO_EXTEND); }

        public:
            S64 immediate;
            RegisterA64* reg;
            Shift shift;
            Extend extend;
            int32_t shift_extend_imm;
        };

        class MemOperand {
        public:
            inline explicit MemOperand(RegisterA64* base, Off offset = 0, AddrMode addr_mode = Offset)
                    : base(base), reg_offset(&UnknowRegiser), offset(offset), addr_mode(addr_mode), shift(NO_SHIFT),
                      extend(NO_EXTEND), shift_extend_imm(0) {}

            inline explicit MemOperand(RegisterA64* base, RegisterA64* reg_offset, Extend extend, unsigned extend_imm)
                    : base(base), reg_offset(reg_offset), offset(0), addr_mode(Offset), shift(NO_SHIFT), extend(extend),
                      shift_extend_imm(extend_imm) {}

            inline explicit MemOperand(RegisterA64* base, RegisterA64* reg_offset, Shift shift = LSL, unsigned shift_imm = 0)
                    : base(base), reg_offset(reg_offset), offset(0), addr_mode(Offset), shift(shift), extend(NO_EXTEND),
                      shift_extend_imm(shift_imm) {}

            inline explicit MemOperand(RegisterA64* base, const Operand &offset, AddrMode addr_mode = Offset)
                    : base(base), reg_offset(&UnknowRegiser), addr_mode(addr_mode) {
                if (offset.IsShiftedRegister()) {
                    reg_offset        = offset.reg;
                    shift            = offset.shift;
                    shift_extend_imm = offset.shift_extend_imm;

                    extend = NO_EXTEND;
                    this->offset = 0;
                } else if (offset.IsExtendedRegister()) {
                    reg_offset        = offset.reg;
                    extend           = offset.extend;
                    shift_extend_imm = offset.shift_extend_imm;

                    shift  = NO_SHIFT;
                    this->offset = 0;
                }
            }

            // =====

            bool IsImmediateOffset() const { return (addr_mode == Offset); }
            bool IsRegisterOffset() const { return (addr_mode == Offset); }
            bool IsPreIndex() const { return addr_mode == PreIndex; }
            bool IsPostIndex() const { return addr_mode == PostIndex; }

        public:
            RegisterA64* base;
            RegisterA64* reg_offset;
            Off offset;
            AddrMode addr_mode;
            Shift shift;
            Extend extend;
            S32 shift_extend_imm;
        };


        template <typename Inst>
        class A64_INST_PC_REL : public InstructionA64<Inst> {
        public:

            A64_INST_PC_REL();

            A64_INST_PC_REL(Inst *inst);

            virtual Off getImmPCOffset() = 0;

            virtual Addr getImmPCOffsetTarget();

        };



        class INST_A64(ADR_ADRP) : public A64_INST_PC_REL<STRUCT_A64(ADR_ADRP)> {
        public:

            enum OP {
                ADR = 0b0,
                ADRP = 0b1,
            };

            A64_ADR_ADRP();

            A64_ADR_ADRP(STRUCT_A64(ADR_ADRP) *inst);

            A64_ADR_ADRP(OP op, RegisterA64 *rd, S64 imme);

            DEFINE_IS(ADR_ADRP)

            U32 instCode() override {
                return isADRP() ? PCRelAddressingOp::ADRP : PCRelAddressingOp::ADR;
            }

            bool isADRP() {
                return get()->op == OP::ADRP;
            }

            Off getImmPCOffset() override;

            Addr getImmPCOffsetTarget() override;

            void decode(STRUCT_A64(ADR_ADRP) *decode) override;

            void assembler() override;

        public:
            OP op;
            RegisterA64* rd;
            S64 imme;
        };



        class INST_A64(MOV_WIDE) : public InstructionA64<STRUCT_A64(MOV_WIDE)> {
        public:

            enum OP {
                // Move and keep.
                MOV_WideOp_K = 0b00,
                // Move with zero.
                MOV_WideOp_Z = 0b10,
                // Move with non-zero.
                MOV_WideOp_N = 0b11,
            };

            A64_MOV_WIDE();

            A64_MOV_WIDE(STRUCT_A64(MOV_WIDE) *inst);

            A64_MOV_WIDE(A64_MOV_WIDE::OP op, RegisterA64* rd, U16 imme, U8 shift);

            DEFINE_IS(MOV_WIDE)

            inline U32 instCode() override {
                switch (op) {
                    case MOV_WideOp_K:
                        return MOVK;
                    case MOV_WideOp_N:
                        return MOVN;
                    case MOV_WideOp_Z:
                        return MOVZ;
                    default:
                        return 0;
                }
            }

            void assembler() override;

            void decode(STRUCT_A64(MOV_WIDE) *decode) override;


            inline U8 getShift() {
                return shift;
            }

            inline OP getOpt() {
                return op;
            }

            inline U16 getImme() {
                return imme;
            }

            inline Register* getRd() {
                return rd;
            }

        public:
            //can be 16/32/64/128
            //hw = shift / 16
            U8 shift;
            OP op;
            U16 imme;
            RegisterA64* rd;
        };



        class INST_A64(B_BL) : public A64_INST_PC_REL<STRUCT_A64(B_BL)> {
        public:

            enum OP {
                B = 0b0,
                BL = 0b1
            };

            A64_B_BL();

            A64_B_BL(STRUCT_A64(B_BL) *inst);

            A64_B_BL(OP op, Off offset);

            DEFINE_IS(B_BL)

            inline Off getOffset() {
                return offset;
            }

            inline OP getOP() {
                return op;
            }

            inline U32 instCode() override {
                return op == B ? UnconditionalBranchOp::B : UnconditionalBranchOp::BL;
            };

            Off getImmPCOffset() override;

            void decode(STRUCT_A64(B_BL) *decode) override;

            void assembler() override;


        public:
            OP op;
            Off offset;
        };


        class INST_A64(CBZ_CBNZ) : public A64_INST_PC_REL<STRUCT_A64(CBZ_CBNZ)> {
        public:

            enum OP {
                CBZ = 0,
                CBNZ = 1
            };

            A64_CBZ_CBNZ();

            A64_CBZ_CBNZ(STRUCT_A64(CBZ_CBNZ) *inst);

            A64_CBZ_CBNZ(OP op, Off offset, RegisterA64 *rt);

            DEFINE_IS(CBZ_CBNZ)

            inline U32 instCode() override {
                return op == CBZ ? CompareBranchOp::CBZ : CompareBranchOp::CBNZ;
            }

            Off getImmPCOffset() override;

            void decode(STRUCT_A64(CBZ_CBNZ) *inst) override;

            void assembler() override;

        public:
            OP op;
            Off offset;
            RegisterA64* rt;
        };


        class INST_A64(B_COND) : public A64_INST_PC_REL<STRUCT_A64(B_COND)> {
        public:
            A64_B_COND();

            A64_B_COND(STRUCT_A64(B_COND) *inst);

            A64_B_COND(Condition condition, Off offset);

            DEFINE_IS(B_COND)

            inline U32 instCode() override {
                return B_cond;
            }

            Off getImmPCOffset() override;

            void decode(STRUCT_A64(B_COND) *inst) override;

            void assembler() override;

        public:
            Condition condition;
            Off offset;
        };


        class INST_A64(TBZ_TBNZ) : public A64_INST_PC_REL<STRUCT_A64(TBZ_TBNZ)> {
        public:

            enum OP {
                TBZ,
                TBNZ
            };

            A64_TBZ_TBNZ();

            A64_TBZ_TBNZ(STRUCT_A64(TBZ_TBNZ) *inst);

            A64_TBZ_TBNZ(OP op, RegisterA64 *rt, U32 bit, Off offset);

            DEFINE_IS(TBZ_TBNZ)

            inline U32 instCode() override {
                return op == TBZ ? TestBranchOp::TBZ : TestBranchOp::TBNZ;
            };

            Off getImmPCOffset() override;

            void decode(STRUCT_A64(TBZ_TBNZ) *inst) override;

            void assembler() override;

        public:
            OP op;
            RegisterA64* rt;
            U32 bit;
            Off offset;
        };


        class INST_A64(LDR_LIT) : public A64_INST_PC_REL<STRUCT_A64(LDR_LIT)> {
        public:

            enum OP {
                LDR_W = 0b00,
                LDR_X = 0b01,
                LDR_SW = 0b10,
                LDR_PRFM = 0b11
            };

            A64_LDR_LIT();

            A64_LDR_LIT(STRUCT_A64(LDR_LIT) *inst);

            A64_LDR_LIT(OP op, RegisterA64 *rt, Off offset);

            DEFINE_IS(LDR_LIT)

            inline U32 instCode() override {
                switch (op) {
                    case LDR_W:
                        return LoadLiteralOp::LDR_w_lit;
                    case LDR_X:
                        return LoadLiteralOp::LDR_x_lit;
                    case LDR_SW:
                        return LoadLiteralOp::LDRSW_x_lit;
                    case LDR_PRFM:
                        return LoadLiteralOp::PRFM_lit;
                }
            }

            Off getImmPCOffset() override;

            void decode(STRUCT_A64(LDR_LIT) *inst) override;

            void assembler() override;

        public:
            OP op;
            RegisterA64* rt;
            Off offset;
        };



        class INST_A64(STR_IMM) : public InstructionA64<STRUCT_A64(STR_IMM)> {
        public:
            A64_STR_IMM();

            A64_STR_IMM(STRUCT_A64(STR_IMM) *inst);

            A64_STR_IMM(RegisterA64 &rt, const MemOperand &operand);

            A64_STR_IMM(Condition condition, RegisterA64 &rt, const MemOperand &operand);

            DEFINE_IS_EXT(STR_IMM, TEST_INST_FIELD(unkown1_0, 0) && TEST_INST_FIELD(unkown2_0, 0))

            void decode(STRUCT_A64(STR_IMM) *inst) override;

            void assembler() override;

            AddrMode getAddrMode() {
                return operand.addr_mode;
            }

        private:
            AddrMode decodeAddrMode();

        public:
            Condition condition = Condition::al;
            RegisterA64* rt;
            MemOperand operand = MemOperand(nullptr);
        private:
            bool wback;
            U32 imm32;
            bool add;
            bool index;
        };

    }

}

#endif //SANDHOOK_NH_INST_ARM64_H
