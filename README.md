Ищи описание на хабре @mrlolthe1st.
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include "shared_structures.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <queue>
// Аллокатор "shared"-памяти (тут используем свой)
std::vector<void*> to_be_free;
void* shared_allocator(std::size_t size) {
	assert(size != 0);
	void* res = malloc(size);
	to_be_free.push_back(res);
	std::cout << "Allocated " << size << " bytes at " << res << "\n";
	return res;
}

struct dyn_array : can_be_shared {
	int* size;
	int* data;
	static void init_data(int& what, std::size_t idx) {
		what = idx * idx;
	}
	dyn_array(int sz) : can_be_shared({ field(&size), array(&data, sz, init_data) }) {
		*size = sz;
	}
	dyn_array(void* ptr) : from_existing(ptr), can_be_shared({ existing_field(&size), existing_array(&data) }) {}
};

struct dyn_inconsistent_matrix : can_be_shared {
	dyn_array* x;
	static void init_array(dyn_array& arr, std::size_t idx) {
		new (&arr) dyn_array(idx + 2);
	}
	dyn_inconsistent_matrix(int cnt) : can_be_shared({ array(&x, cnt, init_array) }) {}
	dyn_inconsistent_matrix(void* from) : from_existing(from), can_be_shared({ existing_array(&x) }) {}
};

int main() {
	int n = 5;
	dyn_inconsistent_matrix m(5);
	void* save_ptr = m.make_shared(shared_allocator);
	dyn_inconsistent_matrix m_ref(save_ptr);
	for (int i = 0; i < n; ++i) {
		std::cout << i << ": ";
		int orig_size = *m.x[i].size;
		int ref_size = *m_ref.x[i].size;
		std::cout << "Size: orig: " << *m.x[i].size << " ref: " << ref_size << "\n";
		if (orig_size != ref_size) {
			throw std::runtime_error("Ooops:(");
		}
		for (int j = 0; j < orig_size; ++j) {
			std::cout << "\tData: orig: " << m.x[i].data[j] << " ref: " << m_ref.x[i].data[j] << "\n";
			m.x[i].data[j] = -1 + j;
			std::cout << "\t After change: orig: " << m.x[i].data[j] << " ref: " << m_ref.x[i].data[j] << "\n";

		}
	}
	for (auto& e : to_be_free) {
		free(e);
	}
}
```
