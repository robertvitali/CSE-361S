#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>

//Robert Vitali 457235

//global variables
int hits = 0;
int misses = 0;
int evictions = 0;
unsigned long counter = 0;


//this is the structure that is storing an individual "line" of information (based on slides)
typedef struct{
	int validBit;
	unsigned int tag;
	unsigned long timestamp;
} CacheEntry;

//this struct is storing multiple cache entries in one set
typedef struct{
	CacheEntry *entry;
} CacheSet;

//this stores the multiple sets that make up the cache 
typedef struct{
	CacheSet *sets;
} Cache;

//checking a hit
void checkHit(Cache *localCache, unsigned int tempTag, int E, int s, int b, long addr){

	unsigned long setBits = (addr << (64 - s - b));
	setBits = setBits >> (64 - s);
	
	//checking if there is a hit
	for(int i = 0; i < E; i++){
		printf(" TEMP TAG: %d", tempTag);
		printf(" CHECK TAG: %d", (localCache -> sets[setBits].entry[i].tag));
		printf(" VALID BIT %d\n", (localCache -> sets[setBits].entry[i].validBit));
		if((localCache -> sets[setBits].entry[i].tag) == tempTag){
			if((localCache -> sets[setBits].entry[i].validBit) == 1){
				hits++;
				counter++;
				localCache -> sets[setBits].entry[i].timestamp = counter;
				printf("HIT!!!\n");
				return;
			}
		}
	}
	printf("MISS!!!!\n");

	//checking if there is a open spot
	for(int j = 0; j < E; j++){
		if((localCache -> sets[setBits].entry[j].validBit) == 0){
			misses++;
			counter++;
			localCache -> sets[setBits].entry[j].timestamp = counter;
			localCache -> sets[setBits].entry[j].tag = tempTag;
			localCache -> sets[setBits].entry[j].validBit = 1;
			return;
		}
	}

	printf("EVICTION!!!\n");

	//no spot? check which to evict
	unsigned long ts = localCache -> sets[setBits].entry[0].timestamp;
	int lowestIndex = 0;
	for(int k = 1; k < E; k++){
		if((localCache -> sets[setBits].entry[k].timestamp) < ts){
			ts = localCache -> sets[setBits].entry[k].timestamp;
			lowestIndex = k;
		}
	}
	counter++;
	localCache -> sets[setBits].entry[lowestIndex].timestamp = counter;
	localCache -> sets[setBits].entry[lowestIndex].tag = tempTag;
	localCache -> sets[setBits].entry[lowestIndex].validBit = 1;
	misses++;
	evictions++;
}


// // //checking a miss
// void checkMiss(Cache *localCache, unsigned int tempTag, int E, int s, int b, long addr){
// 	unsigned setBits = (addr << (64 - s - b));
// 	setBits = setBits >> (64 - s);

// 	//checking if there is a open spot
// 	for(int j = 0; j < E; j++){
// 		if((localCache -> sets[setBits].entry[j].validBit) == 0){
// 			misses++;
// 			counter++;
// 			localCache -> sets[setBits].entry[j].timestamp = counter;
// 			localCache -> sets[setBits].entry[j].tag = tempTag;
// 			localCache -> sets[setBits].entry[j].validBit = 1;
// 			break;
// 		}
// 	}
// }

// //checking an eviction
// void checkEviction(Cache *localCache, unsigned int tempTag, int E, int s, int b, long addr){
// 	unsigned setBits = (addr << (64 - s - b));
// 	setBits = setBits >> (64 - s);

// 	//no spot? check which to evict
// 	unsigned long lowestTimestamp = localCache -> sets[setBits].entry[0].timestamp;
// 	int lowestIndex = 0;
// 	for(int k = 1; k < E; k++){
// 		if((localCache -> sets[setBits].entry[k].timestamp) < lowestTimestamp){
// 			lowestTimestamp = localCache -> sets[setBits].entry[k].timestamp;
// 			lowestIndex = k;
// 		}
// 	}
// 	counter++;
// 	localCache -> sets[setBits].entry[lowestIndex].timestamp = counter;
// 	localCache -> sets[setBits].entry[lowestIndex].tag = tempTag;
// 	localCache -> sets[setBits].entry[lowestIndex].validBit = 1;
// 	misses++;
// 	evictions++;
// }


int main(int argc, char *argv[])
{
   int s; //number of set index bits
   int E; //associativity (number of lines per set)
   int b; //number of block bits, B = 2^b is the block size
   FILE *t; //the file being read
   char op;
   long addr;
   int size;
   long numberOfSets;
   // long sizeOfBlock;
   Cache localCache; //local variable to init cache
   //CacheSet tempSet; //this is used to caputre the specific set we are going to iterate over
   char var;
   unsigned int tempTag;
   while((var = getopt(argc, argv, "s:E:b:t:")) != -1){
	switch(var){
		case 's':
		s = atoi(optarg);
		break;
		case 'E':
		E = atoi(optarg);
		break;
		case 'b':
		b = atoi(optarg);
		break;
		case 't':
		t = fopen(optarg, "r");
		break;
	}
   }

   //printing out varaibles
   //printf("s: %d, E: %d, b: %d", s, E, b);

   numberOfSets = pow(2.0, s); //number of sets
   // sizeOfBlock = pow(2.0, b); //size of block

   //allocating space for all of the sets in the cache
   localCache.sets = (CacheSet *) malloc(numberOfSets * sizeof(CacheSet));

   //allocation of space for each set
   for(int i = 0; i < numberOfSets; i++){
   	localCache.sets[i].entry = (CacheEntry *) malloc(E * sizeof(CacheEntry));
   }  


   while(fscanf(t," %c %lx, %d",&op,&addr,&size) == 3){
	tempTag = addr >> (s + b);
	//printf("enter while loop to read lines");
	switch(op){
		case 'L'://load from memory to cache
		checkHit(&localCache, tempTag, E, s, b, addr);
		break;

		case 'M'://load from memory, make changes, then store in memory
		checkHit(&localCache, tempTag, E, s, b, addr);
		checkHit(&localCache, tempTag, E, s, b, addr);
		break;

		case 'S'://store in memory from cache
		checkHit(&localCache, tempTag, E, s, b, addr);
		break;

		case 'I':
		break;
		}
	}

   
    printSummary(hits, misses, evictions);
    return 0;
}


