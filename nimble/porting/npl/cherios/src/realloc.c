// https://codereview.stackexchange.com/questions/151019/implementing-realloc-in-c

#include <stdlib.h>

void *my_realloc(void *ptr, size_t originalLength, size_t newLength)
{
   // Note that because we cannot rely on implementation details of the standard library,
   // we can never grow a block in place like realloc() can. However, it is semantically
   // equivalent to allocate a new block of the appropriate size, copy the original data
   // into it, and return a pointer to that new block. In fact, realloc() is allowed to
   // do this, as well. So we lose a possible performance optimization (that is rarely
   // possible in practice anyway), but correctness is still ensured, and the caller
   // never need be the wiser.
   //
   // Likewise, we cannot actually shrink a block of memory in-place, so we either
   // have to return the block unchanged (which is legal, because a block of memory
   // is always allowed to be *larger* than necessary), or allocate a new smaller
   // block, copy the portion of the original data that will fit, and return a
   // pointer to this new shrunken block. The latter would actually be slower,
   // so we'll avoid doing this extra work when possible in the current implementation.
   // (You might need to change this if memory pressure gets out of control.)

   if (newLength == 0)
   {
      free(ptr);
      return NULL;
   }
   else if (!ptr)
   {
      return malloc(newLength);
   }
   else if (newLength <= originalLength)
   {
      return ptr;
   }
   else
   {
      assert((ptr) && (newLength > originalLength));
      void *ptrNew = malloc(newLength);
      if (ptrNew)
      {
          memcpy(ptrNew, ptr, originalLength);
          free(ptr);
      }
      return ptrNew;
    }
}
