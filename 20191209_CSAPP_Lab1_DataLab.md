1. bitAnd
```
/*
 * bitAnd - x&y using only ~ and |
 * Example: bitAnd(6, 5) = 4
 * Legal ops: ~ |
 * Max ops : 8
 * Rating: 1
 */
 int bitAnd (int x, int y) {
     return ~((~x) | (~y));
 }
```
2. getByte
```
/*
 * getByte - Extract byte n from word x
 *      Bytes numbered from 0 (LSB) to 3 (MSB)
 *      Examples: getByte(0x12345678, 1) = 0x56
 *      Legal ops: ! ~ & ^ | + << >>
 *      Max ops:6
 *      Rating: 2
 */
 int getByte(int x, int n) {
     return (x >> (n << 3)) & 0xFF;
 }
 
 int getByte2(int x, int n) {
     return ((0xFF << (n << 3)) & x) >> (n << 3);
 }
```