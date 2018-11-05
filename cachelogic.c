#include "tips.h"
#include "math.h"

/* The following two functions are defined in util.c */

/* finds the highest 1 bit, and returns its position, else 0xFFFFFFFF */
unsigned int uint_log2(word w);

/* return random int from 0..x-1 */
int randomint( int x );

	/*
	This function allows the lfu information to be displayed

		assoc_index - the cache unit that contains the block to be modified
		block_index - the index of the block to be modified

	returns a string representation of the lfu information
	*/
char* lfu_to_string(int assoc_index, int block_index) {
	/* Buffer to print lfu information -- increase size as needed. */
	static char buffer[9];
	sprintf(buffer, "%u", cache[assoc_index].block[block_index].accessCount);

	return buffer;
}

	/*
	This function allows the lru information to be displayed

		assoc_index - the cache unit that contains the block to be modified
		block_index - the index of the block to be modified

	returns a string representation of the lru information
	*/
char* lru_to_string(int assoc_index, int block_index)
	{
	/* Buffer to print lru information -- increase size as needed. */
	static char buffer[9];
	sprintf(buffer, "%u", cache[assoc_index].block[block_index].lru.value);

	return buffer;
}

	/*
	This function initializes the lfu information

		assoc_index - the cache unit that contains the block to be modified
		block_number - the index of the block to be modified

	*/
void init_lfu(int assoc_index, int block_index) {
  cache[assoc_index].block[block_index].accessCount = 0;
}

	/*
	This function initializes the lru information

		assoc_index - the cache unit that contains the block to be modified
		block_number - the index of the block to be modified

	*/
void init_lru(int assoc_index, int block_index) {
  cache[assoc_index].block[block_index].lru.value = 0;
}

	/*
	This is the primary function you are filling out,
	You are free to add helper functions if you need them

	@param addr 32-bit byte address
	@param data a pointer to a SINGLE word (32-bits of data)
	@param we   if we == READ, then data used to return
				information back to CPU

				if we == WRITE, then data used to
				update Cache/DRAM
	*/
void accessMemory(address addr, word* data, WriteEnable we) {
	/* Declare variables here */
	unsigned int index_bits;
	unsigned int offset_bits;
	unsigned int tag_bits;
	unsigned int tag;
	unsigned int offset;
	unsigned int index;
	unsigned int mask = 0;
	//cacheBlock temp;
	int b = 0;
	int aNum = 0;
	//unsigned int tempAddr;

	/* handle the case of no cache at all - leave this in */
	if(assoc == 0) {
		accessDRAM(addr, (byte*)data, WORD_SIZE, we);
		return;
	}

	switch (block_size){
		case 4:
		{
			offset_bits = 2;
			break;
		}
		case 8:
		{
			offset_bits = 3;
			break;
		}
		case 16:
		{
			offset_bits = 4;
			break;
		}
		case 32:
		{
			offset_bits = 5;
			break;
		}
		default:
		{
			return;
		}
	}
	switch (set_count){
		case 1:
		{
			index_bits = 0;
			break;
		}
		case 2:
		{
			index_bits = 1;
			break;
		}
		case 4:
		{
			index_bits = 2;
			break;
		}
		case 8:
		{
			index_bits = 3;
			break;
		}
		case 16:
		{
			index_bits = 4;
			break;
		}
		default:
		{
			return;
		}
	}
	// get tag
	tag_bits = 32 - (index_bits + offset_bits);
	for(int i = 0;i < tag_bits; i++){
		mask += 1 << (32 - i);
	}
	tag = addr & mask;
	tag = tag >> (32 - tag_bits);
	//get index
	mask = 0;
	for(int i = 0; i < index_bits; i++){
		mask += 1 << (32 - tag_bits - i);
	}
	index = addr & mask;
	index = index >> (32 - index_bits);
	//get offset
	mask = 0;
	for(int i = 0; i < offset_bits; i++){
		mask += 1 << (i);
	}
	offset = addr & mask;
	//determine if this is a hit or a miss
	for(; b < assoc; b++){
		//if it's a hit, grab the data for a read, or write data for a write
		if(cache[index].block[b].tag == tag && cache[index].block[b].valid == VALID){
			if(we == READ){
				memcpy(data, cache[index].block[b].data + offset, 4);
			}
			else{
				memcpy(cache[index].block[b].data + offset, data, 4);
				if(memory_sync_policy == WRITE_THROUGH){
					accessDRAM(addr, cache[index].block[b].data, block_size, (flag)WRITE);
				}
				else{
					cache[index].block[b].dirty = DIRTY;
				}
			}
			highlight_offset(index, b, offset, HIT);
			return;
		}
	}
	//on a miss, choose block to replace
	if(assoc > 1){
		if(policy == LRU){
			//determine LRU block
			for(int i = 0; i < assoc; i++){
				if(cache[index].block[i].lru.value++ > cache[index].block[aNum].lru.value){
					aNum = i;
				}
			}
		}
		else if(policy == LFU){
			return;	//fuck off, we're not supposed to need to do this
		}
		else{
			//otherwise, use a random number
			aNum = randomint(assoc);
		}
	}

	//reset LRU value for the chosen block
	cache[index].block[aNum].lru.value = 0;
	// if the block has been changed, write back to memory before replacing it
	if(cache[index].block[aNum].dirty == DIRTY){
		accessDRAM(addr, cache[index].block[aNum].data, block_size, (flag)WRITE);
	}
	//highlight chosen block
	highlight_block(index ,aNum);
	highlight_offset(index, aNum, offset, MISS);
	//copy block from memory
	/*if(*/!accessDRAM(addr, cache[index].block[aNum].data, MAX_BLOCK_SIZE, (flag)READ);//){
		if(we == WRITE){
			memcpy(cache[index].block[aNum].data + offset, data, 4);
			//check memory sync policy and act accordingly
			if(memory_sync_policy == WRITE_THROUGH){
				accessDRAM(addr, cache[index].block[aNum].data, MAX_BLOCK_SIZE, (flag)WRITE);
			}
			else{
				cache[index].block[aNum].dirty = DIRTY;
			}
		}
		else{
			memcpy(data, cache[index].block[aNum].data + offset, 4);
		}
	//}
	

	/*
	You need to read/write between memory (via the accessDRAM() function) and
	the cache (via the cache[] global structure defined in tips.h)

	Remember to read tips.h for all the global variables that tell you the
	cache parameters

	The same code should handle random, LFU, and LRU policies. Test the policy
	variable (see tips.h) to decide which policy to execute. The LRU policy
	should be written such that no two blocks (when their valid bit is VALID)
	will ever be a candidate for replacement. In the case of a tie in the
	least number of accesses for LFU, you use the LRU information to determine
	which block to replace.

	Your cache should be able to support write-through mode (any writes to
	the cache get immediately copied to main memory also) and write-back mode
	(and writes to the cache only gets copied to main memory when the block
	is kicked out of the cache.

	Also, cache should do allocate-on-write. This means, a write operation
	will bring in an entire block if the block is not already in the cache.

	To properly work with the GUI, the code needs to tell the GUI code
	when to redraw and when to flash things. Descriptions of the animation
	functions can be found in tips.h
	*/

	/* Start adding code here */


	/* This call to accessDRAM occurs when you modify any of the
	 cache parameters. It is provided as a stop gap solution.
	 At some point, ONCE YOU HAVE MORE OF YOUR CACHELOGIC IN PLACE,
	 THIS LINE SHOULD BE REMOVED.
	*/
	//accessDRAM(addr, (byte*)data, WORD_SIZE, we);
}