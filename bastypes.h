#ifndef BASTYPES_H
#define BASTYPES_H
#include    "arm_comm.h"

typedef
  union bauint { // Byte-addressable UINT
     u16 i; // int: Var.i
     u8 b[2]; // u char: Var.b[0]:Var.b[1]
  } bauint;

typedef
  union baulong { // Byte-addressable long
     u32 i;
     u16 a[2];
     u8 b[4];
  } baulong;

typedef union bits32{
  u32	   i;
  struct {
        unsigned b0 : 1;
        unsigned b1 : 1;
        unsigned b2 : 1;
        unsigned b3 : 1;
        unsigned b4 : 1;
        unsigned b5 : 1;
	unsigned b6 : 1;
	unsigned b7 : 1;
	unsigned b8 : 1;
	unsigned b9 : 1;
	unsigned b10 : 1;
	unsigned b11 : 1;
	unsigned b12 : 1;
	unsigned b13 : 1;
	unsigned b14 : 1;
	unsigned b15 : 1;
	unsigned b16 : 1;
	unsigned b17 : 1;
	unsigned b18 : 1;
	unsigned b19 : 1;
	unsigned b20 : 1;
	unsigned b21 : 1;
	unsigned b22 : 1;
	unsigned b23 : 1;
	unsigned b24 : 1;
	unsigned b25 : 1;
	unsigned b26 : 1;
	unsigned b27 : 1;
	unsigned b28 : 1;
	unsigned b29 : 1;
	unsigned b30 : 1;
	unsigned b31 : 1;
  } bits;
} bits32;

#endif
