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

	static fixed_array<T>& from_struct_pointer(void* ptr) {
		return *((fixed_array<T>*)ptr);
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

	static std::pair<std::size_t, fixed_array<T>*> build(std::size_t size) {
		std::size_t bytes_to_allocate = size * sizeof(T) + sizeof(fixed_array<T>);
		fixed_array<T>* result = (fixed_array<T>*)malloc(bytes_to_allocate);
		new(result) fixed_array<T>(size);
		return { bytes_to_allocate, result };
	}

	void destroy_after_build() {
		free((void*)this);
	}

	static void copy_to(void* from, void* to) {
		auto a = (fixed_array<T>*) from;
		auto b = (fixed_array<T>*) to;
		memcpy(to, from, a->get_size());
	}

};

typedef struct struct_wrapper struct_wrapper_;

struct struct_builder {
private:
	bool is_done;
	void* data;
	std::size_t current_size;
	std::vector<std::pair<std::size_t, void(*)(void*, void*)>> fields;
public:
	struct_builder() {
		is_done = 0;
		current_size = 0;
		data = 0;
	}


	template<typename T, typename ...Args>
	struct_builder& add(Args&& ...args) {
		if (is_done) {
			throw std::runtime_error("Structure build is done, can't add other structures.");
		}

		// Build structure
		std::pair<std::size_t, T*> obj = T::build(args...);

		// Allocate more data
		if (data) {
			data = realloc(data, current_size + obj.first);
		}
		else {
			data = malloc(obj.first);
		}

		// Copy to allocated raw data
		T::copy_to((void*)obj.second, (void*)((size_t)data + current_size));

		// Increment size
		current_size += obj.first;
		fields.push_back({ obj.first, static_cast<void(*)(void*,void*)>(&(T::copy_to)) });

		// Destroy original object
		obj.second->destroy_after_build();
		return *this;	}

	struct_wrapper_& build(void* (*allocator)(std::size_t));


};

struct struct_wrapper {

	std::size_t offsets_offset;
	std::size_t offsets_count;

	std::size_t get_size() {
		return offsets_offset + offsets_count * sizeof(std::size_t);
	}

	template<typename T>
	T* get_at_offset(std::size_t offset) {
		return (T*)((std::size_t)this + offset);
	}

	inline std::size_t& get_offset(std::size_t index) {
		return *get_at_offset<std::size_t>(offsets_offset + index * sizeof(std::size_t));
	}

	void* get_struct_ptr(std::size_t idx) {
		if (idx >= offsets_count) {
			throw std::runtime_error("Can't get struct at idx=" + std::to_string(idx) + "(offsets_count=" + std::to_string(offsets_count) + ").");
		}
		return get_at_offset<void>(get_offset(idx));
	}

};

#include <iostream>

struct_wrapper& struct_builder::build(void* (*allocator)(std::size_t)) {
	std::size_t summary_size = sizeof(struct_wrapper) + sizeof(std::size_t) * fields.size() + current_size;

	struct_wrapper* sw = (struct_wrapper*)allocator(summary_size);

	sw->offsets_offset = sizeof(struct_wrapper) + current_size;
	sw->offsets_count = fields.size();

	std::size_t current_offset = 0;
	std::size_t index = 0;
	for (auto& field : fields) {
		sw->get_offset(index++) = current_offset + sizeof(struct_wrapper);
		field.second((void*)((size_t)data + current_offset), (void*)((size_t)sw + current_offset + sizeof(struct_wrapper)));
		current_offset += field.first;
	}

	free(data);
	return *sw;
}

struct shared_struct {
protected:
	struct_wrapper& wrapper;
	std::size_t counter;
public:

	std::size_t get_size() {
		return wrapper.get_size();
	}

	shared_struct(struct_wrapper& wrap) : wrapper(wrap), counter(0){

	}

};

#define get_type(field) std::remove_reference<decltype(field)>::type
#define init_field(field) field(get_type(field)::from_struct_pointer(wrapper.get_struct_ptr(counter++)))
#define add_field(field, ...) .add<std::remove_reference<decltype(field)>::type>(__VA_ARGS__)

#define generate_constructor(class_name, ...) class_name (void* ptr = nullptr) : shared_struct(build_by_ptr(ptr, shared_allocator)), init_vars(__VA_ARGS__) {}

#define generate_methods(class_name) \
static class_name& from_struct_pointer(void* x) {\
	auto res = (new class_name(x));\
	return *res;\
}\
static std::pair<std::size_t, class_name*> build() {\
	auto& res = build_by_ptr(nullptr, malloc);\
	return { res.get_size(), (class_name*)&res };\
}\
static void copy_to(void* from, void* to) {\
	auto a = (struct_wrapper*)from;\
	auto b = (struct_wrapper*)to;\
	memcpy(to, from, a->get_size());\
}\
\
void destroy_after_build() {\
	free((void*)this);\
}

#define builder_start static struct_wrapper& build_by_ptr(void* ptr, void* (*allocator)(std::size_t)) {\
if (!ptr) {\
	ptr = (void*)&struct_builder()

#define builder_end .build(allocator);\
}\
return *((struct_wrapper*)ptr);\
}


#define CHECK_N(x, n, ...) n
#define CHECK(...) CHECK_N(__VA_ARGS__, 0,)
#define PROBE(x) x, 1,
#define IS_PAREN(x) CHECK(IS_PAREN_PROBE x)
#define IS_PAREN_PROBE(...) PROBE(~)
#define COMPL(b) PRIMITIVE_CAT(COMPL_, b)
#define COMPL_0 1
#define COMPL_1 0
#define EMPTY()
#define DEFER(id) id EMPTY()
#define OBSTRUCT(...) __VA_ARGS__ DEFER(EMPTY)()
#define NOT(x) CHECK(PRIMITIVE_CAT(NOT_, x))
#define NOT_0 PROBE(~)
#define BOOL(x) COMPL(NOT(x))
#define IF(c) IIF(BOOL(c))

#define EAT(...)
#define EXPAND(...) __VA_ARGS__
#define EVAL2(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL3(...) EVAL4(EVAL4(EVAL4(__VA_ARGS__)))
#define EVAL4(...) EVAL5(EVAL5(EVAL5(__VA_ARGS__)))
#define EVAL5(...) __VA_ARGS__
#define oo(x) int(x)

#define WHILE(pred, fst, ...) \
    IF(pred(fst)) \
    ( \
		OBSTRUCT(WHILE_INDIRECT) () \
        ( \
            pred, __VA_ARGS__, init_field(fst) \
        ),  \
         __VA_ARGS__\
    )

#define IIF(c) PRIMITIVE_CAT(IIF_, c)
#define IIF_0(t, ...) __VA_ARGS__
#define IIF_1(t, ...) t

#define CAT(a, ...) PRIMITIVE_CAT(a, __VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__

#define WHILE_INDIRECT() WHILE
#define MM(x, ...) __VA_ARGS__
#define MX(x, ...) x, 1, 
#define WWW(f, ...) NOT(IS_PAREN(f))
#define init_vars(...) EVAL2(WHILE(WWW, __VA_ARGS__, ()))

#endif
