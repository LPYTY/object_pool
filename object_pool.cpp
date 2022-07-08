// object_pool.cpp : 定义静态库的函数。
//

#include "pch.h"
#include "framework.h"
#include "object_pool.h"

int nnnn = 100;
object_pool<string> op_s(nnnn);
object_pool<int> op_i;

int main()
{
	string *buf = (string *)malloc(sizeof(string));

	do
	{
		// Construct
		object_class<string> stringClass(buf);

		stringClass.create_object("asdf");

		// Get value
		string *stringObject = stringClass.get_object();
		cout << *stringObject << endl;

		// Multicast
		object_ptr<string> ptr1(&stringClass);
		cout << *ptr1 << endl;

		object_ptr<string> ptr2 = ptr1;
		*ptr2 = "fdsa";
		cout << *ptr1 << "; " << *ptr2 << endl;
	} while (0);

	free(buf);

	// Object Pool test
	object_pool<string> pool(1);

	do
	{
		auto o = pool.get_object();
		*o = "1234";
	} while (0);

	// Test gc
	do
	{
		auto o = pool.get_object();
		cout << *o << endl;
		*o = "4321";
		cout << *o << endl;
	} while (0);

	// Test expand
	do
	{
		auto o1 = pool.get_object();
		*o1 = "abc";
		auto o2 = pool.get_object();
		*o2 = "def";
		cout << *o1 << *o2 << endl;
	} while (0);

	// Test shrink
	do
	{
		pool.shrink(true);
	} while (0);

	return 0;
}
