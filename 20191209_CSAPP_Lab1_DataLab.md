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

3. logicalShift
```
/*
 * LogicalShift - shift x to the right by n, using a logical shift
 *      Can assume that 0 <= n <= 31
 *      Examples: logicalShift(0x87654321, 4) = 0x08765432
 *      Legal ops: ! ~ & ^ | + << >>
 *      Max ops: 20
 *      Rating: 3
 */
 int logicalShift(int x, int n) {
     //make a mask
     int z = 1 << 31;
     int filter = ~((z >> n) << 1);
     int result = (x >> n) & filter;
     return result;
 }
 ```
 
 4. bitCount
 
 ```
 /*
  * bitCount - returns count of number of 1's in word
  *     Example : bitCount(5) = 2, bitCount(7) = 3
  *     Legal ops: ! ~ & ^ | + << >>
  *     Max ops: 40
  *     Rating: 4
  */
  int bitCount(int x) {
      
  }
 ```
 
 5. bang

 ```
 /*
  * bang - Compute !x without using !
  *     Example: bang(3) = 0, bang(0) = 1
  *     Legal ops: ~ & ^ | + << >>
  *     Max ops: 12
  *     Rating: 4
  */
  int bang(int x) {
      // when x is 0, temp1 is overflow, it's value is 0
      int temp1 = ~x + 1;
      // when x is 0, temp2 is 0, else its top most bit must be 1
      int temp2 = x | temp1;
      // Negation' mainly purpose is to switch the top most bit
      // x = 0, temp3 = 0xffffffff, else temp3 = 0x0
      int temp3 = ~temp2;

      //Right shift of 31 bits
      // x = 0, mask = 0xffffffff, else mask = 0
      int mask = temp3 >> 31;

      return mask & 1;
  }
 ```
 
