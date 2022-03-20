#ifndef SHARED_STRUCTS_
#define SHARED_STRUCTS_
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

template<typename T>
struct fixed_array {
private:
	std::size_t _size;
public:
	fixed_array(std::size_t size) {
		this->_size = size;
		for (std::size_t i = 0; i < size; ++i) {
			this->operator[](i) = T();
		}
	}

	inline std::size_t get_max_size() {
		return _size;
	}

	inline std::size_t get_size() {
		return sizeof(fixed_array<T>) + sizeof(T) * _size;
	}

	inline T& operator[](std::size_t pos) {
		return *(T*)((std::size_t)this + sizeof(fixed_array) + pos * sizeof(T));
	}
	
	static void* heap_builder_allocate(std::size_t size) {
		fixed_array<T>* result = (fixed_array<T>*)malloc(size * sizeof(T) + sizeof(fixed_array<T>));
		new(result) fixed_array<T>(size);
		return (void*)result;
	}

};

template<typename T>
struct fixed_stack{
private:
	std::size_t _top;
	fixed_array<T> _container;
public:
	fixed_stack(std::size_t size) : _container(fixed_array<T>(size)) {
		_top = 0;
	}

	void push(const T& what) {
		if (_top + 1 > _container.get_max_size()) {
			throw std::runtime_error("Can't push: stack overflow");
		}
		_container[_top++] = what;
	}

	T& top() {
		if (_top < 1) {
			throw std::runtime_error("Can't top from empty stack");
		}
		return _container[_top - 1];
	}

	void pop() {
		if (_top < 1) {
			throw std::runtime_error("Can't pop from empty stack");
		}
		--_top;
	}

	std::size_t size() {
		return _top;
	}

	bool empty() {
		return _top == 0;
	}

	std::size_t get_size() {
		return _container.get_size() + sizeof(fixed_stack<T>) - sizeof(fixed_array<T>);
	}

	static void* heap_builder_allocate(std::size_t size) {
		fixed_stack<T>* result = (fixed_stack<T>*)malloc(size * sizeof(T) + sizeof(fixed_stack<T>));
		new(result) fixed_stack<T>(size);
		return (void*)result;
	}
};

typedef struct struct_wrapper struct_wrapper_;

struct struct_builder {
private:
	bool is_done;
	std::vector<std::pair<std::size_t, void*>> fields;
public:
	struct_builder() {
		is_done = 0;
	}


	template<typename T, typename ...Args>
	struct_builder& add(Args&& ...args) {
		if (is_done) {
			throw std::runtime_error("Structure build is done, can't add other structures.");
		}
		T* obj = (T*) T::heap_builder_allocate(args...);
		fields.push_back({ obj->get_size(), obj });
		return *this;
	}

	struct_wrapper_& build(void* (*allocator)(std::size_t));


};

struct struct_wrapper {
	std::size_t offsets_offset;
	std::size_t offsets_count;

	std::size_t get_size() {
		return sizeof(struct_wrapper) + offsets_offset + offsets_count * sizeof(std::size_t);
	}

	template<typename T>
	T* get_at_offset(std::size_t offset) {
		return (T*)((std::size_t)this + sizeof(struct_wrapper) + offset);
	}

	inline std::size_t& get_offset(std::size_t index) {
		return *get_at_offset<std::size_t>(offsets_offset + index * sizeof(std::size_t));
	}

	template<typename T>
	T& get_struct(std::size_t idx) {
		if (idx >= offsets_count) {
			throw std::runtime_error("Can't get struct at idx=" + std::to_string(idx) + "(offsets_count=" + std::to_string(offsets_count) + ").");
		}
		return *get_at_offset<T>(get_offset(idx));
	}

};

struct_wrapper& struct_builder::build(void* (*allocator)(std::size_t)) {
	std::size_t summary_size = sizeof(struct_wrapper) + sizeof(std::size_t) * fields.size();
	for (auto& field : fields) {
		summary_size += field.first;
	}

	struct_wrapper* sw = (struct_wrapper*)allocator(summary_size);

	std::size_t current_offset = 0;
	for (auto& field : fields) {
		memcpy(sw->get_at_offset<void>(current_offset), field.second, field.first);
		current_offset += field.first;
		free(field.second);
	}

	sw->offsets_offset = current_offset;
	sw->offsets_count = fields.size();
	current_offset = 0;
	std::size_t index = 0;

	for (auto& field : fields) {
		sw->get_offset(index++) = current_offset;
		current_offset += field.first;
	}

	return *sw;
}

struct shared_struct {
protected:
	struct_wrapper& wrapper;
public:
	shared_struct(struct_wrapper& wrap) : wrapper(wrap) {

	}
	void* get_struct_address() {
		return &wrapper;
	}

	std::size_t get_struct_size() {
		return wrapper.get_size();
	}
};

#define init_field(field) field(wrapper.get_struct<std::remove_reference<decltype(field)>::type>(counter++))
#define add_field(field, ...) add<std::remove_reference<decltype(field)>::type>(__VA_ARGS__)

#endif