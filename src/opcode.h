#ifndef OPCODE_H__
#define OPCODE_H__

#define OP_NOP	0x00
#define OP_ADD	0x01 // binary operators
#define OP_SUB	0x02
#define OP_MUL	0x03
#define OP_DIV	0x04
#define OP_MOD	0x05
#define OP_EQ	0x06
#define OP_NEQ	0x07
#define OP_LT	0x08
#define OP_LTE	0x09
#define OP_GT	0x0a
#define OP_GTE	0x0b
#define OP_ANDL	0x0c
#define OP_ORL	0x0d
#define OP_CAT	0x0e
#define OP_LCAT 0x0f
#define OP_PSHI	0x10 // stack/variable instructions
#define OP_PSHD	0x11
#define OP_PSHS	0x12
#define OP_PSHV	0x13
#define OP_UVAL	0x14
#define OP_SET	0x15
#define OP_POP	0x16
#define OP_CLSR	0x17
#define OP_SELF	0x18
#define OP_JMP	0x20 // jump instructions
#define OP_JNZ	0x21
#define OP_JZ	0x22
#define OP_CALL	0x23
#define OP_RET	0x24
#define OP_HEAD	0x30 // list instructions
#define OP_TAIL	0x31
#define OP_PRNT	0xfe // print top of stack, for debugging
#define OP_END	0xff

#endif
