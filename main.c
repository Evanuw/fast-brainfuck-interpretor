 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#ifdef _WIN32
	#include <conio.h>
#else
	#include <termios.h>
	#include <sys/mman.h>
#endif

static void executebrainfuck(char *bfProgram, uint32_t bfProgramSize){

	// The compiled code will use RDI (calling convention first arg) as the data pointer.
	// and will use linux syscalls to output data.

	const uint8_t RDI_INC[] = { 0x48, 0xFF, 0xC7 }; // inc RDI
	const uint8_t RDI_ADD[] = { 0x48, 0x81, 0xC7 }; // add RDI, imm32
	const uint8_t RDI_DEC[] = { 0x48, 0xFF, 0xCF }; // dec RDI
	const uint8_t RDI_SUB[] = { 0x48, 0x81, 0xEF }; // sub RDI, imm32
	const uint8_t RDI_PTR_INC[] = { 0xFE, 0x07 }; // inc BYTE PTR [RDI]
	const uint8_t RDI_PTR_ADD[] = { 0x80, 0x07 }; // add BYTE [RDI], imm8
	const uint8_t RDI_PTR_DEC[] = { 0xFE, 0x0F }; // dec BYTE PTR [RDI]
	const uint8_t RDI_PTR_SUB[] = { 0x80, 0x2F }; // sub BYTE [RDI], imm8

	// This one call WRITE(1, (void *)RDI, 1)

	const uint8_t PUTCHAR[] = { 0x48, 0x31, 0xC0, // xor RAX, RAX 
								0x48, 0xFF, 0xC0, // inc RAX
								0x57,             // push RDI
								0x48, 0x89, 0xFE, // mov rsi, rdi
								0x48, 0x89, 0xC7, // mov rdi, rax
								0x48, 0x89, 0xC2, // mov rdx, rax
								0x0F, 0x05,       // syscall
								0x5F };           // pop RDI

	const uint8_t END[] = {0xC3}; // RET

	// for jumps, the relative 32 bit number must be added next to the bytes.

	const uint8_t JUMP_IF_ZERO[] = { 0x80, 0x3F, 0x00, // cmp BYTE PTR [RDI], 0
									 0x0F, 0x84}; // jz REL32
	const uint8_t JUMP_IF_NOT_ZERO[] = { 0x80, 0x3F, 0x00, // cmp BYTE PTR [RDI], 0
									 0x0F, 0x85}; // jnz REL32


	uint8_t *mem = calloc(30000, sizeof(uint8_t)); // mémoire.

	size_t mappedSize = bfProgramSize*sizeof(PUTCHAR);

	uint32_t *hooksPointer = calloc(bfProgramSize, sizeof(uint32_t)); // store the corresponding offset in the code for each hooks.

	// will contain the compiled brainfuck code.
	uint8_t *code = mmap(NULL, mappedSize,
		PROT_EXEC | PROT_WRITE | PROT_READ, 
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
		-1, 0);

	if(code==MAP_FAILED){
		free(mem);
		printf("Huuummmmm out of memory\n");
		return;
	}

	uint8_t *writePtr = code;
	int32_t accu = 0; // accumulation counter of repeating chars

	for(uint32_t i = 0; i < bfProgramSize; i++){
		char nextChar=' ';
		if(i != (bfProgramSize-1) )
			nextChar=bfProgram[i+1];

		switch(bfProgram[i]){
			case '>':

				if(nextChar!='>'){
					if(accu==0){
						(void)memcpy(writePtr, RDI_INC, sizeof(RDI_INC));
						writePtr+=sizeof(RDI_INC);
					}else{
						accu++;
						// simplifie by an ADD
						(void)memcpy(writePtr, RDI_ADD, sizeof(RDI_ADD));
						writePtr+=sizeof(RDI_ADD);
						(void)memcpy(writePtr, &accu, sizeof(int32_t));
						writePtr+=sizeof(int32_t);
						accu=0;
					}
				}else{
					accu+=1;
				}

				break;
			case '<':
				if(nextChar!='<'){
					if(accu==0){
						(void)memcpy(writePtr, RDI_DEC, sizeof(RDI_DEC));
						writePtr+=sizeof(RDI_DEC);
					}else{
						accu++;
						// simplifie by an SUB
						(void)memcpy(writePtr, RDI_SUB, sizeof(RDI_SUB));
						writePtr+=sizeof(RDI_SUB);
						(void)memcpy(writePtr, &accu, sizeof(int32_t));
						writePtr+=sizeof(int32_t);
						accu=0;
					}
				}else{
					accu+=1;
				}
				break;
			case '+':
				if(nextChar!='+'){
					if(accu==0){
						(void)memcpy(writePtr, RDI_PTR_INC, sizeof(RDI_PTR_INC));
						writePtr+=sizeof(RDI_PTR_INC);
					}else{
						accu++;
						// simplifie by an ADD
						(void)memcpy(writePtr, RDI_PTR_ADD, sizeof(RDI_PTR_ADD));
						writePtr+=sizeof(RDI_PTR_ADD);
						(void)memcpy(writePtr, &accu, sizeof(int8_t)); // take just the first LSB
						writePtr+=sizeof(int8_t);
						accu=0;
					}
				}else{
					accu+=1;
				}
				break;
			case '-':
				if(nextChar!='-'){
					if(accu==0){
						(void)memcpy(writePtr, RDI_PTR_DEC, sizeof(RDI_PTR_DEC));
						writePtr+=sizeof(RDI_PTR_DEC);
					}else{
						accu++;
						// simplifie by an ADD
						(void)memcpy(writePtr, RDI_PTR_SUB, sizeof(RDI_PTR_SUB));
						writePtr+=sizeof(RDI_PTR_SUB);
						(void)memcpy(writePtr, &accu, sizeof(int8_t)); // take just the first LSB
						writePtr+=sizeof(int8_t);
						accu=0;
					}
				}else{
					accu+=1;
				}
				break;
			case '.':
				(void)memcpy(writePtr, PUTCHAR, sizeof(PUTCHAR));
				writePtr+=sizeof(PUTCHAR);
				break;
			case '[':
				(void)memcpy(writePtr, JUMP_IF_ZERO, sizeof(JUMP_IF_ZERO));
				writePtr+=sizeof(JUMP_IF_ZERO)+4; // relative offset will be written in the next case.
				hooksPointer[i] = (uint32_t)(writePtr-code);
				break;
			case ']':
				(void)memcpy(writePtr, JUMP_IF_NOT_ZERO, sizeof(JUMP_IF_NOT_ZERO));
				writePtr+=sizeof(JUMP_IF_NOT_ZERO);
				uint32_t depth = 0;
				for(uint32_t p = (i-1); p != (~0); p-- ){ // inverted for loop
					if(bfProgram[p]==']')
						depth++;
					if(bfProgram[p]=='['){
						if(depth==0){

							int32_t relativeOffset =  hooksPointer[p]-(((int32_t)(writePtr-code))+4);
							(void)memcpy(writePtr, &relativeOffset, sizeof(int32_t));

							// add the relative offset for the corresponding '['

							relativeOffset = 0-relativeOffset;
							(void)memcpy( (void *)(code+hooksPointer[p]-4), &relativeOffset, sizeof(uint32_t) );
							break;
						}else{
							depth--;
						}
					}
				}
				writePtr+=sizeof(uint32_t);
				break;

			

		}
	}

	(void)memcpy(writePtr, END, sizeof(END));

	FILE *file = fopen("compiledBrainfuck.bin", "wb");
	(void)fwrite(code, mappedSize, 1, file);
	fclose(file);

	free(hooksPointer);

	((void (*)(void *))code)(mem);


	
	munmap(code, mappedSize);
	free(mem);
}

int main(int argc, char const *argv[])
{
	if(argc!=2){
		printf("Nombre d'argument invalide ):<\n");
		return 1;
	}

	FILE * file = fopen(argv[1], "rb"); // Ouvre en mode binaire pour éviter les bizzarerie.

	if(file==NULL){
		printf("Je trouve pas\n");
		return 1;
	}

	// obtention de la longueur du programme.

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *bfProgram = malloc(size+1);
	if(fread(bfProgram, size, 1, file) == 0){ // Lis les données du fichier.
		free(bfProgram);
		printf("Erreur de lecture du fichier\n");
		return 1;
	}

	bfProgram[size]=0; // null terminator.

	fclose(file);

#ifndef _WIN32
	// change les paramètres de la console.
	struct termios settings;
	tcgetattr(0, &settings);
	settings.c_lflag &= ~ICANON;
	tcsetattr(0, 0, &settings);

#endif

	executebrainfuck(bfProgram, (uint32_t)size);

	free(bfProgram); // libère la mémoire.
	return 0;
}