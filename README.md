# jsmalloc

A malloc implementation, just for fun. The benchmarking framework here was developed by my friend Clayton Knittel so our friends could each program our own memory allocators (https://github.com/ClaytonKnittel/malloc-bench).

# Allocator overview

There are two different sizes of allocations that are treated differently: small allocations (≤ 256 bytes) and large allocations (> 256 bytes).

### Small size allocator (<= 256 bytes)

Any `malloc(size)` call with `size ≤ 256` bytes is treated specially.

Small sizes are stored stores in fixed size "bins" packed within a _SmallBlock_. A small block looks like:

```
SmallBlock (4096 bytes)
 ┌───────────────────┐
 │  Data bin size    │  <- Each block handles one size class.
 │     2 bytes       │
 ├───────────────────┤
 │  Data bin count   │  <- Store the number of bins in the block.
 │      2 bytes      │     Facilitates fast "is the block full?" checks.
 ├───────────────────┤
 │  LinkedList node  │  <- Free blocks are stored in a dequeue.
 │     16 bytes      │     This is a node in the intrusive datastructure.
 ├───────────────────┤
 │   Used bin count  │  <- The number of data bins used.
 │      2 bytes      │     Facilitates fast "is the block full?" checks.
 ├───────────────────┤
 │  Used bins bitset │  <- Which data bins are used?
 │   variable size   │
 ├───────────────────┤
 │     Data bin      │  <- Actual memory handed back to user.
 ├───────────────────┤
 │     Data bin      │
 ├───────────────────┤
 │       ...         │
 └───────────────────┘
```

Each SmallBlock handles allocations of a fixed size (its "size class"). There is a SmallBlock kind for every 16-byte increment up to 256 (8, 16, 32, 48, ..., 240, 256). For example, `SmallBlock` for 8-byte data bins will contain 498 bins (the other 112 bytes is used for bookkeeping).

Small blocks themselves are stored in a larger `mmap`'d region of memory:

```
┌───────────────────────────┐
│Dequeues of non-full blocks│ <- Small blocks with some free space available are
│   (one per size class)    │    kept track of here (one dequeue per size class).
├───────────────────────────┤
│         Stack of          │ <- Blocks that are _completely_ empty are recorded
│       empty blocks        │    in a stack to facilitate reallocation.
├───────────────────────────┤
│        SmallBlock         │
│       (4096 bytes)        │
├───────────────────────────┤
│        SmallBlock         │
│       (4096 bytes)        │
├───────────────────────────┼
│           ...             │
└───────────────────────────┘
```

#### Allocation logic

In the fast-path, allocation looks like:

1. **Find a suitable SmallBlock**: Pick a `SmallBlock` from the dequeue of the requested size class.
1. **Allocate a bin from the block**: Find and set the first unset bit in the used-bins bitset of the `SmallBlock`; increment used-bin count. Return the bin's address.

There are two slowdowns:
* If there isn't a `SmallBlock` with any free data bins, the stack of unused blocks must be updated to pop a free block.
* If, after allocation, the `SmallBlock` becomes full, the block must be removed from its dequeue of non-full blocks.

#### Deallocation logic

Because `SmallBlock`s are always page-aligned, freeing a small allocation is very fast:

1. **Identify the SmallBlock**: Truncate the address to the page boundary to obtain the start of the `SmallBlock`.
1. **Mark the bin as free**: Clear the corresponding bit in the used-bins bitset and decrement the used-bin count.

Similarly to allocation, there is a slowdown if the block becomes empty, since its free block stacks and linked list entries must be updated. Fortunately, since all blocks are exactly 4096 bytes, coalescing free blocks is never necessary.

### Large size allocator (> 256 bytes)

Similar to small allocation sizes, large allocations reside in a _LargeBlock_. Unlike with `SmallBlock`s, each allocation receives its own `LargeBlock`:

```
       LargeBlock        
┌───────────────────────┐
│  Previous block free? │  <- Whether the previous block in contiguous memory is free.
│        1 bit          │     Enables coalescing neighboring free blocks.
├───────────────────────┤
│         Kind          │  <- Some metadata keeping track of whether this is the
│        3 bits         │     first or last block on the heap.
├───────────────────────┤
│         Size          │  <- The size of this block.
│        28 bits        │
├───────────────────────┤
│       Alignment       │  <- Variable length empty space for alignment.
│     Variable size     │
├───────────────────────┤
│   Data offset length  │  <- Stores the number of bytes to jump back from this address.
│        4 bytes        │     Used during `free` to locate the block's start.
├───────────────────────┤
│       User data       │  <- Actual memory handed back to the user.
│     Variable size     │
└───────────────────────┘
```

When a LargeBlock is freed, its memory layout looks like:

```
     Free LargeBlock        
┌───────────────────────┐
│  Previous block free? │  <- Whether the previous block in contiguous memory is free.
│        1 bit          │     Enables coalescing neighboring free blocks.
├───────────────────────┤
│         Kind          │  <- Some metadata keeping track of whether this is the
│        3 bits         │     first or last block on the heap.
├───────────────────────┤
│         Size          │  <- The size of this block.
│        28 bits        │
├───────────────────────┤
│     Dequeue node      │  <- Node in an intrusive linked list. 
│       16 bytes        │     Allows faster lookup of free blocks chosen for a deque.
├───────────────────────┤
│  Red Black tree node  │  <- Node in an intrusive RB tree. 
│       32 bytes        │     Allows quick-ish lookup of other free blocks by size.
├───────────────────────┤
│      Empty space      │  <- Padding extends until the end of the block.
├───────────────────────┤
│         Size          │  <- Size allows the next block coalesce with this block.
│        4 bytes        │
└───────────────────────┘
```

Generally, free blocks are stored in a RB tree keyed on the block's size.

However, for sizes between 256 bytes and 8096 bytes, there's an additional optimization to skip the RB tree, since traversing all the links in the tree can be pretty slow. For these sizes, there is a set of ~500 linked lists of `FreeBlock`s. Each list contains blocks of exactly the same size. Each list is tracked in a bitset that indicates if the list is empty. The bitset enables a fast lookup of which linked list has `FreeBlock`s available via `popcount`. If there is a `FreeBlock` with space, the system just pops that block and allocates it. Otherwise, it falls back to the RB tree. If there's no free block, it falls back to allocating new memory.

```
         Linked lists for sizes >256 and <=8096         
┌─────────────────────────┐   ┌─────────┐    ┌─────────┐
│FreeBlock LL of size 272 ├──►│FreeBlock├───►│FreeBlock│
├─────────────────────────┤   ├─────────┤    └─────────┘
│FreeBlock LL of size 288 ├──►│FreeBlock│               
├─────────────────────────┤   └─────────┘               
│           ...           │                             
├─────────────────────────┤                             
│FreeBlock LL of size 8096│                             
└─────────────────────────┘                             
┌──────────────────────────────────────────────────────┐
│                BitSet (512 bits)                     │
│Indicates which linked lists above are non-empty,     │
│allowing for fast popcnt check for finding free blocks│
└──────────────────────────────────────────────────────┘
                                                        
             Red Black Tree for sizes >8096             
                      ┌─────────┐                       
                      │FreeBlock│                       
                    ┌─┴─────────┴─┐                     
                    ▼             ▼                     
                ┌─────────┐  ┌─────────┐                
                │FreeBlock│  │FreeBlock│                
                └─┬────┬──┘  └─┬────┬──┘                
                  ▼    ▼       ▼    ▼                   
                 ...  ...     ...  ...                  
```

## Cookbook

### Test jsmalloc live with `LD_PRELOAD`

To test jsmalloc in your own binaries, run:

```sh
bazel build --config=opt //src:liballoc.so
LD_PRELOAD=./bazel-bin/src/liballoc.so your_binary
```

Doesn't work with apps that bring their own malloc implementation (e.g. Firefox, Chrome).

### Run benchmarks

```sh
bazel run --config=opt //src:driver
```

### Configure intellisense

To get tooling like clangd to play nicely with Bazel, run:

```sh
bazel run //:refresh_compile_commands
```

See https://github.com/hedronvision/bazel-compile-commands-extractor.

## Benchmarks

### jsmalloc results (no multithreading):

```
---------------------------------------------------------------------------
| trace                           | correct? | mega ops / s | utilization |
---------------------------------------------------------------------------
|*traces/bdd-aa32.trace           |        Y |        110.1 |       82.3% |
|*traces/bdd-aa4.trace            |        Y |        113.2 |       64.5% |
|*traces/bdd-ma4.trace            |        Y |        116.4 |       81.3% |
|*traces/bdd-nq7.trace            |        Y |        114.0 |       83.1% |
|*traces/cbit-abs.trace           |        Y |        103.4 |       71.8% |
|*traces/cbit-parity.trace        |        Y |         94.8 |       73.6% |
|*traces/cbit-satadd.trace        |        Y |         97.4 |       82.7% |
|*traces/cbit-xyz.trace           |        Y |        101.2 |       77.9% |
| traces/clang.trace              |        Y |         87.8 |       94.3% |
| traces/firefox.trace            |        Y |         50.4 |       93.8% |
| traces/four-in-a-row.trace      |        Y |         84.7 |       91.1% |
| traces/grep.trace               |        Y |         62.4 |       71.0% |
| traces/haskell-web-server.trace |        Y |         63.4 |       99.0% |
| traces/mc_server.trace          |        Y |         39.5 |       83.6% |
| traces/mc_server_large.trace    |        Y |         34.3 |       76.7% |
| traces/mc_server_small.trace    |        Y |         79.1 |       92.0% |
|*traces/ngram-fox1.trace         |        Y |        101.7 |       39.0% |
|*traces/ngram-gulliver1.trace    |        Y |        106.3 |       45.2% |
|*traces/ngram-gulliver2.trace    |        Y |         96.6 |       44.8% |
|*traces/ngram-moby1.trace        |        Y |        103.0 |       43.6% |
|*traces/ngram-shake1.trace       |        Y |         99.8 |       43.2% |
| traces/onoro-cc.trace           |        Y |         75.3 |       74.9% |
| traces/onoro.trace              |        Y |         21.9 |       87.6% |
| traces/py-catan-ai.trace        |        Y |         33.8 |       94.6% |
| traces/py-euler-nayuki.trace    |        Y |         43.7 |       88.3% |
| traces/scp.trace                |        Y |        100.0 |       85.9% |
|*traces/server.trace             |        Y |        100.9 |       62.5% |
|*traces/simple.trace             |        Y |         95.3 |       36.8% |
|*traces/simple_calloc.trace      |        Y |        123.6 |       75.9% |
|*traces/simple_realloc.trace     |        Y |         82.5 |       90.1% |
| traces/solitaire.trace          |        Y |         83.9 |       92.3% |
| traces/ssh.trace                |        Y |        106.9 |       82.0% |
|*traces/syn-array-short.trace    |        Y |         78.6 |       76.6% |
|*traces/syn-array.trace          |        Y |         32.4 |       96.3% |
|*traces/syn-mix-realloc.trace    |        Y |         21.0 |       72.3% |
|*traces/syn-mix-short.trace      |        Y |         91.5 |       47.9% |
|*traces/syn-mix.trace            |        Y |         56.6 |       93.2% |
|*traces/syn-string-short.trace   |        Y |         98.1 |       20.7% |
|*traces/syn-string.trace         |        Y |         92.7 |       86.3% |
|*traces/syn-struct-short.trace   |        Y |         98.5 |       20.4% |
|*traces/syn-struct.trace         |        Y |         95.2 |       89.1% |
|*traces/test-zero.trace          |        Y |         92.3 |       71.2% |
|*traces/test.trace               |        Y |         80.2 |       76.8% |
| traces/vim.trace                |        Y |         77.7 |       88.3% |
| traces/vlc.trace                |        Y |         77.9 |       95.3% |
---------------------------------------------------------------------------
* = ignored for scoring

Summary:
All correct? Y
Average utilization: 87.7%
Average mega ops / s: 60.8
Score: 72.8%
```

### glibc malloc results:

### TCMalloc results:

### jemalloc results:
