#pragma once

#include "pch.h"
#include "framework.h"

#ifndef __P_OBJECT_POOL__
#define __P_OBJECT_POOL__

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
class object_pool;

template<typename object_type>
class object_class
{

protected:
	object_type* pobj;
	size_t ref_cnt;
	bool deleted;


public:
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

	void add_ptr()
	{
		ref_cnt++;
	}

	void remove_ptr()
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

	object_class() = delete;

	object_class(object_type* _pobj)
	{
		pobj = _pobj;
		ref_cnt = 0;
		deleted = true;
	}

	~object_class()
	{
		force_delete();
	}

};

template<typename object_type>
class object_ptr
{
protected:
	typedef object_class<object_type> obj_class;
	typedef object_ptr<object_type> obj_ptr;
protected:
	obj_class* pobj_class;
	bool valid;

public:
	void check_valid() const
	{
		if (!valid)
			throw runtime_error("Invalid pointer!");
	}
	object_type& operator*() const
	{
		check_valid();
		return (*(pobj_class->get_object()));
	}

	object_type* operator->() const
	{
		check_valid();
		return pobj_class->get_object();
	}

	obj_ptr& operator=(const obj_ptr& src)
	{
		auto last_pobj_class = pobj_class;
		pobj_class = src.pobj_class;
		valid = src.valid;
		pobj_class->add_ptr();
		if (last_pobj_class != nullptr)
			last_pobj_class->remove_ptr();
	}

	object_ptr(const obj_ptr& src)
	{
		pobj_class = nullptr;
		(*this) = src;
	}

	template<typename... Args>
	object_ptr(obj_class* _pclass, Args&& ... args)
	{
		pobj_class = _pclass;
		pobj_class->create_object(std::forward<Args>(args)...);
		pobj_class->add_ptr();
		valid = true;
	}

	virtual ~object_ptr()
	{
		pobj_class->remove_ptr();
		pobj_class->delete_object();
	}

	void destroy()
	{
		pobj_class->delete_object();
		pobj_class = nullptr;
		valid = false;
	}

	object_type* unsafe_ptr() const
	{
		return pobj_class->get_object();
	}
	
};

template<typename object_type>
class object_list_ptr
{
protected:
	typedef object_class<object_type>  obj_class;
	typedef object_ptr<object_type>  obj_ptr;
	typedef object_list_ptr<object_type> obj_list_ptr;

protected:
	size_t __size;
	size_t __end;
	obj_ptr* ptr_list;

protected:
	template<typename... Args>
	void add_obj(obj_class* _pclass, Args&& ... args)
	{
		if (__end >= size) throw overflow_error("Too many objects!");
		new(ptr_list + __end) obj_ptr(_pclass, std::forward<Args>(args)...);
		end++;
	}

public:
	
	object_list_ptr& operator=(const obj_list_ptr& src)
	{
		__size = src.__size;
		__end = src.__end;
		auto last_list = ptr_list;
		ptr_list = malloc(_size * sizeof(obj_ptr));
		if (!ptr_list) throw bad_alloc();
		for (int i = 0; i < __end; i++)
		{
			new(ptr_list + i) obj_ptr(src.ptr_list[i]);
		}
		for (int i = 0; i < __end; i++)
		{
			last_list[i].~obj_ptr();
			free(last_list);
		}
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
		ptr_list = malloc(_size * sizeof(obj_ptr));
		if (!ptr_list) throw bad_alloc();
	}

	object_list_ptr(const obj_list_ptr& src)
	{
		*this = src;
	}
	
	virtual ~object_list_ptr()
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
protected:
	typedef object_pool<object_type> obj_pool;
	typedef object_ptr<object_type> obj_ptr;
	typedef object_list_ptr<object_type> obj_list_ptr;
	typedef object_class<object_type> obj_class;

protected:
	struct object_node
	{
		obj_class this_obj;
		object_node* next;

		object_node(object_type* pobj) :this_obj(pobj), next(nullptr) {}
	};

protected:
	object_node* class_head;
	object_node* last_class;
	void* memory_pool;
	size_t current_capacity;
	size_t used;

public:
	const static size_t object_size = sizeof(object_type);
	const static size_t default_pool_size = 50;

protected:
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
		object_node* pcur = class_head, * pnext;
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
		last_class = pcur;
		
		int cnt = 0;
		for (object_node* pobj = class_head; pobj != nullptr; pobj = pobj->next, cnt++)
		{
			memcpy((object_type*)memory_pool + cnt, pobj->this_obj.get_object(), 1 * object_size);
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
		memcpy(pnew, memory_pool, used * object_size);
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
	obj_list_ptr get_object_list(size_t count, Args&& ... args)
	{
		if (count == 0) throw runtime_error("Count cannot be 0!");
		auto pre_class = last_class;
		bool succeeded = false;
		succeeded = create_objects(count);
		if (!succeeded)
			gc();
		succeeded = create_objects(count);
		while (!succeeded)
		{
			extend();
			succeeded = create_objects(count);
		}

		obj_list_ptr plist(count);
		auto pcur = pre_class->next;
		for (int i = 0; i < count; i++)
		{
			plist.add_obj(obj_ptr(&(last_class->this_obj), std::forward<Args>(args)...));
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
		memcpy(pnew, memory_pool, used * object_size);
		free(memory_pool);
		memory_pool = pnew;
		current_capacity = used;

		object_node* pobj = class_head;
		for (int i = 0; pobj != nullptr; pobj = pobj->next, i++)
		{
			pobj->this_obj.transfer((object_type*)memory_pool + i);
		}
	}
};

#endif // !__P_OBJECT_POOL__