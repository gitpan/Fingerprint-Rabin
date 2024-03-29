/***********************************************************************

$Id: rabin64.c,v 1.3 2000/09/05 11:39:13 borud Exp $

 File:   fingerprint.c
 Author: Mark Mitchell
 Date:   05/31/1998

 Contents: A port of the Modula-3 fingerpinting module to C.

 Copyright (c) 1998 by Mark Mitchell.  All rights reserved.

 Redistribution and use in source and binary forms are permitted
 provided that the above copyright notice and this paragraph are
 duplicated in all such forms.  The name Mark Mitchell may not be used
 to endorse or promote products derived from this software without
 specific prior written permission.

 THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT AND EXPRESS OR
 IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.

***********************************************************************/

/***********************************************************************
  Included Files
***********************************************************************/

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "rabin64.h"

/***********************************************************************
  Macros
***********************************************************************/

/* FINGERPRINT_INT_32_TYPE is a 32-bit signed integral type.  */

#ifndef FINGERPRINT_INT_32_TYPE
#define FINGERPRINT_INT_32_TYPE int
#endif /* ifndef FINGERPRINT_INT_32_TYPE */

/* FINGERPRINT_POINTER_INT_TYPE is an integral type with the same
   number of bits as pointers.  */

#ifndef FINGERPRINT_POINTER_INT_TYPE
#define FINGERPRINT_POINTER_INT_TYPE int
#endif /* ifndef FINGERPRINT_POINTER_INT_TYPE */

/* FINGERPRINT_LITTLE_ENDIAN is 1 if the target is little-endian, 0 if
   it is big-endian, and undefined otherwise.  */

#if FINGERPRINT_USE_INTEGRAL_TYPE
#ifdef FINGERPRINT_LITTLE_ENDIAN
#error "You must not define FINGERPRINT_LITTLE_ENDIAN if you define FINGERPRINT_INTEGRAL_TYPE"
#else /* ifndef FINGERPRINT_LITTLE_ENDIAN */
#define FINGERPRINT_LITTLE_ENDIAN 1
#endif /* ifdef FINGERPRINT_LITTLE_ENDIAN */
#endif /* FINGERPRINT_USE_INTEGRAL_TYPE */

/* MAY_BE_LITTLE_ENDIAN is non-zero if the target might be
   little-endian.  */

#if !defined(FINGERPRINT_LITTLE_ENDIAN) || FINGERPRINT_LITTLE_ENDIAN
#define MAY_BE_LITTLE_ENDIAN 1
#else /* !(!defined(FINGERPRINT_LITTLE_ENDIAN)
           || FINGERPRINT_LITTLE_ENDIAN) */
#define MAY_BE_LITTLE_ENDIAN 0
#endif /* (!defined(FINGERPRINT_LITTLE_ENDIAN)
           || FINGERPRINT_LITTLE_ENDIAN) */

/* MAY_BE_LITTLE_ENDIAN is non-zero if the target might be
   big-endian.  */

#if !defined(FINGERPRINT_LITTLE_ENDIAN) || !FINGERPRINT_LITTLE_ENDIAN
#define MAY_BE_BIG_ENDIAN 1
#else /* !(!defined(FINGERPRINT_LITTLE_ENDIAN)
           || 1FINGERPRINT_LITTLE_ENDIAN) */
#define MAY_BE_BIG_ENDIAN 0
#endif /* (!defined(FINGERPRINT_LITTLE_ENDIAN)
           || FINGERPRINT_LITTLE_ENDIAN) */

/***********************************************************************
  Types
***********************************************************************/

/* The Modula-3 INTEGER type is the type of all integers represented
   by the implementation.  Implicit in the Modula-3 fingerprinting
   algorithm is the idea that this type has the same size as a
   pointer as the code contains:

     LOOPHOLE (addr, INTEGER)

   at one point.  */

typedef FINGERPRINT_POINTER_INT_TYPE integer_t;

/* A fingerprint_int_32_t is a 32-bit integral type:

     TYPE Int32 = [-16_7fffffff - 1 .. 16_7fffffff].  */

typedef FINGERPRINT_INT_32_TYPE
                int_32_t;

#if !FINGERPRINT_USE_INTEGRAL_TYPE
/* A fingerprint_poly_t is a polynomial of degree [0..63] with
   coefficients in the field Z[2]:

     TYPE T = ARRAY [0..1] OF Int32;

   Because arrays are not first-class types in C, we enclose the array
   in a structure type.

   The coefficients of a polynomial are in reverse (VAX) order: P(x) =
   x^64 + c[0] * x^63 + c[1] * x^62 + ... + c[63].

   The leading coefficient is not stored in the 64-bit representation
   of the basis polynomial.  All other polynomials are residues MOD the
   basis, hence they don't have an x^64 term.  */

typedef struct poly_t {
  int_32_t      array[2];
} poly_t;
#else /* FINGERPRINT_USE_INTEGRAL_TYPE */
typedef FINGERPRINT_INTEGRAL_TYPE poly_t;
#endif /* !FINGERPRINT_USE_INTEGRAL_TYPE */

/* We carefully use only names beginning with "fingerprint" in the
   external interface, but there is no reason to be so careful
   internally.  */

typedef fingerprint_byte_t byte_t;
typedef fingerprint_word_t word_t;

/***********************************************************************
  Static Function Definitions
***********************************************************************/

/* The Modula-3 `MOD' operator is such that `X MOD Y' is non-negative
   if Y is positive and non-positive if Y is negative.  C's `%'
   operator makes no such guarantee.  This function computes the
   Modula-3 MOD operation.  If your C compiler explicitly ensures a
   Modula-3-like `%' operator, you might be able to improve
   performance by replacing this function with a macro.  */

static integer_t mod (integer_t x, integer_t y)
{
  const int positive = (y > 0);
  /* We cannot use the library function `abs' to do these conversions
     since we don't know that integer_t is `int' which is the type of
     `abs'.  */
  if (x < 0)
    x = -x;
  if (y < 0)
    y = -y;
  return positive ? (x % y) : - (x % y);
}

/***********************************************************************
  Modula-3 `Word' Module
***********************************************************************/

/* (x + y) MOD 2^[Word.Size] */
#define word_plus(x, y) (((word_t) (x)) + ((word_t) (y)))

/* (x * y) MOD 2^[Word.Size] */
#define word_times(x, y) (((word_t) (x)) * ((word_t) (y)))

/* Bitwise AND of X and Y.  */
#define word_and(x, y) (((word_t) (x)) & ((word_t) (y)))

/* Bitwise OR of X and Y.  */
#define word_or(x, y) (((word_t) (x)) | ((word_t) (y)))

/* Bitwise XOR of X and Y.  */
#define word_xor(x, y) (((word_t) (x)) ^ ((word_t) (y)))

/* Bitwise complement of X.  */
#define word_not(x) (~((word_t) (x)))

/* The Modula-3 versions of word_left_shift and word_right_shift are
   in terms of Word.Shift.  Here, we code them directly.  Note that
   C's right-shift operator does not necessarily behave as desired,
   unless the operand is unsigned.  */
#define word_left_shift(x, n) (((word_t) (x)) << n)
#define word_right_shift(x, n) (((word_t) (x)) >> n)

/* Take N bits from X, with bit I as the least significant bit, and
   return them as the least significant N bits of a word whose other
   bits are 0.  */
#define word_extract(x, i, n) \
  ((((word_t) (x)) >> i) & ~(((word_t) -1) << n))

/***********************************************************************
  Modula-3 `Poly' Module
***********************************************************************/

/* Mask to grab 32 significant bits.  */

#define POLY_SIG_BITS 0xffffffff

#if !FINGERPRINT_USE_INTEGRAL_TYPE

/* Return the Ith half of T.  */

#define POLY_HALF(t, i) ((t).array[i])

/* Form an initialization expression for the polynomial whose first
   word is T0 and whose second is T1.  */

#define POLY_INIT(t0, t1) {{ (t0), (t1) }}

/* Construct in T the polynomial whose first word is T0 and whose
   second is T1.  */

#define POLY_FORM(t, t0, t1) \
  (POLY_HALF (t, 0) = t0, POLY_HALF (t, 1) = t1)

#else /* FINGERPRINT_USE_INTEGRAL_TYPE */

#define POLY_HALF(t, i) (((int_32_t*) &(t))[i])
#define POLY_INIT(t0, t1) \
  ((((poly_t) (t1)) << 32) | (((poly_t) (t0)) & POLY_SIG_BITS))
#define POLY_FORM(t, t0, t1) ((t) = POLY_INIT ((t0), (t1)))

#endif /* !FINGERPRINT_USE_INTEGRAL_TYPE */

/***********************************************************************
  Types
***********************************************************************/

/* TYPE IntPtr = UNTRACED REF Int32 */

typedef int_32_t*
                int_ptr_t;

/* TYPE IntBytes = RECORD b0, b1, b2, b2: Byte END;

   The Modula-3 code assumes that it can treat these bytes as an Int32
   as well.  We therefore use a union; that guarantees that
   Int32-accesses will not be misaligned.  Note: this use of unions
   (to preform "type punning") results in implementation-defined
   behavior in ANSI/ISO C.  */

typedef union int_bytes_t {
  int_32_t      w;
  byte_t        b[4];
} int_bytes_t;

/***********************************************************************
  Variables
***********************************************************************/

#ifndef FINGERPRINT_LITTLE_ENDIAN
static int poly_little_endian;
static int poly_big_endian;
static int poly_init_done;
#else /* ifdef FINGERPRINT_LITTLE_ENDIAN */
#define poly_little_endian FINGERPRINT_LITTLE_ENDIAN
#endif /* ifndef FINGERPRINT_LITTLE_ENDIAN */

/* ONE = T { 0, FIRST (Int32) }; */

static const poly_t
                POLY_ONE = POLY_INIT (0, (-0x7fffffff - 1));

static const poly_t
                poly64[256]
                        /* poly64[i] = i(x) * x^64 MOD P */
                        = {POLY_INIT (0, 0),
                           POLY_INIT (36728807, 152935311),
                           POLY_INIT (73457614, 305870622),
                           POLY_INIT (105951273, 455519377),
                           POLY_INIT (85802743, 386180924),
                           POLY_INIT (120410384, 504970419),
                           POLY_INIT (25026873, 88051746),
                           POLY_INIT (55414494, 203557805),
                           POLY_INIT (132578437, 494634872),
                           POLY_INIT (97948514, 342236407),
                           POLY_INIT (59122507, 255897702),
                           POLY_INIT (28724396, 106769385),
                           POLY_INIT (50053746, 176103492),
                           POLY_INIT (13348245, 56802251),
                           POLY_INIT (110828988, 407115610),
                           POLY_INIT (78344795, 291081429),
                           POLY_INIT (34699361, 159762416),
                           POLY_INIT (2168710, 9985151),
                           POLY_INIT (108111791, 465498350),
                           POLY_INIT (71419976, 312699745),
                           POLY_INIT (118245014, 511795404),
                           POLY_INIT (87828849, 396163907),
                           POLY_INIT (57448792, 213538770),
                           POLY_INIT (22869695, 94882909),
                           POLY_INIT (100107492, 352206984),
                           POLY_INIT (130542339, 501472007),
                           POLY_INIT (26696490, 113604502),
                           POLY_INIT (61289677, 265874457),
                           POLY_INIT (15383059, 66774964),
                           POLY_INIT (47896052, 182942779),
                           POLY_INIT (76178909, 297914538),
                           POLY_INIT (112855610, 417090341),
                           POLY_INIT (69398722, 319524832),
                           POLY_INIT (101883685, 437654639),
                           POLY_INIT (4337420, 19970302),
                           POLY_INIT (41042155, 137175921),
                           POLY_INIT (20958773, 68102364),
                           POLY_INIT (51356114, 219329363),
                           POLY_INIT (90116603, 372514754),
                           POLY_INIT (124747292, 522814541),
                           POLY_INIT (63179847, 242223256),
                           POLY_INIT (32793504, 124621591),
                           POLY_INIT (128242569, 474693510),
                           POLY_INIT (93633646, 357999625),
                           POLY_INIT (114897584, 427077540),
                           POLY_INIT (82402647, 275330091),
                           POLY_INIT (45739390, 189765818),
                           POLY_INIT (9011865, 38929205),
                           POLY_INIT (104040611, 445532176),
                           POLY_INIT (67364676, 328455071),
                           POLY_INIT (39016301, 146112270),
                           POLY_INIT (6502538, 27845761),
                           POLY_INIT (53392980, 227209004),
                           POLY_INIT (18799027, 77034659),
                           POLY_INIT (122579354, 531748914),
                           POLY_INIT (92145277, 380388285),
                           POLY_INIT (30766118, 133549928),
                           POLY_INIT (65346497, 250107111),
                           POLY_INIT (95792104, 365885558),
                           POLY_INIT (126206991, 483615737),
                           POLY_INIT (80235217, 284256340),
                           POLY_INIT (116925750, 434959323),
                           POLY_INIT (11048223, 46817098),
                           POLY_INIT (43580152, 198689989),
                           POLY_INIT (93937903, 358995648),
                           POLY_INIT (128519944, 477646159),
                           POLY_INIT (33144609, 123658718),
                           POLY_INIT (63557830, 239303249),
                           POLY_INIT (8674840, 39940604),
                           POLY_INIT (45363711, 192735859),
                           POLY_INIT (82084310, 274351842),
                           POLY_INIT (114617905, 424140141),
                           POLY_INIT (41917546, 136204728),
                           POLY_INIT (5239693, 17025591),
                           POLY_INIT (102712228, 438658726),
                           POLY_INIT (70200387, 322501929),
                           POLY_INIT (123904669, 521860740),
                           POLY_INIT (89312634, 369585419),
                           POLY_INIT (50494803, 220316058),
                           POLY_INIT (20058804, 71064085),
                           POLY_INIT (126359694, 484446512),
                           POLY_INIT (95975273, 368937663),
                           POLY_INIT (65587008, 249243182),
                           POLY_INIT (30976167, 130465185),
                           POLY_INIT (43329145, 199538188),
                           POLY_INIT (10832286, 49884547),
                           POLY_INIT (116783543, 434077970),
                           POLY_INIT (80057936, 281156253),
                           POLY_INIT (7267339, 26973768),
                           POLY_INIT (39750636, 143003079),
                           POLY_INIT (68041669, 329294166),
                           POLY_INIT (104748066, 448608985),
                           POLY_INIT (91478780, 379531636),
                           POLY_INIT (121877787, 528657147),
                           POLY_INIT (18023730, 77858410),
                           POLY_INIT (52652757, 230268389),
                           POLY_INIT (29069357, 107895072),
                           POLY_INIT (59506634, 259244719),
                           POLY_INIT (98258915, 341143102),
                           POLY_INIT (132849668, 491320753),
                           POLY_INIT (78032602, 292224540),
                           POLY_INIT (110543165, 410477971),
                           POLY_INIT (13005076, 55691522),
                           POLY_INIT (49684211, 172774029),
                           POLY_INIT (106785960, 454418008),
                           POLY_INIT (74253135, 302532055),
                           POLY_INIT (37598054, 154069318),
                           POLY_INIT (908417, 3371721),
                           POLY_INIT (54547039, 202471780),
                           POLY_INIT (24133048, 84730603),
                           POLY_INIT (119573905, 506089082),
                           POLY_INIT (84992630, 389535221),
                           POLY_INIT (61532236, 267099856),
                           POLY_INIT (26904491, 116786527),
                           POLY_INIT (130692994, 500214222),
                           POLY_INIT (100292709, 348992065),
                           POLY_INIT (112711355, 418331116),
                           POLY_INIT (76003676, 301113955),
                           POLY_INIT (47647093, 181669618),
                           POLY_INIT (15165074, 63542653),
                           POLY_INIT (72094921, 311433640),
                           POLY_INIT (108821294, 462258727),
                           POLY_INIT (2935559, 11218614),
                           POLY_INIT (35431648, 162968889),
                           POLY_INIT (22096446, 93634196),
                           POLY_INIT (56706521, 210314523),
                           POLY_INIT (87160304, 397379978),
                           POLY_INIT (117545495, 514986501),
                           POLY_INIT (116277429, 431580288),
                           POLY_INIT (81669970, 279248655),
                           POLY_INIT (42901371, 193090462),
                           POLY_INIT (12513436, 44025873),
                           POLY_INIT (66289218, 247317436),
                           POLY_INIT (29561253, 127948851),
                           POLY_INIT (127115660, 478606498),
                           POLY_INIT (94621291, 362508077),
                           POLY_INIT (17349680, 79881208),
                           POLY_INIT (54055895, 232749175),
                           POLY_INIT (90727422, 385471718),
                           POLY_INIT (123210777, 535056233),
                           POLY_INIT (68518599, 331760836),
                           POLY_INIT (103148832, 450617163),
                           POLY_INIT (7691529, 33387482),
                           POLY_INIT (38089454, 148957269),
                           POLY_INIT (83835092, 272409456),
                           POLY_INIT (114251571, 421607679),
                           POLY_INIT (10479386, 34051182),
                           POLY_INIT (45058301, 186257377),
                           POLY_INIT (31589923, 121111628),
                           POLY_INIT (64121284, 237346755),
                           POLY_INIT (92461549, 352531282),
                           POLY_INIT (129152522, 471771357),
                           POLY_INIT (52020305, 222766088),
                           POLY_INIT (19508150, 73056135),
                           POLY_INIT (125377439, 528225046),
                           POLY_INIT (88700024, 375490713),
                           POLY_INIT (100989606, 440632116),
                           POLY_INIT (70554945, 324933819),
                           POLY_INIT (40117608, 142128170),
                           POLY_INIT (5524111, 23408549),
                           POLY_INIT (46968951, 179497824),
                           POLY_INIT (16572304, 61829359),
                           POLY_INIT (111964089, 411688062),
                           POLY_INIT (77332574, 294930417),
                           POLY_INIT (131174016, 498486364),
                           POLY_INIT (98689383, 346806227),
                           POLY_INIT (61952334, 260930370),
                           POLY_INIT (25247401, 110158029),
                           POLY_INIT (86658290, 399076376),
                           POLY_INIT (119153429, 517273495),
                           POLY_INIT (21664572, 99769094),
                           POLY_INIT (58391771, 217038985),
                           POLY_INIT (3633669, 13486884),
                           POLY_INIT (34020834, 164647083),
                           POLY_INIT (72854987, 318176314),
                           POLY_INIT (107463212, 468412341),
                           POLY_INIT (14534678, 53947536),
                           POLY_INIT (49129457, 170571551),
                           POLY_INIT (79501272, 286006158),
                           POLY_INIT (109934655, 403800065),
                           POLY_INIT (96532193, 338922412),
                           POLY_INIT (133208326, 489558051),
                           POLY_INIT (27273519, 101235890),
                           POLY_INIT (59786952, 253044541),
                           POLY_INIT (121320595, 508341224),
                           POLY_INIT (84630388, 391196775),
                           POLY_INIT (56355677, 209165558),
                           POLY_INIT (23823546, 90834809),
                           POLY_INIT (36047460, 155716820),
                           POLY_INIT (1467779, 5609307),
                           POLY_INIT (105305514, 460536778),
                           POLY_INIT (74889805, 309239877),
                           POLY_INIT (58138714, 215790144),
                           POLY_INIT (21450685, 96545231),
                           POLY_INIT (119013268, 518489438),
                           POLY_INIT (86478963, 402267857),
                           POLY_INIT (107617965, 467146108),
                           POLY_INIT (73036106, 314937075),
                           POLY_INIT (34259299, 165880418),
                           POLY_INIT (3845764, 16693741),
                           POLY_INIT (76668127, 296170808),
                           POLY_INIT (111260472, 414887607),
                           POLY_INIT (15794961, 60555814),
                           POLY_INIT (46230774, 176265641),
                           POLY_INIT (26010152, 111383044),
                           POLY_INIT (62688719, 264112523),
                           POLY_INIT (99368422, 345548058),
                           POLY_INIT (131879425, 495271573),
                           POLY_INIT (23484475, 89748912),
                           POLY_INIT (55982044, 205844031),
                           POLY_INIT (84314101, 392315566),
                           POLY_INIT (121038866, 511695137),
                           POLY_INIT (75196108, 308138636),
                           POLY_INIT (105580843, 457197827),
                           POLY_INIT (1816834, 6743442),
                           POLY_INIT (36427493, 159088157),
                           POLY_INIT (109094078, 404943560),
                           POLY_INIT (78695257, 289368391),
                           POLY_INIT (48266096, 169461206),
                           POLY_INIT (13636759, 50617945),
                           POLY_INIT (60660297, 254170612),
                           POLY_INIT (28177838, 104582779),
                           POLY_INIT (134038919, 488465130),
                           POLY_INIT (97331808, 335608165),
                           POLY_INIT (123064472, 534199712),
                           POLY_INIT (90554239, 382379567),
                           POLY_INIT (53808982, 233573054),
                           POLY_INIT (17129649, 82940209),
                           POLY_INIT (38334063, 148085404),
                           POLY_INIT (7897480, 30277907),
                           POLY_INIT (103297441, 451456386),
                           POLY_INIT (68705862, 334837261),
                           POLY_INIT (11742237, 44874456),
                           POLY_INIT (42157050, 196157783),
                           POLY_INIT (80999379, 278367686),
                           POLY_INIT (115579956, 428480073),
                           POLY_INIT (95294186, 363339236),
                           POLY_INIT (127827213, 481658475),
                           POLY_INIT (30330148, 127085306),
                           POLY_INIT (67019459, 244232565),
                           POLY_INIT (88385785, 374536784),
                           POLY_INIT (125093662, 525296095),
                           POLY_INIT (19167031, 74042702),
                           POLY_INIT (51648720, 225728193),
                           POLY_INIT (5871118, 22437228),
                           POLY_INIT (40499689, 139183843),
                           POLY_INIT (70863296, 325937778),
                           POLY_INIT (101262887, 443609597),
                           POLY_INIT (44192892, 187268392),
                           POLY_INIT (9583515, 37021351),
                           POLY_INIT (113413042, 420629046),
                           POLY_INIT (83027029, 269472185),
                           POLY_INIT (129985163, 472766996),
                           POLY_INIT (93259116, 355484059),
                           POLY_INIT (64992581, 236383498),
                           POLY_INIT (32496290, 118191749)
                        };

static const poly_t
                poly72[256]
                        /* poly72[i] = i(x) * x^72 MOD P */
                        = {POLY_INIT (0, 0),
                           POLY_INIT (-1961202135, 335293334),
                           POLY_INIT (468213049, 344628781),
                           POLY_INIT (-1863175408, 125220283),
                           POLY_INIT (973880089, 443020634),
                           POLY_INIT (-1323936464, 161210060),
                           POLY_INIT (568616480, 250440567),
                           POLY_INIT (-1426192375, 487669985),
                           POLY_INIT (2042951513, 129835956),
                           POLY_INIT (-220292752, 339806242),
                           POLY_INIT (1647094368, 322420121),
                           POLY_INIT (-382286775, 13342223),
                           POLY_INIT (1137232960, 500881134),
                           POLY_INIT (-925719959, 237436280),
                           POLY_INIT (1478529401, 156256451),
                           POLY_INIT (-751108272, 447505237),
                           POLY_INIT (-209064270, 259671912),
                           POLY_INIT (2022932635, 478571774),
                           POLY_INIT (-396250229, 468769093),
                           POLY_INIT (1669062050, 135066323),
                           POLY_INIT (-913975893, 354267698),
                           POLY_INIT (1117764482, 115448228),
                           POLY_INIT (-764573550, 26684447),
                           POLY_INIT (1501030075, 309004169),
                           POLY_INIT (-1974658581, 147285212),
                           POLY_INIT (22509506, 456871754),
                           POLY_INIT (-1851439918, 474872561),
                           POLY_INIT (448735995, 263311719),
                           POLY_INIT (-1337908494, 312512902),
                           POLY_INIT (995839195, 22854160),
                           POLY_INIT (-1414955061, 103419819),
                           POLY_INIT (548606434, 366355517),
                           POLY_INIT (-418128540, 519343825),
                           POLY_INIT (1812575053, 218670407),
                           POLY_INIT (-50626467, 176034044),
                           POLY_INIT (2011295348, 427965290),
                           POLY_INIT (-585111939, 76774283),
                           POLY_INIT (1443203156, 393170973),
                           POLY_INIT (-956843196, 270132646),
                           POLY_INIT (1307432301, 65392176),
                           POLY_INIT (-1630066115, 424258917),
                           POLY_INIT (365773844, 179668723),
                           POLY_INIT (-2059438332, 230896456),
                           POLY_INIT (237312301, 507451614),
                           POLY_INIT (-1529147100, 53368895),
                           POLY_INIT (801210125, 282227625),
                           POLY_INIT (-1087157219, 396674578),
                           POLY_INIT (875110964, 72936836),
                           POLY_INIT (345650134, 294570425),
                           POLY_INIT (-1618977281, 41083439),
                           POLY_INIT (259157743, 84170644),
                           POLY_INIT (-2073558842, 385383426),
                           POLY_INIT (781584591, 199834851),
                           POLY_INIT (-1517525274, 404035445),
                           POLY_INIT (897471990, 526623438),
                           POLY_INIT (-1100727329, 211781976),
                           POLY_INIT (1834927247, 372501005),
                           POLY_INIT (-431707482, 97501595),
                           POLY_INIT (1991678390, 45708320),
                           POLY_INIT (-38996065, 289759158),
                           POLY_INIT (1465057174, 206839639),
                           POLY_INIT (-599223873, 531117249),
                           POLY_INIT (1287299759, 417235322),
                           POLY_INIT (-945763194, 186821356),
                           POLY_INIT (-1006945373, 245093539),
                           POLY_INIT (1222727050, 493354805),
                           POLY_INIT (-669817190, 437340814),
                           POLY_INIT (1393100979, 166551832),
                           POLY_INIT (-101252934, 352068089),
                           POLY_INIT (1928162963, 117443183),
                           POLY_INIT (-501260925, 7782356),
                           POLY_INIT (1761948586, 327849026),
                           POLY_INIT (-1170223878, 153548567),
                           POLY_INIT (824550099, 450550913),
                           POLY_INIT (-1579707965, 497838394),
                           POLY_INIT (718143466, 240140972),
                           POLY_INIT (-2144077853, 323027533),
                           POLY_INIT (187275722, 12397019),
                           POLY_INIT (-1680102694, 130784352),
                           POLY_INIT (281134323, 339195894),
                           POLY_INIT (812832017, 31560651),
                           POLY_INIT (-1150764232, 303937629),
                           POLY_INIT (731547688, 359337446),
                           POLY_INIT (-1602165247, 110569072),
                           POLY_INIT (176090632, 461792913),
                           POLY_INIT (-2124120031, 142232839),
                           POLY_INIT (295088945, 252508348),
                           POLY_INIT (-1702044392, 485544746),
                           POLY_INIT (1236673096, 106737791),
                           POLY_INIT (-1028895647, 362847209),
                           POLY_INIT (1381924721, 316026450),
                           POLY_INIT (-649850536, 19531204),
                           POLY_INIT (1941576017, 473646373),
                           POLY_INIT (-123701384, 264728243),
                           POLY_INIT (1750221928, 145873672),
                           POLY_INIT (-481809855, 458092702),
                           POLY_INIT (619243207, 275733106),
                           POLY_INIT (-1343059730, 59994596),
                           POLY_INIT (1057012734, 82166879),
                           POLY_INIT (-1273309737, 387575753),
                           POLY_INIT (518315486, 168341288),
                           POLY_INIT (-1778469897, 435455166),
                           POLY_INIT (84705511, 511848709),
                           POLY_INIT (-1911099698, 226368147),
                           POLY_INIT (1563169182, 399669702),
                           POLY_INIT (-701071433, 70144592),
                           POLY_INIT (1187269799, 56158187),
                           POLY_INIT (-841080178, 279235709),
                           POLY_INIT (1730161287, 230001820),
                           POLY_INIT (-331725650, 508143370),
                           POLY_INIT (2093512638, 423563953),
                           POLY_INIT (-137225833, 180566311),
                           POLY_INIT (-681506699, 521477402),
                           POLY_INIT (1551591004, 216610444),
                           POLY_INIT (-863414964, 195003191),
                           POLY_INIT (1200831333, 409184417),
                           POLY_INIT (-311610516, 91416640),
                           POLY_INIT (1719098693, 378454998),
                           POLY_INIT (-159027627, 301495917),
                           POLY_INIT (2107572348, 33840635),
                           POLY_INIT (-1364852948, 413679278),
                           POLY_INIT (633311493, 190059832),
                           POLY_INIT (-1253203435, 203595907),
                           POLY_INIT (1045941308, 534678293),
                           POLY_INIT (-1800813515, 47172596),
                           POLY_INIT (531868188, 288612450),
                           POLY_INIT (-1891526388, 373642713),
                           POLY_INIT (73135909, 96042575),
                           POLY_INIT (-2013890746, 490187079),
                           POLY_INIT (216799599, 248218321),
                           POLY_INIT (-1675744641, 163395434),
                           POLY_INIT (386155606, 440474876),
                           POLY_INIT (-1107669921, 122706973),
                           POLY_INIT (920658550, 346847115),
                           POLY_INIT (-1508765338, 333103664),
                           POLY_INIT (755531599, 2550182),
                           POLY_INIT (-30244833, 445287155),
                           POLY_INIT (1965616694, 158769509),
                           POLY_INIT (-438641370, 234886366),
                           POLY_INIT (1858122511, 503070536),
                           POLY_INIT (-1002521850, 15564713),
                           POLY_INIT (1327813935, 319902783),
                           POLY_INIT (-539564481, 342352260),
                           POLY_INIT (1422690326, 127650322),
                           POLY_INIT (1954519540, 307097135),
                           POLY_INIT (-10094627, 28427705),
                           POLY_INIT (1872217293, 113733634),
                           POLY_INIT (-460477724, 356211604),
                           POLY_INIT (1316201197, 136977269),
                           POLY_INIT (-982922044, 467022051),
                           POLY_INIT (1436286932, 480281944),
                           POLY_INIT (-561933827, 257732302),
                           POLY_INIT (230387373, 368102811),
                           POLY_INIT (-2036268924, 101508621),
                           POLY_INIT (374551444, 24794038),
                           POLY_INIT (-1656136259, 310802464),
                           POLY_INIT (934761908, 261568705),
                           POLY_INIT (-1129497699, 476779351),
                           POLY_INIT (744425613, 454928108),
                           POLY_INIT (-1488623964, 148999546),
                           POLY_INIT (1625664034, 63121302),
                           POLY_INIT (-335559669, 272567296),
                           POLY_INIT (2064512795, 390707643),
                           POLY_INIT (-266888910, 79008301),
                           POLY_INIT (1525256507, 430232268),
                           POLY_INIT (-772538606, 173603162),
                           POLY_INIT (1090636802, 221138145),
                           POLY_INIT (-904158677, 517105527),
                           POLY_INIT (421617019, 75367458),
                           POLY_INIT (-1841613998, 394407860),
                           POLY_INIT (46727234, 284465679),
                           POLY_INIT (-1982632341, 50901401),
                           POLY_INIT (590177890, 505016696),
                           POLY_INIT (-1472788405, 233167598),
                           POLY_INIT (952449883, 177434453),
                           POLY_INIT (-1277209230, 426722499),
                           POLY_INIT (-1821621104, 213475582),
                           POLY_INIT (410397369, 524634984),
                           POLY_INIT (-2004608599, 406060755),
                           POLY_INIT (60716928, 198169925),
                           POLY_INIT (-1453293687, 383686052),
                           POLY_INIT (578425248, 86162994),
                           POLY_INIT (-1299701072, 39062409),
                           POLY_INIT (965889177, 296230943),
                           POLY_INIT (-358042679, 184828746),
                           POLY_INIT (1639112160, 418932956),
                           POLY_INIT (-247402768, 529456487),
                           POLY_INIT (2052751577, 208860913),
                           POLY_INIT (-794523440, 291747344),
                           POLY_INIT (1539237625, 44014982),
                           POLY_INIT (-884156951, 99166269),
                           POLY_INIT (1079425984, 370475947),
                           POLY_INIT (1141722341, 330071524),
                           POLY_INIT (-820567348, 5265010),
                           POLY_INIT (1608847836, 119989193),
                           POLY_INIT (-721453067, 349882463),
                           POLY_INIT (2114025468, 164333758),
                           POLY_INIT (-182773291, 439853864),
                           POLY_INIT (1709779653, 490804883),
                           POLY_INIT (-286046996, 247282949),
                           POLY_INIT (1036630972, 336682576),
                           POLY_INIT (-1227631211, 133002694),
                           POLY_INIT (639755909, 10207357),
                           POLY_INIT (-1388607316, 325577707),
                           POLY_INIT (130384037, 242658058),
                           POLY_INIT (-1931481460, 495616156),
                           POLY_INIT (472767900, 452736295),
                           POLY_INIT (-1757957195, 151002801),
                           POLY_INIT (-1216044457, 483801740),
                           POLY_INIT (1017039998, 254415130),
                           POLY_INIT (-1402142866, 140289185),
                           POLY_INIT (662081863, 463507255),
                           POLY_INIT (-1920427698, 112316374),
                           POLY_INIT (110294887, 357426240),
                           POLY_INIT (-1772043145, 305877499),
                           POLY_INIT (494578270, 29850221),
                           POLY_INIT (-834644722, 460003640),
                           POLY_INIT (1163541287, 144126638),
                           POLY_INIT (-710408137, 266438421),
                           POLY_INIT (1588749854, 471706755),
                           POLY_INIT (-196317673, 17624162),
                           POLY_INIT (2136342590, 317769716),
                           POLY_INIT (-274451666, 361132623),
                           POLY_INIT (1690197255, 108681689),
                           POLY_INIT (-1558277759, 223933237),
                           POLY_INIT (671416232, 514119843),
                           POLY_INIT (-1191785288, 433220888),
                           POLY_INIT (871146129, 170804878),
                           POLY_INIT (-1726829928, 390006383),
                           POLY_INIT (302564529, 79900153),
                           POLY_INIT (-2097481823, 62232642),
                           POLY_INIT (165714312, 273265620),
                           POLY_INIT (-623221032, 182833281),
                           POLY_INIT (1371539697, 421133079),
                           POLY_INIT (-1053672479, 510611116),
                           POLY_INIT (1244157384, 227763514),
                           POLY_INIT (-522822207, 276964827),
                           POLY_INIT (1808544744, 58592845),
                           POLY_INIT (-79822600, 67681270),
                           POLY_INIT (1881435857, 401903712),
                           POLY_INIT (1352105779, 35828829),
                           POLY_INIT (-611512038, 299802571),
                           POLY_INIT (1266622986, 380119664),
                           POLY_INIT (-1067103197, 89391590),
                           POLY_INIT (1788560426, 407191815),
                           POLY_INIT (-511628797, 196700817),
                           POLY_INIT (1903368467, 214949674),
                           POLY_INIT (-93751494, 523498684),
                           POLY_INIT (693340266, 94345193),
                           POLY_INIT (-1572215229, 375635071),
                           POLY_INIT (851170643, 286591428),
                           POLY_INIT (-1180583046, 48833106),
                           POLY_INIT (325038963, 536371891),
                           POLY_INIT (-1740251814, 201607461),
                           POLY_INIT (146271818, 192085150),
                           POLY_INIT (-2085781405, 412014344)
                        };

static const poly_t
                poly80[256]
                        /* poly80[i] = i(x) * X^80 MOD P */
                        = {POLY_INIT (0, 0),
                           POLY_INIT (-1753253426, 125726524),
                           POLY_INIT (788460444, 251453049),
                           POLY_INIT (-1182692782, 159560005),
                           POLY_INIT (1576920888, 502906098),
                           POLY_INIT (-897409290, 445109198),
                           POLY_INIT (1929581732, 319120011),
                           POLY_INIT (-461607574, 343608759),
                           POLY_INIT (-1238937829, 142717156),
                           POLY_INIT (559429333, 268427224),
                           POLY_INIT (-1730560889, 108883613),
                           POLY_INIT (262587721, 16974241),
                           POLY_INIT (-337991645, 360188950),
                           POLY_INIT (2091241965, 302408490),
                           POLY_INIT (-987472961, 461689455),
                           POLY_INIT (1381704305, 486194515),
                           POLY_INIT (1817091638, 285434313),
                           POLY_INIT (-80615432, 377294581),
                           POLY_INIT (1118858666, 536854448),
                           POLY_INIT (-707849116, 411160716),
                           POLY_INIT (833845518, 217767227),
                           POLY_INIT (-1496579904, 193245703),
                           POLY_INIT (525175442, 33948482),
                           POLY_INIT (-2009926820, 91778174),
                           POLY_INIT (-630599379, 428003629),
                           POLY_INIT (1293330659, 519880209),
                           POLY_INIT (-191421775, 394137428),
                           POLY_INIT (1676172159, 268460136),
                           POLY_INIT (-2020329963, 75197919),
                           POLY_INIT (283856859, 50660067),
                           POLY_INIT (-1452620407, 176665510),
                           POLY_INIT (1041611847, 234478746),
                           POLY_INIT (-717112057, 292817554),
                           POLY_INIT (1111409865, 369780142),
                           POLY_INIT (-71348581, 529061099),
                           POLY_INIT (1824536405, 418823127),
                           POLY_INIT (-2000922049, 210613856),
                           POLY_INIT (532882417, 200530268),
                           POLY_INIT (-1505580637, 40986649),
                           POLY_INIT (826134637, 84870949),
                           POLY_INIT (1667691036, 435534454),
                           POLY_INIT (-199652398, 512480586),
                           POLY_INIT (1301807488, 386491407),
                           POLY_INIT (-622364594, 276237107),
                           POLY_INIT (1050350884, 67896964),
                           POLY_INIT (-1444647702, 57829816),
                           POLY_INIT (275113656, 183556349),
                           POLY_INIT (-2028298378, 227456961),
                           POLY_INIT (-1190190287, 7826267),
                           POLY_INIT (779115263, 118031463),
                           POLY_INIT (-1745760083, 244036898),
                           POLY_INIT (9349475, 167107102),
                           POLY_INIT (-453852151, 495900585),
                           POLY_INIT (1938668999, 451983509),
                           POLY_INIT (-905169003, 326240720),
                           POLY_INIT (1567837787, 336357100),
                           POLY_INIT (254307370, 150395839),
                           POLY_INIT (-1739123228, 260617347),
                           POLY_INIT (567713718, 101320134),
                           POLY_INIT (-1230379400, 24406778),
                           POLY_INIT (1389726482, 353331021),
                           POLY_INIT (-978652452, 309397617),
                           POLY_INIT (2083223694, 468957492),
                           POLY_INIT (-346816192, 479057416),
                           POLY_INIT (-1486881947, 295361573),
                           POLY_INIT (807436971, 384086809),
                           POLY_INIT (-1985935111, 526778972),
                           POLY_INIT (517894455, 404254048),
                           POLY_INIT (-90039203, 207839447),
                           POLY_INIT (1843226003, 186453995),
                           POLY_INIT (-732106815, 44023470),
                           POLY_INIT (1126405647, 98685330),
                           POLY_INIT (293123198, 421227713),
                           POLY_INIT (-2046308944, 509936637),
                           POLY_INIT (1065764834, 401060536),
                           POLY_INIT (-1460060628, 278519172),
                           POLY_INIT (1283806022, 81973299),
                           POLY_INIT (-604362104, 60604175),
                           POLY_INIT (1652269274, 169741898),
                           POLY_INIT (-184231660, 224420214),
                           POLY_INIT (-888052397, 10075628),
                           POLY_INIT (1550720157, 132633296),
                           POLY_INIT (-437283121, 241525653),
                           POLY_INIT (1922100993, 152767657),
                           POLY_INIT (-1762885013, 492831006),
                           POLY_INIT (26475429, 438201890),
                           POLY_INIT (-1206750729, 329047911),
                           POLY_INIT (795674681, 350400603),
                           POLY_INIT (2100701768, 135793928),
                           POLY_INIT (-364293242, 258368052),
                           POLY_INIT (1405671892, 115659633),
                           POLY_INIT (-994598886, 26917965),
                           POLY_INIT (550227312, 367112698),
                           POLY_INIT (-1212894018, 312467142),
                           POLY_INIT (238370540, 454913923),
                           POLY_INIT (-1723185374, 476250303),
                           POLY_INIT (1914586722, 15652535),
                           POLY_INIT (-446611540, 126925195),
                           POLY_INIT (1558230526, 236062926),
                           POLY_INIT (-878719952, 158099442),
                           POLY_INIT (803447130, 488073797),
                           POLY_INIT (-1197680492, 443090297),
                           POLY_INIT (18698950, 334214204),
                           POLY_INIT (-1771951352, 345365248),
                           POLY_INIT (-1002894983, 141518419),
                           POLY_INIT (1397125303, 252774767),
                           POLY_INIT (-355992859, 110344234),
                           POLY_INIT (2109244203, 32364310),
                           POLY_INIT (-1715147199, 362207905),
                           POLY_INIT (247175055, 317240733),
                           POLY_INIT (-1220928035, 459932888),
                           POLY_INIT (541418515, 471100388),
                           POLY_INIT (508614740, 300791678),
                           POLY_INIT (-1993367142, 378787906),
                           POLY_INIT (816720840, 521234695),
                           POLY_INIT (-1479454202, 409929275),
                           POLY_INIT (1135427436, 202640268),
                           POLY_INIT (-724416862, 191521968),
                           POLY_INIT (1834208496, 48813557),
                           POLY_INIT (-97733314, 93764297),
                           POLY_INIT (-1468557489, 426510234),
                           POLY_INIT (1057549953, 504522918),
                           POLY_INIT (-2037816109, 395368931),
                           POLY_INIT (301341981, 284079839),
                           POLY_INIT (-175476617, 76921704),
                           POLY_INIT (1660225977, 65786964),
                           POLY_INIT (-613121045, 174679313),
                           POLY_INIT (1275853349, 219613741),
                           POLY_INIT (1130592161, 273117515),
                           POLY_INIT (-736358801, 389619319),
                           POLY_INIT (1839039549, 515616562),
                           POLY_INIT (-85787149, 432390158),
                           POLY_INIT (513708185, 230608313),
                           POLY_INIT (-1981683369, 180396677),
                           POLY_INIT (811623173, 54662080),
                           POLY_INIT (-1491133749, 71073020),
                           POLY_INIT (-180078406, 415678895),
                           POLY_INIT (1648050548, 532197011),
                           POLY_INIT (-608515290, 372907990),
                           POLY_INIT (1288024808, 289698026),
                           POLY_INIT (-1464213630, 88046941),
                           POLY_INIT (1069983308, 37818977),
                           POLY_INIT (-2042156002, 197370660),
                           POLY_INIT (288904656, 213765144),
                           POLY_INIT (791538071, 21237890),
                           POLY_INIT (-1202548647, 104497086),
                           POLY_INIT (30611979, 263769851),
                           POLY_INIT (-1767087163, 147235271),
                           POLY_INIT (1926237871, 482192496),
                           POLY_INIT (-441485471, 465814348),
                           POLY_INIT (1546583347, 306278921),
                           POLY_INIT (-883849987, 356457781),
                           POLY_INIT (-1727355252, 163946598),
                           POLY_INIT (242605890, 247189338),
                           POLY_INIT (-1208724208, 121208351),
                           POLY_INIT (545991902, 4657443),
                           POLY_INIT (-990428748, 339483796),
                           POLY_INIT (1401436282, 323122088),
                           POLY_INIT (-368463320, 448840429),
                           POLY_INIT (2104937446, 499035601),
                           POLY_INIT (-1776104794, 20151257),
                           POLY_INIT (22917992, 105714917),
                           POLY_INIT (-1193526982, 265266592),
                           POLY_INIT (799228148, 145869468),
                           POLY_INIT (-874566242, 483051307),
                           POLY_INIT (1554011216, 464824343),
                           POLY_INIT (-450765310, 305535314),
                           POLY_INIT (1918805964, 357070446),
                           POLY_INIT (537232829, 162712381),
                           POLY_INIT (-1216676749, 248292353),
                           POLY_INIT (251360801, 122557764),
                           POLY_INIT (-1719398417, 3177080),
                           POLY_INIT (2113430149, 340490191),
                           POLY_INIT (-360244405, 322246899),
                           POLY_INIT (1392939289, 448244150),
                           POLY_INIT (-998643497, 499762826),
                           POLY_INIT (-93563760, 271587856),
                           POLY_INIT (1829973342, 391017772),
                           POLY_INIT (-728586484, 516736105),
                           POLY_INIT (1139662530, 431139669),
                           POLY_INIT (-1483623512, 231319266),
                           POLY_INIT (820955750, 179816926),
                           POLY_INIT (-1989197772, 53835931),
                           POLY_INIT (504379898, 72030119),
                           POLY_INIT (1279990667, 414296820),
                           POLY_INIT (-617323963, 533710280),
                           POLY_INIT (1656088599, 374174861),
                           POLY_INIT (-171273767, 288562097),
                           POLY_INIT (297204915, 88610310),
                           POLY_INIT (-2033613443, 37124410),
                           POLY_INIT (1061687087, 196397183),
                           POLY_INIT (-1472760095, 214607683),
                           POLY_INIT (-465793852, 31305070),
                           POLY_INIT (1933833482, 111395410),
                           POLY_INIT (-893223080, 253850391),
                           POLY_INIT (1572669078, 140450859),
                           POLY_INIT (-1178506244, 472125852),
                           POLY_INIT (784208434, 458915488),
                           POLY_INIT (-1757439904, 316198885),
                           POLY_INIT (4252078, 363241689),
                           POLY_INIT (1377551327, 157031818),
                           POLY_INIT (-983254511, 237138614),
                           POLY_INIT (2095394883, 127976435),
                           POLY_INIT (-342210163, 14593231),
                           POLY_INIT (266740967, 346399096),
                           POLY_INIT (-1734779607, 333172292),
                           POLY_INIT (555276155, 442072833),
                           POLY_INIT (-1234719051, 489099325),
                           POLY_INIT (-2005789966, 283036839),
                           POLY_INIT (520973116, 396403611),
                           POLY_INIT (-1500716690, 505549534),
                           POLY_INIT (838047904, 425491938),
                           POLY_INIT (-711985718, 220688469),
                           POLY_INIT (1123060740, 173612905),
                           POLY_INIT (-76478890, 64728620),
                           POLY_INIT (1812889496, 77971728),
                           POLY_INIT (1045781993, 408910915),
                           POLY_INIT (-1456856025, 522261375),
                           POLY_INIT (279686773, 379822650),
                           POLY_INIT (-2016094277, 299748614),
                           POLY_INIT (1672002257, 94814385),
                           POLY_INIT (-187186401, 47755149),
                           POLY_INIT (1297500493, 190455496),
                           POLY_INIT (-634834813, 203715060),
                           POLY_INIT (830288323, 279554044),
                           POLY_INIT (-1509799923, 400017600),
                           POLY_INIT (528728671, 508918149),
                           POLY_INIT (-1996702831, 422254265),
                           POLY_INIT (1820382971, 223353614),
                           POLY_INIT (-67129547, 170816562),
                           POLY_INIT (1115563367, 61654391),
                           POLY_INIT (-721331031, 80915019),
                           POLY_INIT (-2024112424, 405280536),
                           POLY_INIT (270862102, 525760548),
                           POLY_INIT (-1448833724, 383043937),
                           POLY_INIT (1054602378, 296396381),
                           POLY_INIT (-626550304, 97627114),
                           POLY_INIT (1306058798, 45073622),
                           POLY_INIT (-195466628, 187528595),
                           POLY_INIT (1663439794, 206772911),
                           POLY_INIT (1563668469, 27969077),
                           POLY_INIT (-900934085, 114600201),
                           POLY_INIT (1942838377, 257300556),
                           POLY_INIT (-458087001, 136869744),
                           POLY_INIT (13519053, 475232967),
                           POLY_INIT (-1749995261, 455939579),
                           POLY_INIT (774945617, 313500862),
                           POLY_INIT (-1185955169, 366070658),
                           POLY_INIT (-350953234, 153843409),
                           POLY_INIT (2087426336, 240458221),
                           POLY_INIT (-974515342, 131573928),
                           POLY_INIT (1385523900, 11126676),
                           POLY_INIT (-1226242090, 349358627),
                           POLY_INIT (563510808, 330081567),
                           POLY_INIT (-1743260598, 439227482),
                           POLY_INIT (258510212, 491813734)
};

static const poly_t
                poly88[256]
                        /* poly80[i] = i(x) * X^80 MOD P */
                        = {POLY_INIT (0, 0),
                           POLY_INIT (964379295, 346020725),
                           POLY_INIT (2133460053, 441286634),
                           POLY_INIT (1179731658, 248685727),
                           POLY_INIT (-209155647, 132658900),
                           POLY_INIT (-889992354, 326626721),
                           POLY_INIT (-1935503980, 497371454),
                           POLY_INIT (-1244016885, 154833483),
                           POLY_INIT (-418311294, 265317801),
                           POLY_INIT (-563457763, 458208988),
                           POLY_INIT (-1740957737, 362615363),
                           POLY_INIT (-1589619384, 16959798),
                           POLY_INIT (345610819, 137911165),
                           POLY_INIT (769841372, 480739336),
                           POLY_INIT (1806933526, 309666967),
                           POLY_INIT (1388895369, 116064226),
                           POLY_INIT (-836622588, 530635603),
                           POLY_INIT (-145136229, 188678182),
                           POLY_INIT (-1324866735, 99460281),
                           POLY_INIT (-2005704242, 292716492),
                           POLY_INIT (1034596037, 407432583),
                           POLY_INIT (80866394, 215430898),
                           POLY_INIT (1115728528, 33919597),
                           POLY_INIT (2080106511, 379210008),
                           POLY_INIT (691221638, 275822330),
                           POLY_INIT (273184281, 82800015),
                           POLY_INIT (1444491475, 171821328),
                           POLY_INIT (1868722764, 513938021),
                           POLY_INIT (-625230521, 395870254),
                           POLY_INIT (-473890856, 50813787),
                           POLY_INIT (-1517176558, 232128452),
                           POLY_INIT (-1662321779, 424289457),
                           POLY_INIT (-1852256413, 204701607),
                           POLY_INIT (-1461481988, 413967570),
                           POLY_INIT (-290272458, 377356365),
                           POLY_INIT (-674657879, 48356152),
                           POLY_INIT (1645233826, 198920563),
                           POLY_INIT (1533740093, 524587526),
                           POLY_INIT (490357495, 295057049),
                           POLY_INIT (608239720, 84536812),
                           POLY_INIT (1988746465, 65212942),
                           POLY_INIT (1341300350, 394054011),
                           POLY_INIT (161732788, 430861796),
                           POLY_INIT (819501611, 221361809),
                           POLY_INIT (-2063510240, 67839194),
                           POLY_INIT (-1132849217, 278200239),
                           POLY_INIT (-97824395, 507927344),
                           POLY_INIT (-1018162198, 182026309),
                           POLY_INIT (1606084711, 328443124),
                           POLY_INIT (1723968248, 118259585),
                           POLY_INIT (546368562, 165600030),
                           POLY_INIT (434876077, 490799211),
                           POLY_INIT (-1405984346, 343642656),
                           POLY_INIT (-1790368967, 14960981),
                           POLY_INIT (-753375757, 238480842),
                           POLY_INIT (-362600596, 447297215),
                           POLY_INIT (-1196688411, 474204509),
                           POLY_INIT (-2117027462, 148640296),
                           POLY_INIT (-947781712, 101627575),
                           POLY_INIT (-17122001, 311520706),
                           POLY_INIT (1260614180, 464256905),
                           POLY_INIT (1918382267, 255075580),
                           POLY_INIT (873035377, 31883363),
                           POLY_INIT (225588462, 360274710),
                           POLY_INIT (590454470, 409403215),
                           POLY_INIT (441113689, 217654330),
                           POLY_INIT (1545312915, 36405413),
                           POLY_INIT (1700845580, 380918736),
                           POLY_INIT (-793176313, 529451419),
                           POLY_INIT (-373140072, 185668334),
                           POLY_INIT (-1349315758, 96712305),
                           POLY_INIT (-1763159603, 291269892),
                           POLY_INIT (-1004499644, 397841126),
                           POLY_INIT (-44314661, 53037459),
                           POLY_INIT (-1156937455, 234614028),
                           POLY_INIT (-2106471538, 425997945),
                           POLY_INIT (933758085, 274637874),
                           POLY_INIT (248727066, 79789895),
                           POLY_INIT (1216479440, 169073624),
                           POLY_INIT (1912160847, 512491693),
                           POLY_INIT (-317474366, 130425884),
                           POLY_INIT (-731317411, 324665193),
                           POLY_INIT (-1841709673, 495672310),
                           POLY_INIT (-1421672696, 152338563),
                           POLY_INIT (513486851, 3019464),
                           POLY_INIT (669020828, 347195837),
                           POLY_INIT (1639003222, 442723618),
                           POLY_INIT (1489663689, 251442775),
                           POLY_INIT (167946816, 135678389),
                           POLY_INIT (863627487, 478778048),
                           POLY_INIT (1965600277, 307967583),
                           POLY_INIT (1280568458, 113569066),
                           POLY_INIT (-108387455, 268336993),
                           POLY_INIT (-1057922786, 459383828),
                           POLY_INIT (-2036324396, 364052619),
                           POLY_INIT (-1076140725, 19717118),
                           POLY_INIT (-1297526363, 341147880),
                           POLY_INIT (-1949166790, 13261725),
                           POLY_INIT (-847030800, 236519170),
                           POLY_INIT (-185067665, 445064311),
                           POLY_INIT (1092737124, 331200060),
                           POLY_INIT (2019203835, 119696713),
                           POLY_INIT (1040964657, 166775254),
                           POLY_INIT (124821166, 493818531),
                           POLY_INIT (1438138919, 461761857),
                           POLY_INIT (1824719032, 253376052),
                           POLY_INIT (714229362, 29921963),
                           POLY_INIT (334038253, 358042078),
                           POLY_INIT (-1506751514, 476961685),
                           POLY_INIT (-1622439559, 150077664),
                           POLY_INIT (-652554317, 102802559),
                           POLY_INIT (-530477780, 314539786),
                           POLY_INIT (2089514657, 200629179),
                           POLY_INIT (1173369918, 527073486),
                           POLY_INIT (60912372, 297280593),
                           POLY_INIT (987377771, 86507300),
                           POLY_INIT (-1895563424, 203255151),
                           POLY_INIT (-1233601025, 411219482),
                           POLY_INIT (-265684171, 374346373),
                           POLY_INIT (-917325398, 47172080),
                           POLY_INIT (-1684380381, 69547538),
                           POLY_INIT (-1562302532, 280685927),
                           POLY_INIT (-458202762, 510151160),
                           POLY_INIT (-573889559, 183997069),
                           POLY_INIT (1746070754, 63766726),
                           POLY_INIT (1365880445, 391306163),
                           POLY_INIT (389605559, 427851564),
                           POLY_INIT (776186408, 220177497),
                           POLY_INIT (1270856935, 62863262),
                           POLY_INIT (1925512824, 388033771),
                           POLY_INIT (882227378, 435308660),
                           POLY_INIT (233765421, 225284865),
                           POLY_INIT (-1204341466, 72810826),
                           POLY_INIT (-2125695047, 281598527),
                           POLY_INIT (-954388109, 505052832),
                           POLY_INIT (-26840084, 176530901),
                           POLY_INIT (-1397837979, 208624183),
                           POLY_INIT (-1781142022, 418414914),
                           POLY_INIT (-746280144, 371336669),
                           POLY_INIT (-352327249, 46005928),
                           POLY_INIT (1596335780, 193424611),
                           POLY_INIT (1717396539, 521713558),
                           POLY_INIT (537666289, 298455817),
                           POLY_INIT (427253870, 89507964),
                           POLY_INIT (-2053268509, 471854285),
                           POLY_INIT (-1125721732, 142620600),
                           POLY_INIT (-88629322, 106074919),
                           POLY_INIT (-1009984215, 315443282),
                           POLY_INIT (1981092386, 469228057),
                           POLY_INIT (1332629693, 258474348),
                           POLY_INIT (155129463, 29009395),
                           POLY_INIT (809784552, 354778758),
                           POLY_INIT (1653377121, 332366180),
                           POLY_INIT (1542966014, 122706449),
                           POLY_INIT (497454132, 159579790),
                           POLY_INIT (618516139, 488449531),
                           POLY_INIT (-1862008416, 338147248),
                           POLY_INIT (-1468054721, 12086469),
                           POLY_INIT (-298973707, 241879130),
                           POLY_INIT (-682277014, 452268847),
                           POLY_INIT (-634948732, 260851769),
                           POLY_INIT (-480497381, 454267724),
                           POLY_INIT (-1525844015, 364984275),
                           POLY_INIT (-1669974706, 22998182),
                           POLY_INIT (699398725, 140803821),
                           POLY_INIT (282376410, 486253976),
                           POLY_INIT (1451621904, 304677127),
                           POLY_INIT (1878965391, 112646770),
                           POLY_INIT (1026973702, 6038928),
                           POLY_INIT (72163993, 348389093),
                           POLY_INIT (1109156947, 437344890),
                           POLY_INIT (2070357708, 244220175),
                           POLY_INIT (-826349113, 129241924),
                           POLY_INIT (-138040488, 321636401),
                           POLY_INIT (-1315639918, 502885550),
                           POLY_INIT (-1997558003, 157726683),
                           POLY_INIT (335893632, 271356778),
                           POLY_INIT (763237919, 78858271),
                           POLY_INIT (1798262997, 174189696),
                           POLY_INIT (1381241418, 519976949),
                           POLY_INIT (-410133183, 398763454),
                           POLY_INIT (-554262562, 56327883),
                           POLY_INIT (-1733830380, 227138132),
                           POLY_INIT (-1579377781, 420872481),
                           POLY_INIT (-216774910, 536673987),
                           POLY_INIT (-898693731, 191047094),
                           POLY_INIT (-1942076585, 95519017),
                           POLY_INIT (-1253768760, 288250460),
                           POLY_INIT (10276547, 404015127),
                           POLY_INIT (971476060, 210441058),
                           POLY_INIT (2142685846, 39434237),
                           POLY_INIT (1187874825, 382102664),
                           POLY_INIT (1754145313, 467257553),
                           POLY_INIT (1375035582, 256250788),
                           POLY_INIT (396633716, 26523451),
                           POLY_INIT (786392299, 353070158),
                           POLY_INIT (-1694061600, 473038341),
                           POLY_INIT (-1568806529, 145630576),
                           POLY_INIT (-466833483, 108823023),
                           POLY_INIT (-581440214, 316889754),
                           POLY_INIT (-1885392477, 336176504),
                           POLY_INIT (-1226542276, 9862669),
                           POLY_INIT (-256559626, 239393426),
                           POLY_INIT (-909215895, 450560487),
                           POLY_INIT (2081929314, 333550508),
                           POLY_INIT (1164770045, 125716697),
                           POLY_INIT (54377527, 162327622),
                           POLY_INIT (977731240, 489895731),
                           POLY_INIT (-1498675931, 75043714),
                           POLY_INIT (-1613281350, 283560183),
                           POLY_INIT (-645529232, 506752104),
                           POLY_INIT (-520272913, 179025693),
                           POLY_INIT (1428458724, 59843926),
                           POLY_INIT (1818218107, 386858531),
                           POLY_INIT (705595569, 433871548),
                           POLY_INIT (326486574, 222527945),
                           POLY_INIT (1102911143, 195657259),
                           POLY_INIT (2026263608, 523674974),
                           POLY_INIT (1050088178, 300155329),
                           POLY_INIT (132927597, 92002996),
                           POLY_INIT (-1305108634, 205605119),
                           POLY_INIT (-1957765639, 417239946),
                           POLY_INIT (-853566669, 369899285),
                           POLY_INIT (-194717268, 43248736),
                           POLY_INIT (-115937982, 401258358),
                           POLY_INIT (-1066553379, 58027011),
                           POLY_INIT (-2042828521, 229099676),
                           POLY_INIT (-1085822072, 423105513),
                           POLY_INIT (178152579, 268599714),
                           POLY_INIT (870655516, 77421271),
                           POLY_INIT (1974755542, 173014600),
                           POLY_INIT (1288643145, 516957501),
                           POLY_INIT (503840448, 406510303),
                           POLY_INIT (662486111, 212140458),
                           POLY_INIT (1630403221, 41395509),
                           POLY_INIT (1482078218, 384335424),
                           POLY_INIT (-309364991, 533916683),
                           POLY_INIT (-722192994, 189609854),
                           POLY_INIT (-1834650796, 94344161),
                           POLY_INIT (-1411501621, 285231252),
                           POLY_INIT (926206534, 139095077),
                           POLY_INIT (240093401, 483768144),
                           POLY_INIT (1209978387, 302453711),
                           POLY_INIT (1902480524, 110676154),
                           POLY_INIT (-994294905, 262298353),
                           POLY_INIT (-37289704, 457015684),
                           POLY_INIT (-1147779118, 367994139),
                           POLY_INIT (-2098395827, 24182382),
                           POLY_INIT (-802825788, 127533452),
                           POLY_INIT (-379675813, 319150841),
                           POLY_INIT (-1357914735, 500661862),
                           POLY_INIT (-1770742002, 155755795),
                           POLY_INIT (598560773, 7485272),
                           POLY_INIT (450237082, 351136813),
                           POLY_INIT (1552372816, 440354994),
                           POLY_INIT (1711019727, 245404615)
                        };

#ifndef FINGERPRINT_LITTLE_ENDIAN
static void poly_find_byte_order (void)
{
  int_32_t    i = 0x12345678;
  int_bytes_t x = { i };
  word_t      a0 = word_extract (i, 0, 8);
  word_t      a1 = word_extract (i, 8, 8);
  word_t      a2 = word_extract (i, 16, 8);
  word_t      a3 = word_extract (i, 24, 8);

  if (a0 == x.b[0] && a1 == x.b[1] && a2 == x.b[2] && a3 == x.b[3]) {
    poly_little_endian = 1;
  } else if (a0 == x.b[3] && a1 == x.b[2] && a2 == x.b[1] && a3 == x.b[0]) {
    poly_big_endian = 1;
  } else {
    /* Unsupported byte ordering ...  */
    assert (0);
  }

  poly_init_done = 1;
}
#endif /* FINGERPRINT_LITTLE_ENDIAN */

/* Return the sign-extended bottom 32 bits of X.  */

static int_32_t poly_fix_32 (word_t x)
{
  if (sizeof (word_t) == sizeof (int_32_t))
    /* Sign-extension is trivial; the types are the same size.
       Hopefully, this branch will reduce the function to the
       identity, and compilers will be clever enough to inline it.  */
    return x;
  else
    {
      const integer_t sign = 0x80000000;
      const integer_t sign_extend = word_left_shift (word_not (0), 31);

      if (word_and (x, sign) == 0)
        return word_and (x, POLY_SIG_BITS);
      else
        return word_or (sign_extend, word_and (x, POLY_SIG_BITS));
    }
}

#if MAY_BE_LITTLE_ENDIAN
static poly_t poly_extend_words_le (const poly_t  p,
                                    const byte_t* source,
                                    int           len)
{
  int_ptr_t   ip = (int_ptr_t) source;
  int_bytes_t tmp;
  /* Curiously, the Modula-3 sources use INTEGER for the type of the
     next two variables, rather than int_32_t.  */
  integer_t   p0 = POLY_HALF (p, 0);
  integer_t   p1 = POLY_HALF (p, 1);
  poly_t      result;

  while (len > 0) {
    /* Split the low-order bytes.  */
    tmp.w = p0;

    /* Compute the new result.  */
    p0 = word_xor (p1,
                   word_xor (word_xor (POLY_HALF (poly88[tmp.b[0]], 0),
                                       POLY_HALF (poly80[tmp.b[1]], 0)),
                             word_xor (POLY_HALF (poly72[tmp.b[2]], 0),
                                       POLY_HALF (poly64[tmp.b[3]], 0))));
    p1 = word_xor (*ip,
                   word_xor (word_xor (POLY_HALF (poly88[tmp.b[0]], 1),
                                       POLY_HALF (poly80[tmp.b[1]], 1)),
                             word_xor (POLY_HALF (poly72[tmp.b[2]], 1),
                                       POLY_HALF (poly64[tmp.b[3]], 1))));
    len -= sizeof (int_32_t);
    ++ip; /* The Modula-3 source is INC (ip, ADRSIZE (ip^)), but C's
             pointer arithmetic behaves differently.  */
  }
  POLY_FORM (result, p0, p1);
  return result;
}
#endif /* MAY_BE_LITTLE_ENDIAN */

#if MAY_BE_BIG_ENDIAN
static poly_t poly_extend_words_be (const poly_t  p,
                                    const byte_t* source,
                                    int           len)
{
  int_ptr_t   ip = (int_ptr_t) source;
  int_bytes_t tmp;
  int_32_t    p0 = POLY_HALF (p, 0);
  int_32_t    p1 = POLY_HALF (p, 1);
  int_bytes_t x;
  int_bytes_t y;
  poly_t      result;

  while (len > 0) {
    /* Byte swap the input word -- inline.  */
    y.w = *ip;
    x.b[0] = y.b[3];
    x.b[1] = y.b[2];
    x.b[2] = y.b[1];
    x.b[3] = y.b[0];

    /* Split the low-order bytes.  */
    tmp.w = p0;

    /* Compute the new result.  */
    p0 = word_xor (p1,
                   word_xor (word_xor (POLY_HALF (poly88[tmp.b[3]], 0),
                                       POLY_HALF (poly80[tmp.b[2]], 0)),
                             word_xor (POLY_HALF (poly72[tmp.b[1]], 0),
                                       POLY_HALF (poly64[tmp.b[0]], 0))));
    p1 = word_xor (x.w,
                   word_xor (word_xor (POLY_HALF (poly88[tmp.b[3]], 1),
                                       POLY_HALF (poly80[tmp.b[2]], 1)),
                             word_xor (POLY_HALF (poly72[tmp.b[1]], 1),
                                       POLY_HALF (poly64[tmp.b[0]], 1))));

    len -= sizeof (int_32_t);
    ++ip; /* The Modula-3 source is INC (ip, ADRSIZE (ip^)), but C's
             pointer arithmetic behaves differently.  */
  }
  POLY_FORM (result, p0, p1);
  return result;
}
#endif /* MAY_BE_BIG_ENDIAN */

static poly_t poly_extend_bytes (const poly_t t,
                                 const byte_t* addr,
                                 int len)
{
  const byte_t* cp = addr;
  integer_t     n_bits = 8 * len;
  integer_t     x_bits = 32 - n_bits;
  integer_t     t0 = word_and (POLY_HALF (t, 0), POLY_SIG_BITS);
  integer_t     t0_x = word_left_shift (t0, x_bits);
  integer_t     t0_n = word_right_shift (t0, n_bits);
  integer_t     t1 = word_and (POLY_HALF (t, 1), POLY_SIG_BITS);
  integer_t     t1_x = word_left_shift (t1, x_bits);
  integer_t     t1_n = word_right_shift (t1, n_bits);
  char          tmp[4];
  poly_t        result;

  POLY_HALF (result, 0) = poly_fix_32 (t0_x);
  POLY_HALF (result, 1) = poly_fix_32 (word_xor (t0_n, t1_x));

  switch (len) {
  case 1:
    tmp[0] = word_extract (t1_n, 0, 8);
    tmp[1] = word_extract (t1_n, 8, 8);
    tmp[2] = word_extract (t1_n, 16, 8);
    tmp[3] = cp[0];
    break;

  case 2:
    tmp[0] = word_extract (t1_n, 0, 8);
    tmp[1] = word_extract (t1_n, 8, 8);
    tmp[2] = cp[0];
    tmp[3] = cp[1];
    break;

  case 3:
    tmp[0] = word_extract (t1_n, 0, 8);
    tmp[1] = cp[0];
    tmp[2] = cp[1];
    tmp[3] = cp[2];
    break;

  default:
    assert (0);
  }

  if (poly_little_endian) {
#if MAY_BE_LITTLE_ENDIAN
    return poly_extend_words_le (result, tmp, 4);
#else /* MAY_BE_LITTLE_ENDIAN */
    ;
#endif /* MAY_BE_LITTLE_ENDIAN */
  } else {
#if MAY_BE_BIG_ENDIAN
    return poly_extend_words_be (result, tmp, 4);
#else /* MAY_BE_BIG_ENDIAN */
    ;
#endif /* MAY_BE_BIG_ENDIAN */
  }
}

/* This procedure assumes that the LEN bytes beginning at address ADDR
   define a polynomial, A(x) of degree 8 * LEN.  The procedure returns
   (INIT * x ^ (8 * LEN) + A(x)) % PolyBasis.P.  */

static poly_t poly_compute_mod (poly_t        init,
                                const byte_t* addr,
                                integer_t     len)
{
  integer_t j;
  integer_t k;
  poly_t    result = init;

#ifndef FINGERPRINT_LITTLE_ENDIAN
  /* We don't need to do this if we already know the endianness.  */
  if (!poly_init_done)
    poly_find_byte_order ();
#endif /* FINGERPRINT_LITTLE_ENDIAN */

  /* Word align the source pointer.  */
  j = mod ((word_t) addr, 4);
  if (len >= 4 && j != 0) {
    j = 4 - j;
    result = poly_extend_bytes (result, addr, j);
    addr += j;
    len -= j;
  }

  /* Compute the bulk of the result a word at a time.  */
  if (len >= 4) {
    j = mod ((word_t) len, 4);
    k = len - j;
    if (poly_little_endian) {
#if MAY_BE_LITTLE_ENDIAN
      result = poly_extend_words_le (result, addr, k);
#else /* MAY_BE_LITTLE_ENDIAN */
      ;
#endif /* MAY_BE_LITTLE_ENDIAN */
    } else {
#if MAY_BE_BIG_ENDIAN
      result = poly_extend_words_be (result, addr, k);
#else /* MAY_BE_BIG_ENDIAN */
      ;
#endif /* MAY_BE_BIG_ENDIAN */
    }
    addr += k;
    len = j;
  }

  /* Finish up the last few bytes.  */
  if (len > 0)
    result = poly_extend_bytes (result, addr, len);

  return result;
}

#if !FINGERPRINT_USE_INTEGRAL_TYPE
static void poly_to_bytes (poly_t t, byte_t* b)
{
  /* Generate the bytes in little-endian order.  */
  b[0] = word_extract (POLY_HALF (t, 0), 0, 8);
  b[1] = word_extract (POLY_HALF (t, 0), 8, 8);
  b[2] = word_extract (POLY_HALF (t, 0), 16, 8);
  b[3] = word_extract (POLY_HALF (t, 0), 24, 8);
  b[4] = word_extract (POLY_HALF (t, 1), 0, 8);
  b[5] = word_extract (POLY_HALF (t, 1), 8, 8);
  b[6] = word_extract (POLY_HALF (t, 1), 16, 8);
  b[7] = word_extract (POLY_HALF (t, 1), 24, 8);
}

static void poly_from_bytes (byte_t* b, poly_t* t)
{
  /* Assume the bytes are in little-endian order.  */
  POLY_HALF (*t, 0) =
    poly_fix_32 (word_or (word_or (                 b[0],
                                   word_left_shift (b[1], 8)),
                          word_or (word_left_shift (b[2], 16),
                                   word_left_shift (b[3], 24))));
  POLY_HALF (*t, 1) =
    poly_fix_32 (word_or (word_or (                 b[4],
                                   word_left_shift (b[5], 8)),
                          word_or (word_left_shift (b[6], 16),
                                   word_left_shift (b[7], 24))));
}
#else /* FINGERPRINT_USE_INTEGRAL_TYPE */
#define poly_to_bytes(t, b) (*((fingerprint_t*) (b)) = t)
#define poly_from_bytes(b, t) (*(t) = (*((poly_t*) b)))
#endif /* !FINGERPRINT_USE_INTEGRAL_TYPE */

/***********************************************************************
  Modula-3 `Fingerprint' Module
***********************************************************************/

/***********************************************************************
  Macros
***********************************************************************/

#define FINGERPRINT_A 0xff208489
#define FINGERPRINT_B 0xf4872e10
#define FINGERPRINT_C 0x402d619b
#define FINGERPRINT_D 0x0bf359a7

/***********************************************************************
  Variables
***********************************************************************/

#if !FINGERPRINT_USE_INTEGRAL_TYPE
const fingerprint_t
                fingerprint_zero;
#endif /* FINGERPRINT_USE_INTEGRAL_TYPE */

/* const */ fingerprint_t
                fingerprint_of_empty;

static const byte_t fingerprint_perm[256]
                        = { 55, 254, 252, 251, 250, 248, 240, 245,
                            246, 238, 237, 244, 7, 189, 214, 236,
                            235, 20, 33, 8, 227, 14, 233, 178,
                            172, 60, 229, 133, 152, 19, 210, 203,
                            221, 208, 76, 18, 13, 199, 113, 62,
                            40, 190, 213, 194, 43, 181, 21, 15,
                            201, 162, 90, 186, 71, 117, 107, 70,
                            191, 5, 173, 44, 39, 12, 174, 183,
                            99, 11, 176, 163, 161, 72, 86, 105,
                            2, 83, 42, 52, 179, 135, 103, 110,
                            151, 58, 108, 96, 166, 25, 115, 66,
                            142, 10, 141, 48, 104, 34, 159, 120,
                            22, 140, 64, 82, 78, 68, 207, 125,
                            123, 150, 144, 138, 128, 139, 136, 114,
                            119, 53, 148, 185, 41, 124, 216, 143,
                            49, 92, 98, 51, 112, 73, 50, 63,
                            16, 46, 158, 126, 206, 122, 94, 132,
                            88, 184, 28, 84, 127, 156, 167, 223,
                            118, 89, 116, 17, 111, 121, 109, 77,
                            146, 61, 224, 101, 81, 218, 97, 188,
                            243, 155, 57, 102, 54, 129, 93, 192,
                            153, 106, 36, 145, 79, 31, 137, 26,
                            67, 85, 175, 80, 168, 65, 91, 1,
                            147, 149, 6, 29, 37, 69, 182, 165,
                            4, 74, 55, 47, 171, 169, 75, 134,
                            193, 195, 198, 131, 38, 180, 56, 196,
                            23, 154, 177, 200, 205, 27, 209, 95,
                            204, 160, 3, 30, 157, 32, 9, 212,
                            211, 45, 202, 170, 0, 219, 187, 87,
                            35, 100, 217, 232, 164, 228, 220, 197,
                            231, 215, 226, 130, 225, 234, 241, 239,
                            59, 230, 247, 24, 249, 242, 222, 253
                        };

/***********************************************************************
  Function Definitions
***********************************************************************/

void fingerprint_init (void)
{
  /* Make sure that the configuration was correct.  */
#if UCHAR_MAX != 255
#error "The fingerprint module requires 8-bit characters."
#endif /* UCHAR_MAX != 255 */
  assert (sizeof (int_32_t) == 4);
  assert (sizeof (poly_t) == 8);
  assert (sizeof (fingerprint_t) == 8);
  assert (sizeof (void*) == sizeof (integer_t));

  /* Intialize.  */
  poly_to_bytes (POLY_ONE, FINGERPRINT_BYTE (fingerprint_of_empty));
}


/* 
** strings may contain null chars.  thus we need this function
** for fingerprinting binary data.  rewrote fingerprint_from_text
** to use this function instead.
**
**    - Bj�rn Borud <borud@fast.no>
*/
fingerprint_t 
fingerprint_from_buffer (const char *buffer, int size)
{
  fingerprint_t result;
  poly_t        poly;

  poly = poly_compute_mod (POLY_ONE,
                           (const byte_t*) buffer,
                           (integer_t) size);
  poly_to_bytes (poly, FINGERPRINT_BYTE (result));

  return result;
}

inline fingerprint_t 
fingerprint_from_text (const char* text)
{
  return fingerprint_from_buffer(text, strlen(text));
}

fingerprint_t fingerprint_combine (fingerprint_t fp1,
                                   fingerprint_t fp2)
{
  poly_t        poly1;
  poly_t        poly2;
  fingerprint_t buf[2];
  fingerprint_t res;
  int           i;

  buf[0] = fp1;
  buf[1] = fp2;

  poly1 = poly_compute_mod (POLY_ONE,
                            (const byte_t*) &buf[0],
                            sizeof (buf));

  POLY_HALF (poly2, 0) =
    poly_fix_32 (word_plus (word_times (POLY_HALF (poly1, 0),
                                        FINGERPRINT_A),
                            word_times (POLY_HALF (poly1, 1),
                                        FINGERPRINT_B)));
  POLY_HALF (poly2, 1) =
    poly_fix_32 (word_plus (word_times (POLY_HALF (poly1, 0),
                                        FINGERPRINT_C),
                            word_times (POLY_HALF (poly1, 1),
                                        FINGERPRINT_D)));
  poly_to_bytes (poly2, FINGERPRINT_BYTE (res));

  for (i = 0; i < 8; ++i)
    FINGERPRINT_BYTE (res)[i] =
      fingerprint_perm [FINGERPRINT_BYTE (res)[i]];

  return res;
}

fingerprint_t fingerprint_from_chars (const char*   text,
                                      fingerprint_t fp)
{
  integer_t     n = (integer_t) strlen (text);
  fingerprint_t result;
  poly_t        init;
  poly_t        poly;

  if (n == 0)
    return fp;

  poly_from_bytes (FINGERPRINT_BYTE (fp), &init);
  poly = poly_compute_mod (init, (const byte_t*) text, n);
  poly_to_bytes (poly, FINGERPRINT_BYTE (result));

  return result;
}

#if !FINGERPRINT_USE_INTEGRAL_TYPE
int fingerprint_equal (fingerprint_t fp1,
                       fingerprint_t fp2)
{
  return memcmp (&fp1, &fp2, sizeof (fingerprint_t)) == 0;
}
#endif /* !FINGERPRINT_USE_INTEGRAL_TYPE */

int fingerprint_equal_f (fingerprint_t fp1,
                         fingerprint_t fp2)
{
  return fingerprint_equal (fp1, fp2);
}

word_t fingerprint_hash (fingerprint_t fp)
{
  poly_t x;
  poly_from_bytes (FINGERPRINT_BYTE (fp), &x);
  return word_xor (POLY_HALF (x, 0), POLY_HALF (x, 1));
}

/***********************************************************************
  Unit Test
***********************************************************************/

#undef FINGERPRINT_TEST
#ifdef FINGERPRINT_TEST
#include <stdio.h>

static void fingerprint_print (fingerprint_t fp, byte_t* buffer)
{
  int i;

  buffer += sprintf (buffer, "{");
  for (i = 0; i < 8; ++i) {
    buffer +=
      sprintf (buffer, "%s%x", i != 0 ? ", " : "",
               (unsigned int) (FINGERPRINT_BYTE (fp))[i]);
  }
  buffer += sprintf (buffer, "}");
}

static fingerprint_t fingerprint_print_with_text (const char* text)
{
  char buffer[64];
  fingerprint_t fp;

  fp = fingerprint_from_text (text);
  fingerprint_print (fp, buffer);
  printf ("The fingerprint of:\n  %s\nis:\n  %s\n",
          text, buffer);
  return fp;
}

static int fingerprint_compare (fingerprint_t fp,
                                unsigned      bytes[])
{
  int i;
  for (i = 0; i < 8; ++i) {
    if (FINGERPRINT_BYTE (fp)[i] != bytes[i])
      return 0;
  }

  return 1;
}

int main()
{
  char          buffer[64];
  fingerprint_t fp1;
  fingerprint_t fp2;
  fingerprint_t fp3;
  fingerprint_t fp4;

  unsigned      fp_of_good_men [] = { 0x79, 0x62, 0xbf, 0x45, 0xa3,
                                      0x53, 0xb2, 0x2b };
  unsigned      fp_of_quick_fox [] = { 0x53, 0x3e, 0x7b, 0x88, 0x6,
                                       0x19, 0xba, 0x38 };
  unsigned      fp_of_combine [] = { 0xd4, 0x18, 0x54, 0x6, 0xa7,
                                     0x68, 0x8c, 0x35 };

#define XSTR(X) #X
#define STR(X) XSTR(X)

  printf ("Configuration\n"
          "-------------\n"
          "FINGERPRINT_INTEGRAL_TYPE:    %s\n"
          "FINGERPRINT_INT_32_TYPE:      %s\n"
          "FINGERPRINT_POINTER_INT_TYPE: %s\n"
          "FINGERPRINT_LITTLE_ENDIAN:    %s\n\n"
          "Tests\n"
          "-----\n",
#ifdef FINGERPRINT_INTEGRAL_TYPE
          STR(FINGERPRINT_INTEGRAL_TYPE),
#else /* ifndef FINGERPRINT_INTEGRAL_TYPE */
          "undefined",
#endif /* ifdef FINGERPRINT_INTEGRAL_TYPE */
          STR(FINGERPRINT_INT_32_TYPE),
          STR(FINGERPRINT_POINTER_INT_TYPE),
#ifdef FINGERPRINT_LITTLE_ENDIAN
          STR(FINGERPRINT_LITTLE_ENDIAN)
#else /* ifndef FINGERPRINT_LITTLE_ENDIAN */
          "undefined"
#endif /* ifdef FINGERPRINT_INTEGRAL_TYPE */
          );

  fingerprint_init ();

  fp1 = fingerprint_print_with_text ("");
  fp2 = fingerprint_print_with_text ("Now is the time for all good men "
                                     "to come to the aid of their country.");
  fp3 = fingerprint_print_with_text ("The quick brown fox jumped over the "
                                     "lazy dog.");

  fp4 = fingerprint_combine (fp2, fp3);
 fingerprint_print (fp4, buffer);
  printf ("The fingerprint of (\"Now...\", \"The quick...\") is:\n  %s\n",
          buffer);
  fflush (stdin);

  assert (fingerprint_equal (fp1, fingerprint_of_empty));
  assert (fingerprint_compare (fp2, fp_of_good_men));
  assert (fingerprint_compare (fp3, fp_of_quick_fox));

  assert (fingerprint_compare (fp4, fp_of_combine));

  return 0;
}
#endif /* ifdef FINGERPRINT TEST */
