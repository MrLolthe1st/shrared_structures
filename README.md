# Dynamic through static shared structures builder
Если вам надо создать какую-то структуру, которая полностью размещается на непрерывном куске памяти, доступ к которой может быть осуществлен из нескольких процессов и, при этом, эта структура *не может быть построена во время компиляции* (например, вам надо создавать таблицы с разным кол-вом строк, разной длиной строк, разным кол-вом столбцов, и тп) -- эта библиотека Вам поможет. Сразу перейдём к примерам:
```cpp
#include "shared_structures.h"
#include <iostream>
#include <fstream>

void* shared_allocator(std::size_t size) {
  void* res = malloc(size);
  std::cout << "Allocated " << size << " bytes at " << res << "\n";
  return res;
}

struct table : shared_struct {
  // Генерация нужных методов
  generate_methods(table)

  // Строим структуру
  builder_start
    add_field(x, 10)
  builder_end

  // Нужные поля
  fixed_array<int>& x;

  // Генерируем конструктор, инициализируем ссылки
  generate_constructor(table, x)
};

struct two_tables : shared_struct {
  // Генерация нужных методов
  generate_methods(two_tables)

  // Строим структуру
  builder_start
    add_field(t1)
    add_field(t2)
  builder_end

  // Нужные поля
  table& t1, & t2;
  // Генерируем конструктор, инициализируем ссылки
  generate_constructor(two_tables, t1, t2)

  ~two_tables() {
    // Удаляем t1 и t2, тк это экземпляры table, созданные через new
    delete& t1;
    delete& t2;
  }
};


int main() {
  two_tables t;
  t.t1.x[0] = 1;
  t.t2.x[1] = 1;
  for (int i = 0; i < 10; ++i) {
    std::cout << t.t1.x[i] << " " << t.t2.x[i] << "\n";
  }
  
}
```
