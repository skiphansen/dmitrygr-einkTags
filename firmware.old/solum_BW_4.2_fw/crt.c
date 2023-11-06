

//main must exist
extern int main(void);

extern void __data_data();
extern void __data_start();
extern void __data_end();
extern void __bss_start();
extern void __bss_end();


void __attribute__((used, naked, section(".vectors"))) __entry(void)
{
	unsigned int *dst, *src, *end;

	//copy data
	dst = (unsigned int*)&__data_start;
	src = (unsigned int*)&__data_data;
	end = (unsigned int*)&__data_end;
	while(dst != end)
		*dst++ = *src++;

	//init bss
	dst = (unsigned int*)&__bss_start;
	end = (unsigned int*)&__bss_end;
	while(dst != end)
		*dst++ = 0;
	
	main();
}

