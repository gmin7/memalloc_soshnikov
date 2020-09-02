#include <iostream>
#include <unistd.h>   // for sbrk
#include <assert.h>

// http://dmitrysoshnikov.com/compilers/writing-a-memory-allocator/#assignments

using word_t = intptr_t;

struct Block 
{
  // object header
  size_t size;
  bool used;
  Block *next;
  // user data
  word_t data[1];
};

// Heap start. Initialized on first allocation.
static Block *heapStart = nullptr;
 
// Current top. Updated on each allocation.
static auto top = heapStart;

// Aligns the size by the machine word.
inline size_t align(size_t n) 
{
  return (n + sizeof(word_t) - 1) & ~(sizeof(word_t) - 1);
};

// Returns the total allocation size, reserving
// space for the Block structure
inline size_t allocSize(size_t size) 
{

  std::cout << "Block size: " << sizeof(Block) << std::endl;
  std::cout << "Block size: " <<    align( sizeof(std::declval<Block>().size) + 
    sizeof(std::declval<Block>().next) + 
    sizeof(std::declval<Block>().used) + 
    sizeof(std::declval<Block>().data))   << std::endl;
  return size + sizeof(Block) - sizeof(std::declval<Block>().data); // Snazzy !
};

Block *requestFromOS(size_t size)
{
  // Get the current heap break (program break pointer)
  Block* block = (Block*)sbrk(0);

  // Check for OOM
  if( sbrk(allocSize(size))==(void *)-1 ) return nullptr;
  return block;
};

Block *getHeader(word_t *data)
{
  return (Block *)((char *)data + sizeof(std::declval<Block>().data) - sizeof(Block));
  // return (Block *)data - sizeof(std::declval<Block>().next) - sizeof(std::declval<Block>().used) - sizeof(std::declval<Block>().size);
};

void free(word_t *data)
{
  auto block = getHeader(data);
  block->used = false;
};

/////// Block searching algos ///////

// Mode for searching a free block.
enum class SearchMode {
  FirstFit,
  NextFit,
  BestFit,
};
 
// Previously found block. Updated in `nextFit`
static Block *searchStart = heapStart;

// Current search mode
static auto searchMode = SearchMode::FirstFit;

// Reset heap to its original position
void resetHeap()
{
  if(heapStart == nullptr) return;

  // Roll back to the beginning!
  brk(heapStart);
  heapStart = nullptr;
  top = nullptr;
  searchStart = nullptr;
};

// Initializes the heap, and the search mode.
void init(SearchMode mode) 
{
  searchMode = mode;
  resetHeap();
}

/**
 * First-fit algorithm
 * 
 * The first-fit algorithm traverses all the blocks starting at the 
 * beginning of the heap (the heapStart which we initialized on first allocation). 
 * It returns the first found block if it fits the size, or the nullptr otherwise.
*/
Block *firstFit(size_t size)
{
  auto block = heapStart;
  while(block!=nullptr)
  {
    // O(n) search
    if(block->used || block->size < size)
    {
      block = block->next;
      continue;
    }
    // Found a free block
    return block;
  }
  return nullptr;
};

/**
 * Next-fit algorithm.
 *
 * Returns the next free block which fits the size.
 * Updates the `searchStart` of success.
 */
Block *nextFit(size_t size) 
{
  auto block = searchStart;
  while(block!=nullptr)
  {
    // O(n) search
    if(block->used || block->size < size)
    {
      block = block->next;
      continue;
    }
    // Found a free block
    searchStart = block;
    return block;
  }

  block = heapStart;
  while(block!=searchStart)
  {
    // O(n) search
    if(block->used || block->size < size)
    {
      block = block->next;
      continue;
    }
    // Found a free block
    searchStart = block;
    return block;
  }
  
  return nullptr;
};
 
/**
 * Best-fit algorithm.
 *
 * Returns a free block which size fits the best.
 */
Block *bestFit(size_t size) 
{
  
  Block *block = heapStart;
  Block *prev = heapStart;
  Block *best_block = nullptr;
  std::cout << "got here" << std::endl;

  while(block!=nullptr)
  {
    std::cout << "loop" << std::endl;
    // O(n) search
    std::cout << "Next block size: " << block->size << std::endl;
    if(block->used || block->size<size)
    {
      block = block->next;
      continue;
    }
    if(block->size==size)
    {
      std::cout << "found same size block in best fit" << std::endl;
      return block;
    }
    else if(best_block==nullptr || block->size<best_block->size)
    {
      std::cout << "here we go" << std::endl;
      best_block = block;
      block = block->next;
    }
  }

  if(best_block!=nullptr)
  {
    std::cout << "Best block size is: " << best_block->size << std::endl;
  }
  return best_block;
};

// Tries to find a block that fits.
Block *findBlock(size_t size) 
{
  switch (searchMode) 
  {
    case SearchMode::FirstFit:
      return firstFit(size);
    case SearchMode::NextFit:
      return nextFit(size);
    case SearchMode::BestFit:
      return bestFit(size);
  }
  return nullptr;
};

///// Block Splitting /////

// Splits the block on two, returns the pointer to the smaller sub-block.
Block *split(Block *block, size_t size) 
{
  Block* added_block = (Block*)((char *)block->data + size);
  added_block->used = false;
  added_block->next = block->next;
  std::cout << "initial block size: " << block->size << std::endl;
  added_block->size = block->size - size 
                                  - sizeof(std::declval<Block>().size)
                                  - sizeof(std::declval<Block>().used)
                                  - sizeof(std::declval<Block>().next);

  block->next = added_block;
  block->size = size;
  block->used = true;

  return block;
};

// Whether this block can be split.
inline bool canSplit(Block *block, size_t size) 
{
  std::cout << block->size << " and " << size << std::endl;
  if(block->size > size) return true;
  return false;
};

// Allocates a block from the list, splitting if needed.
Block *listAllocate(Block *block, size_t size) {

  // Split the larger block, reusing the free part.
  if (canSplit(block, size)) 
  {
    block = split(block, size);
  }
  block->used = true;
  block->size = size;
  return block;
};

// Allocates a block of memory of (at least) `size` bytes.
inline word_t *alloc(size_t size) 
{
  std::cout << "Find block of (at least) size " << size << std::endl;
  // Align that compute appropriate num bytes for user data
  size_t user_size = align(size);

  // Search for an available block
  if (auto block = findBlock(size)) 
  {
    std::cout << "Found block" << std::endl;
    listAllocate(block, size);
    return block->data;
  }

  std::cout << "need to allocate " << std::endl;
  auto block = requestFromOS(user_size);
  block->used = true;
  block->size = size;
  assert(block != nullptr);
  
  // If uninitialized set heapStart to the start 
  // of the first block that goes into unmapped data
  if(heapStart==nullptr)
  {
    std::cout << "initialize heapstart" << std::endl;
    heapStart = block;
  }

  // Make old block tail point to new block tail
  if(top!=nullptr)
  {
    top->next = block;
  }

  // Set the top to point to the newly allocated block
  top = block;
  top->next = nullptr;

  // Return user's payload
  return block->data;
};

int main()
{
  // // Test 1
  // std::cout << "Test 1" << std::endl;
  // auto p1 = alloc(3);
  // auto p1b = getHeader(p1);
  // assert(p1b->size == sizeof(word_t));

  // // Test 2
  // std::cout << "Test 2" << std::endl;
  // auto p2 = alloc(8);
  // auto p2b = getHeader(p2);
  // assert(p2b->size == 8);

  // // Test 3: Freeing an obj
  // std::cout << "Test 3" << std::endl;
  // free(p2);
  // assert(p2b->used == false);

  // // Test 4: Reuse a block
  // std::cout << "Test 4" << std::endl;
  // auto p4 = alloc(8);
  // auto p4b = getHeader(p4);
  // assert(p4b->size == 8);
  // assert(p4b == p2b);

  // // Test 5: Next search start position
  // std::cout << "Test 5" << std::endl;
  // // Init the heap, and the searching algorithm.
  // init(SearchMode::NextFit);
  // // [[8, 1], [8, 1], [8, 1]]
  // alloc(8);
  // alloc(8);
  // alloc(8);
  // // [[8, 1], [8, 1], [8, 1], [16, 1], [16, 1]]
  // auto o1 = alloc(16);
  // auto o2 = alloc(16);
  // // [[8, 1], [8, 1], [8, 1], [16, 0], [16, 0]]
  // free(o1);
  // free(o2);
  // // [[8, 1], [8, 1], [8, 1], [16, 1], [16, 0]]
  // auto o3 = alloc(16);
  // // Start position from o3:
  // assert(searchStart == getHeader(o3));
  // // [[8, 1], [8, 1], [8, 1], [16, 1], [16, 1]]
  // //                           ^ start here
  // alloc(16);

  // Test case 6: Best-fit search
  std::cout << "Test 6" << std::endl;
  init(SearchMode::BestFit);
  // [[8, 1], [64, 1], [8, 1], [16, 1]]
  alloc(8);
  auto z1 = alloc(64);
  alloc(8);
  auto z2 = alloc(16);
  // Free the last 16
  free(z2);
  assert(getHeader(z2)->used == false);
  // Free 64:
  free(z1);
  // [[8, 1], [64, 0], [8, 1], [16, 0]]
  // Reuse the last 16 block:
  auto z3 = alloc(16);
  assert(getHeader(z3) == getHeader(z2));
  // [[8, 1], [64, 0], [8, 1], [16, 1]]
  // Reuse 64, splitting it to 16, and 48
  z3 = alloc(16);
  assert(getHeader(z3) == getHeader(z1));
  // [[8, 1], [16, 1], [48, 0], [8, 1], [16, 1]]

  // Test 7
  std::cout << "Test 7" << std::endl;
  std::cout << heapStart->size << std::endl;
  std::cout << heapStart->next->size << std::endl;
  std::cout << heapStart->next->next->size << std::endl;
  assert(heapStart->next->next->size==48);

  puts("\nAll assertions passed!\n");
  return 0;
};
