# Dynamic through static shared structures builder
Если вам надо создать какую-то структуру, которая полностью размещается на непрерывном куске памяти, доступ к которой может быть осуществлен из нескольких процессов и, при этом, эта структура *не может быть построена во время компиляции* (например, вам надо создавать таблицы с разным кол-вом строк, разной длиной строк, разным кол-вом столбцов, и тп) -- эта библиотека Вам поможет. Сразу перейдём к примерам:
```cpp
#include "shared_structures.h"
#include <iostream>
#include <fstream>

typedef fixed_stack<int> avail_rows;

// Ваша shared-структура
struct table : shared_struct {
	std::size_t counter = 0;

	struct_wrapper& build_by_address(void* addr) {
		// Если передали адрес для создания - берем готовую структуру
		if (!addr) {
			// иначе - создаём структуру, в памяти, которую выделил переданный аллокатор (в данном случае malloc)
			addr = (void*)&struct_builder().add_field(free_rows, 100).build(malloc);
			// ниже в документации будет пример с shared-сегментами из linux'а
		}
		return *((struct_wrapper*)addr);
	}
public:
	//avail_rows& free_rows;
	avail_rows& free_rows;

	// хотели хранить в нашей структуре free_rows, берем их из нашего wrapper'а
	table(void* addr = nullptr) : counter(0), shared_struct(build_by_address(addr)), init_field(free_rows) {

	}

};
int main() {
	table t;
	for (int i = 0; i < 100; ++i)
		t.free_rows.push(i * i);
	while (t.free_rows.size() > 50) {
		std::cout << t.free_rows.top() << " ";
		t.free_rows.pop();
	}
	// Запишем структуру на диск
	std::ofstream file("table", std::ios::binary);
	file.write((const char*)t.get_struct_address(), t.get_struct_size());
	free(t.get_struct_address());
	file.flush();
	file.close();

	// Считаем её
	std::ifstream file_in("table", std::ios::binary);
	char buffer[600] = {};
	file_in.read((char*)&buffer, 600);
	// P.S. auto because broken markdown
	auto t2 = table((void*)buffer);
	// Всё еще работает!
	while (!t2.free_rows.empty()) {
		std::cout << t2.free_rows.top() << " ";
		t2.free_rows.pop();
	}
}
```