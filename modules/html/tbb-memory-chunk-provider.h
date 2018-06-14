#pragma once

namespace caspar { namespace html {

//
// INTERNAL IMPLEMENTATION
// This class is not intended to be used in normal code
//


//
// MEMORY CHUNK PROVIDER
//
//

template <typename Tvalue_type>
class tbbMemoryChunkProvider {
public:
	typedef Tvalue_type value_type;

	__forceinline void *allocate(size_t pages) {
		void *ptr = NULL;
		size_t sz = pages * sizeof(value_type);

		ptr = VirtualAlloc(NULL, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		return ptr;
	}//


	__forceinline void deallocate(void *ptr, size_t pages) {

		VirtualFree(ptr, 0, MEM_RELEASE);
	}//

};


} }
