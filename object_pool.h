#pragma once

#include "pch.h"
#include "framework.h"

#ifndef __P_OBJECT_POOL__
#define __P_OBJECT_POOL__

#include <new>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

template<typename object_type>
class object_class;

template<typename object_type>
class object_ptr;

template<typename object_type>
class object_list_ptr;

template<typename object_type>
class limited_object_pool;

template<typename object_type>
class object_pool;

template<typename object_type>
class object_class
{
	friend class object_ptr<object_type>;
	friend class limited_object_pool<object_type>;
	friend class object_pool<object_type>;

private:
	object_type* pobj;
	size_t ref_cnt;
	bool deleted;

#ifdef _DEBUG
public:
#else
private:
#endif

	object_type* get_object() const
	{
		if (is_deleted()) throw(runtime_error("null pointer"));
		return pobj;
	}

	void create_object()
	{
		pobj = new(pobj) object_type;
		deleted = false;
	}

	template<typename... Args>
	void create_object(Args&& ... args)
	{
		pobj = new(pobj) object_type(std::forward<Args>(args)...);
		deleted = false;
	}

	void add_ref()
	{
		ref_cnt++;
	}

	void remove_ref()
	{
		if (ref_cnt > 0)ref_cnt--;
	}

	void transfer(void* pnew_addr)
	{
		pobj = (object_type*)pnew_addr;
	}

	bool is_deleted() const
	{
		return deleted;
	}

	void delete_object()
	{
		if (ref_cnt == 0 && !deleted)
		{
			deleted = true;
			if (is_class<object_type>::value)
			{
				pobj->~object_type();
			}
			pobj = nullptr;
		}
	}

	void force_delete()
	{
		ref_cnt = 0;
		delete_object();
	}

public:
	object_class() = delete;

	object_class(object_type* _pobj)
	{
		pobj = _pobj;
		ref_cnt = 0;
		deleted = true;
	}

	object_class& operator=(const object_class&) = delete;

	~object_class()
	{
		force_delete();
	}

};

template<typename object_type>
class object_ptr
{
private:
	typedef object_class<object_type> obj_class;
	typedef object_ptr<object_type> obj_ptr;

private:
	obj_class* pobj_class;
	bool valid;

private:
	void copy_init(const obj_ptr& src)
	{
		auto last_pobj_class = pobj_class;
		pobj_class = src.pobj_class;
		valid = src.valid;
		pobj_class->add_ref();
		if (last_pobj_class != nullptr)
			last_pobj_class->remove_ref();
	}
	void assert_valid() const
	{
		if (!valid)
			throw runtime_error("Invalid pointer!");
	}

public:
	object_type& operator*() const
	{
		assert_valid();
		return (*(pobj_class->get_object()));
	}

	object_type* operator->() const
	{
		assert_valid();
		return pobj_class->get_object();
	}

	obj_ptr& operator=(const obj_ptr& src)
	{
		copy_init(src);
		return *this;
	}

	object_ptr(const obj_ptr& src)
	{
		pobj_class = nullptr;
		copy_init(src);
	}

	object_ptr(obj_class* _pclass)
	{
		pobj_class = _pclass;
		pobj_class->create_object();
		pobj_class->add_ref();
		valid = true;
	}

	template<typename... Args>
	object_ptr(obj_class* _pclass, Args&&... args)
	{
		pobj_class = _pclass;
		pobj_class->create_object(std::forward<Args>(args)...);
		pobj_class->add_ref();
		valid = true;
	}

	~object_ptr()
	{
		if (valid)
		{
			destroy();
		}
	}

	bool is_valid() const
	{
		return valid;
	}

	void destroy()
	{
		if (valid)
		{
			pobj_class->remove_ref();
			pobj_class->delete_object();
			pobj_class = nullptr;
			valid = false;
		}
	}

	object_type* unsafe_ptr() const
	{
		return pobj_class->get_object();
	}
	
};

template<typename object_type>
class object_list_ptr
{
	friend class object_pool<object_type>;
	friend class limited_object_pool<object_type>;
private:
	typedef object_class<object_type>  obj_class;
	typedef object_ptr<object_type>  obj_ptr;
	typedef object_list_ptr<object_type> obj_list_ptr;

private:
	size_t __size;
	size_t __end;
	obj_ptr* ptr_list;

private:
	void copy_init(const obj_list_ptr& src)
	{
		__size = src.__size;
		__end = src.__end;
		auto last_list = ptr_list;
		ptr_list = (obj_ptr*)malloc(__size * sizeof(obj_ptr));
		if (!ptr_list) throw bad_alloc();
		for (int i = 0; i < __end; i++)
		{
			new(ptr_list + i) obj_ptr(src.ptr_list[i]);
		}
		for (int i = 0; i < __end; i++)
		{
			if (last_list == nullptr) break;
			last_list[i].~obj_ptr();
			free(last_list);
		}
	}

	template<typename... Args>
	void add_obj(obj_class* _pclass, Args&&... args)
	{
		if (__end >= __size) throw overflow_error("Too many objects!");
		new(ptr_list + __end) object_ptr(_pclass, std::forward<Args&&>(args)...);
		__end++;
	}

public:
	object_list_ptr& operator=(const obj_list_ptr& src)
	{
		copy_init(src);
		return *this;
	}

	object_type& operator[](size_t index) const
	{
		if (index >= __size) throw out_of_range("Index out of range!");
		return *(ptr_list[index]);
	}

	obj_ptr get_ptr(size_t index) const
	{
		return ptr_list[index];
	}

	size_t size() const
	{
		return __size;
	}

	object_list_ptr(size_t _size) :__size(_size), __end(0)
	{
		ptr_list = (obj_ptr*)malloc(_size * sizeof(obj_ptr));
		if (!ptr_list) throw bad_alloc();
	}

	object_list_ptr(const obj_list_ptr& src)
	{
		copy_init(src);
	}
	
	~object_list_ptr()
	{
		for (int i = 0; i < __end; i++)
		{
			ptr_list[i].~obj_ptr();
		}
		free(ptr_list);
	}

};

template<typename object_type>
class object_pool
{
private:
	typedef object_pool<object_type> obj_pool;
	typedef object_ptr<object_type> obj_ptr;
	typedef object_list_ptr<object_type> obj_list_ptr;
	typedef object_class<object_type> obj_class;

private:
	struct object_node
	{
		obj_class this_obj;
		object_node* next;

		object_node(object_type* pobj) :this_obj(pobj), next(nullptr) {}
	};

private:
	object_node* class_head;
	object_node* last_class;
	void* memory_pool;
	size_t current_capacity;
	size_t used;

public:
	const static size_t object_size = sizeof(object_type);
	const static size_t default_pool_size = 50;

private:
	void add_new_class(object_type* pobj)
	{
		if (class_head == nullptr)
		{
			class_head = new object_node(pobj);
			last_class = class_head;
		}
		else
		{
			last_class->next = new object_node(pobj);
			last_class = last_class->next;
		}	
	}

	void gc()
	{
		if (class_head == nullptr) return;
		object_node* pcur = class_head , * pnext;
		
		while (true)
		{
			pnext = pcur->next;
			if (pnext == nullptr) break;
			if (pnext->this_obj.is_deleted())
			{
				pcur->next = pnext->next;
				delete pnext;
			}
			else
				pcur = pnext;
		}

		if (class_head->this_obj.is_deleted())
		{
			if (pcur == class_head)
			{
				delete class_head;
				class_head = nullptr;
				last_class = nullptr;
				used = 0;
				return;
			}
			pnext = class_head->next;
			delete class_head;
			class_head = pnext;
		}
		
		last_class = pcur;
		
		int cnt = 0;
		for (object_node* pobj = class_head; pobj; pobj = pobj->next, cnt++)
		{
			if ((object_type*)memory_pool + cnt == pobj->this_obj.get_object())
				break;
			new((object_type*)memory_pool + cnt) object_type(*(pobj->this_obj.get_object()));
			pobj->this_obj.get_object()->~object_type();
		}
		used = cnt;
	}

	void* get_first_available() const
	{
		if (used >= current_capacity) return nullptr;
		return (object_type*)memory_pool + used;
	}

	bool create_objects(size_t num)
	{
		void* pstart = get_first_available();
		if (!pstart || num == 0) return false;
		if (used + num > current_capacity) return false;
		for (int i = 0; i < num; i++)
		{
			add_new_class(new((object_type*)pstart + i) object_type);
		}
		used += num;
		return true;
	}

	void extend()
	{
		void* pnew = malloc(2 * current_capacity * object_size);
		if (pnew == NULL) throw bad_alloc();
		for (int i = 0; i < used; i++)
		{
			new((object_type*)pnew + i) object_type( * ((object_type*)memory_pool + i));
			((object_type*)memory_pool + i)->~object_type();
		}
		free(memory_pool);
		memory_pool = pnew;
		current_capacity *= 2;

		object_node* pobj = class_head;
		for (int i = 0; pobj != nullptr; pobj = pobj->next, i++)
		{
			pobj->this_obj.transfer((object_type*)memory_pool + i);
		}
	}
public:
	object_pool(size_t init_pool_size = default_pool_size) :class_head(nullptr), last_class(nullptr), current_capacity(init_pool_size), used(0)
	{
		memory_pool = malloc(init_pool_size * object_size);
		if (memory_pool == NULL) throw bad_alloc();
	}

	object_pool(const obj_pool&) = delete;

	object_pool& operator=(const obj_pool&) = delete;

	~object_pool()
	{
		object_node* pcur = class_head, *pnext;
		while (pcur != nullptr)
		{
			pnext = pcur->next;
			delete pcur;
			pcur = pnext;
		}

		free(memory_pool);
	}

	template<typename... Args>
	obj_ptr get_object(Args&& ... args)
	{
		bool succeeded = false;
		succeeded = create_objects(1);
		if (succeeded)
			return obj_ptr(&(last_class->this_obj), std::forward<Args>(args)...);
		gc();
		succeeded = create_objects(1);
		if (succeeded)
			return obj_ptr(&(last_class->this_obj), std::forward<Args>(args)...);
		while (!succeeded)
		{
			extend();
			succeeded = create_objects(1);
		}
		return obj_ptr(&(last_class->this_obj), std::forward<Args>(args)...);
	}

	template<typename... Args>
	obj_list_ptr get_object_list(size_t _count, Args&& ... args)
	{
		if (_count == 0) throw runtime_error("Count cannot be 0!");
		auto pre_class = last_class;
		bool succeeded = false;
		succeeded = create_objects(_count);
		if (!succeeded)
			gc();
		pre_class = last_class;
		succeeded = create_objects(_count);
		while (!succeeded)
		{
			extend();
			pre_class = last_class;
			succeeded = create_objects(_count);
		}
		;
		obj_list_ptr plist(_count);
		auto pcur = pre_class->next;
		for (int i = 0; i < _count; i++)
		{
			plist.add_obj(&(last_class->this_obj), std::forward<Args>(args)...);
			pcur = pcur->next;
		}

		return plist;
	}
	
	bool shrink(bool forced = false)
	{
		gc();
		if (!forced && used * 2 > current_capacity) 
			return false;
		void* pnew = malloc(used * object_size);
		if (pnew == NULL) throw bad_alloc();
		for (int i = 0; i < used; i++)
		{
			new((object_type*)pnew + i) object_type(*((object_type*)memory_pool + i));
			((object_type*)memory_pool + i)->~object_type();
		}
		free(memory_pool);
		memory_pool = pnew;
		current_capacity = used;

		object_node* pobj = class_head;
		for (int i = 0; pobj != nullptr; pobj = pobj->next, i++)
		{
			pobj->this_obj.transfer((object_type*)memory_pool + i);
		}
	}

	size_t used_cnt() const
	{
		return used;
	}

	size_t capacity() const
	{
		return current_capacity;
	}
};

template<typename object_type>
class limited_object_pool
{
private:
	typedef limited_object_pool<object_type> obj_pool;
	typedef object_ptr<object_type> obj_ptr;
	typedef object_list_ptr<object_type> obj_list_ptr;
	typedef object_class<object_type> obj_class;

private:
	struct object_node
	{
		obj_class this_obj;
		object_node* next;

		object_node(object_type* pobj) :this_obj(pobj), next(nullptr) {}
	};

private:
	object_node* class_head;
	object_node* last_class;
	void* memory_pool;
	size_t current_capacity;
	size_t used;

public:
	const static size_t object_size = sizeof(object_type);
	const static size_t default_pool_size = 50;

private:
	void add_new_class(object_type* pobj)
	{
		if (class_head == nullptr)
		{
			class_head = new object_node(pobj);
			last_class = class_head;
		}
		else
		{
			last_class->next = new object_node(pobj);
			last_class = last_class->next;
		}
	}



	void* get_first_available() const
	{
		if (used >= current_capacity) return nullptr;
		return (object_type*)memory_pool + used;
	}

	bool create_objects(size_t num)
	{
		void* pstart = get_first_available();
		if (!pstart || num == 0) return false;
		if (used + num > current_capacity) return false;
		for (int i = 0; i < num; i++)
		{
			add_new_class(new((object_type*)pstart + i) object_type);
		}
		used += num;
		return true;
	}

public:
	limited_object_pool(size_t init_pool_size = default_pool_size) :class_head(nullptr), last_class(nullptr), current_capacity(init_pool_size), used(0)
	{
		memory_pool = malloc(init_pool_size * object_size);
		if (memory_pool == NULL) throw bad_alloc();
	}

	limited_object_pool(const limited_object_pool&) = delete;

	limited_object_pool& operator=(const limited_object_pool&) = delete;

	~limited_object_pool()
	{
		object_node* pcur = class_head, * pnext;
		while (pcur != nullptr)
		{
			pnext = pcur->next;
			delete pcur;
			pcur = pnext;
		}

		free(memory_pool);
	}

	template<typename... Args>
	obj_ptr get_object(Args&& ... args)
	{
		if (!create_objects(1))
			throw overflow_error("No enough capacity!");
		return obj_ptr(&(last_class->this_obj), std::forward<Args>(args)...);
	}

	template<typename... Args>
	obj_list_ptr get_object_list(size_t _count, Args&& ... args)
	{
		if (_count == 0) throw runtime_error("Count cannot be 0!");
		auto pre_class = last_class;
		if (!create_objects(_count))
			throw overflow_error("No enough capacity!");
		;
		obj_list_ptr plist(_count);
		auto pcur = pre_class->next;
		for (int i = 0; i < _count; i++)
		{
			plist.add_obj(&(last_class->this_obj), std::forward<Args>(args)...);
			pcur = pcur->next;
		}

		return plist;
	}

	size_t used_cnt() const
	{
		return used;
	}

	size_t capacity() const
	{
		return current_capacity;
	}
};

#endif // !__P_OBJECT_POOL__