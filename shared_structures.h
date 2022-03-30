#ifndef SHARED_STRUCTS_
#define SHARED_STRUCTS_
#include <cstddef>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstring>

typedef void* (*memory_allocator)(std::size_t);
using flatten_result = std::pair<void*, std::size_t>;

struct field_info {
	// Указатель на указатель (который является полем структуры)
	// который необходимо проинициализировать
	void** pointer_to_pointer;
	bool is_existing;
	// Количество объектов в массиве (поле = массив длины 1)
	std::size_t count;
	// Функция, которая создаст из подконтрольного поля "плоский" кусок памяти с данными
	flatten_result(*flat_fn)(void*);
	bool is_shared;
	// Функция, которая уничтожит созданный нами на куче объект (если небходимо)
	std::size_t(*destroy_)(void*);
	// Функция, 
	void* (*init_at_)(void*, void*);
	std::size_t type_size;

	flatten_result flatten() {
		std::size_t size = sizeof(std::size_t);
		
		void* result = malloc(size);
		
		if (!result) {
			throw std::runtime_error("Out of memory");
		}
		
		*(std::size_t*)result = count;
		
		void* cur_ptr_data = *pointer_to_pointer;

		if (!is_shared) {
			void* prev_ptr = result;
			result = realloc(result, size + count * type_size);
			if (!result) {
				free(prev_ptr);
				throw std::runtime_error("Out of memory");
			}
			memcpy((void*)((size_t)result + size), cur_ptr_data, count * type_size);
			return { result, size + count * type_size };
		}

		for (std::size_t i = 0; i < count; ++i) {
			flatten_result f = flat_fn(cur_ptr_data);
			std::size_t last_offset = size;
			size += f.second;
			void* prev_ptr = result;
			result = realloc(result, size);
			if (!result) {
				free(prev_ptr);
				throw std::runtime_error("Out of memory");
			}
			memcpy((void*)((size_t)result + last_offset), f.first, f.second);
			free(f.first);
			cur_ptr_data = (void*)((std::size_t)cur_ptr_data + type_size);
		}
		return { result, size };
	}

	std::size_t destroy() {
		if (is_existing) {
			return type_size * count;
		}
		std::size_t size = 0;
		void* ptr = *pointer_to_pointer;
		for (std::size_t i = 0; i < count; ++i) {
			std::size_t sz = destroy_(ptr);
			ptr = (void*)((std::size_t)ptr + type_size);
			size += sz;
		}
		is_existing = 1;
		free(*pointer_to_pointer);
		return size;
	}

	void* init_at(void* ptr) {
		char* cur_ptr = (char*)*pointer_to_pointer;
		ptr = (void*)((std::size_t)ptr + sizeof(std::size_t));
		if (!is_shared) {
			if (!this->is_existing) {
				free(*pointer_to_pointer);
				this->is_existing = 1;
			}
			*pointer_to_pointer = ptr;
			ptr = (void*)((std::size_t)ptr + type_size * count);
			return ptr;
		}
		for (std::size_t i = 0; i < count; ++i) {
			ptr = init_at_((void*)cur_ptr, ptr);
			cur_ptr += type_size;
		}
		return ptr;
	}

	template<typename T>
	static flatten_result std_flat(T* what) {
		void* x = malloc(sizeof(T));
		memcpy(x, (void*)what, sizeof(T));
		return { x, sizeof(T) };
	}

	template<typename T>
	static std::size_t std_dest(void* what) {
		return sizeof(T);
	}

	template<typename T>
	static void* std_init(void* where, void* what) {
		return (void*)((std::size_t)what + sizeof(T));
	}

	template<typename T>
	static field_info build(T** ptr, std::size_t count, bool is_ex) {
		bool is_shared = 0;
		std::size_t (*destroy)(void*) = std_dest<T>;
		flatten_result(*flatten)(void*) = (flatten_result(*)(void*))std_flat<T>;
		void* (*init_at)(void*, void*) = std_init<T>;
		if constexpr (!std::is_fundamental<T>::value) {
			flatten = T::flatten;
			destroy = T::destroy;
			is_shared = 1;
			init_at = T::init_at;
		}
		field_info res;
		res.flat_fn = flatten;
		res.destroy_ = destroy;
		res.is_shared = is_shared;
		res.init_at_ = init_at;
		res.pointer_to_pointer = (void**)ptr;
		res.type_size = sizeof(T);
		res.is_existing = !is_ex;
		res.count = count;
		return res;
	}

};

struct from_existing {
private:
	void* ptr;

	template<typename T, typename ...Args>
	void existing_field_from_array(T* ptr) {
		new(ptr) T(this->ptr);
		this->ptr = T::init_at(ptr, this->ptr);
	}

public:
	from_existing(void* ptr = nullptr) : ptr(ptr) {};

	template<typename T, typename ...Args>
	field_info existing_field(T** ptr, bool from_array = 0) {
		if (!from_array) {
			std::size_t array_size = *(std::size_t*)this->ptr;
			if (array_size != 1)
				throw std::runtime_error("Excepted field, but got array");
			inc(sizeof(std::size_t));
		}
		if constexpr (std::is_fundamental<T>::value) {
			if (from_array) {
				return {};
			}
			*ptr = (T*)this->ptr;
			inc(sizeof(T));
			return field_info::build(ptr, 1, 0);
		}
		else {
			if (!from_array) {
				*ptr = (T*)malloc(sizeof(T));
			}
			new(*ptr) T(this->ptr);
			this->ptr = T::init_at(*ptr, this->ptr);
		}
		return field_info::build(ptr, 1, 1);
	}

	template<typename T, typename ...Args>
	field_info existing_array(T** ptr) {
		std::size_t array_size = *(std::size_t*)this->ptr;
		inc(sizeof(std::size_t));
		if constexpr (std::is_fundamental<T>::value) {
			*ptr = (T*)this->ptr;
			return field_info::build(ptr, array_size, 0);
		}
		else {
			*ptr = (T*)(malloc(sizeof(T) * array_size));
			for (std::size_t i = 0; i < array_size; ++i) {
				existing_field_from_array((T*)(*ptr + i));
			}
		}
		return field_info::build(ptr, array_size, 1);
	}

	void inc(std::size_t amount) {
		this->ptr = (void*)((std::size_t)this->ptr + amount);
	}
};

struct can_be_shared : virtual from_existing {
	// store schema to use in parent structs
	// Store pointer to pointer to init
	std::vector<field_info> pointers_to_init;
public:
	can_be_shared(std::vector<field_info> pointers_to_init) : pointers_to_init(pointers_to_init) {
	};

	static flatten_result flatten(void* th) {
		can_be_shared* obj = (can_be_shared*)th;
		void* data = nullptr;
		std::size_t size = 0;
		for (auto& e : obj->pointers_to_init) {
			flatten_result res = e.flatten();
			std::size_t prev_offset = size;
			size += res.second;
			void* prev_ptr = data;
			data = realloc(data, size);
			if (!data) {
				free(prev_ptr);
				throw new std::runtime_error("Out of memory");
			}
			void* cur_ptr = (void*)((std::size_t)data + prev_offset);
			memcpy(cur_ptr, res.first, res.second);
			free(res.first);
		}
		return { data, size };
	}

	static void* init_at(void* th, void* ptr) {
		can_be_shared* obj = (can_be_shared*)th;
		for (auto& e : obj->pointers_to_init) {
			ptr = e.init_at(ptr);
		}
		return ptr;
	}

	void* make_shared(memory_allocator shared_allocator) {
		flatten_result res = flatten((void*)this);
		void* data = shared_allocator(res.second);
		memcpy(data, res.first, res.second);
		init_at((void*)this, data);
		free(res.first);
		return data;
	}

	static std::size_t destroy(void* th) {
		std::size_t size = 0;
		can_be_shared* obj = (can_be_shared*)th;
		for (auto& e : obj->pointers_to_init) {
			if (!e.is_existing)
				size += e.destroy(), e.is_existing = 1;
		}
		obj->~can_be_shared();
		return size;
	}

	~can_be_shared() {
		for (auto& e : pointers_to_init) {
			e.destroy();
			e.is_existing = 1;
		}
	}

	template<typename T, typename ...Args>
	field_info field(T** ptr, Args&& ...args) {
		*ptr = (T*)malloc(sizeof(T));
		new(*ptr) T(args...);
		return field_info::build(ptr, 1, 1);
	}

	template<typename T, typename ...Args>
	field_info array(T** ptr, std::size_t count, void(*init_fn)(T&, std::size_t, Args&...), Args&...args) {
		*ptr = (T*)(malloc(sizeof(T) * count));
		for (std::size_t i = 0; i < count; ++i) {
			init_fn((*ptr)[i], i, args...);
		}
		return field_info::build(ptr, count, 1);
	}

private:
	can_be_shared(const can_be_shared& other) {};
	can_be_shared& operator=(const can_be_shared& other) { return *this; };
};

#endif 
