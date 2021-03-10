#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


typedef struct check32
{
    uint32_t a32type;
}CHECK32;


typedef struct check16
{
    uint32_t a16type;
}CHECK16;

typedef struct check48
{
	uint8_t s48type[4];
}CHECK48;

int main()
{
	CHECK32* a32;
	CHECK48* a48=NULL;
	CHECK16* a16=NULL;
	uint8_t* p;

	printf("size of 32: %d\n", (int)sizeof(*a32));
	printf("size of 48: %d\n", (int)sizeof(*a48));

	a32->a32type = 0x56813846;
	printf("value in 32 is: %08X\n", a32->a32type);

	a48 = a32;
	printf("value in 48 is: %02X%02X%02X%02X\n", a48->s48type[3], a48->s48type[2], a48->s48type[1], a48->s48type[0]);

	a16 = (CHECK16*)(a48->s48type + 2);
	printf("value in 16 is: %08X\n", a16->a16type);

	a32->a32type = 0x12893846;
	p = (uint8_t*)&a32->a32type;
	printf("value in -p is: %02X%02X%02X%02X\n", p[3], p[2], p[1], p[0]);

	a32->a32type = 0x12897556;
	p = (uint8_t*)a48->s48type;
	printf("value in -p is: %02X%02X%02X%02X\n", p[3], p[2], p[1], p[0]);

	p = (uint8_t*)a32;
	printf("value in  p is: %02X%02X%02X%02X\n", p[3], p[2], p[1], p[0]);

	return 0;
}