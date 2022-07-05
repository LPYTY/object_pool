// object_pool.cpp : 定义静态库的函数。
//

#include "pch.h"
#include "framework.h"
#include "object_pool.h"

object_pool<string> op_s;

int main()
{
	auto ps = op_s.get_object();
	ps->append("s");
	string ss = *ps;
	cout << *ps << ss;
	return 0;
}
