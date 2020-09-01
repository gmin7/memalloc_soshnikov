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

/**
 * Mode for searching a free block.
 */
enum class SearchMode {
  FirstFit,
  NextFit,
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
 
// Tries to find a block that fits.
Block *findBlock(size_t size) 
{
  switch (searchMode) {
    case SearchMode::FirstFit:
      return firstFit(size);
    case SearchMode::NextFit:
      return nextFit(size);
  }
  return nullptr;
};

// Allocates a block of memory of (at least) `size` bytes.
inline word_t *alloc(size_t size) 
{
  std::cout << "Allocate block of (at least) size " << size << std::endl;
  // Align that compute appropriate num bytes for user data
  size_t user_size = align(size);

  // Search for an available block
  if (auto block = findBlock(size)) 
  {
    block->used = true;
    return block->data;
  }

  auto block = requestFromOS(user_size);
  block->size = user_size;
  block->used = true;
  
  // If uninitialized set heapStart to the start 
  // of the first block that goes into unmapped data
  if(heapStart==nullptr)
  {
    heapStart = block;
  }

  // Make old block tail point to new block tail
  if(top!=nullptr)
  {
    top->next = block;
  }

  // Set the top to point to the newly allocated block
  top = block;

  // Return user's payload
  return block->data;
};

int main()
{

  // Test 1
  auto p1 = alloc(3);
  auto p1b = getHeader(p1);
  assert(p1b->size == sizeof(word_t));

  // Test 2
  auto p2 = alloc(8);
  auto p2b = getHeader(p2);
  assert(p2b->size == 8);

  // Test 3: Freeing an obj
  free(p2);
  assert(p2b->used == false);

  // Test 4: Reuse a block
  auto p4 = alloc(8);
  auto p4b = getHeader(p4);
  assert(p4b->size == 8);
  assert(p4b == p2b);

  // --------------------------------------
  // Test case 5: Next search start position
  //
  // Init the heap, and the searching algorithm.
  init(SearchMode::NextFit);

  // [[8, 1], [8, 1], [8, 1]]
  alloc(8);
  alloc(8);
  alloc(8);

  // [[8, 1], [8, 1], [8, 1], [16, 1], [16, 1]]
  auto o1 = alloc(16);
  auto o2 = alloc(16);

  // [[8, 1], [8, 1], [8, 1], [16, 0], [16, 0]]
  free(o1);
  free(o2);
  
  // [[8, 1], [8, 1], [8, 1], [16, 1], [16, 0]]
  auto o3 = alloc(16);
  
  // Start position from o3:
  assert(searchStart == getHeader(o3));
  
  // [[8, 1], [8, 1], [8, 1], [16, 1], [16, 1]]
  //                           ^ start here
  alloc(16);

  puts("\nAll assertions passed!\n");
  return 0;
};
