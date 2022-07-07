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
	clock_t c1b, c1e, c2b, c2e, c3b, c3e;

	auto pl = op_i.get_object_list(10, 1);
	cout << pl[1] << endl;

	auto ppp = new object_ptr<string>*[nnnn];

	cerr << "step1\n";
	c1b = clock();
	for (int i = 0; i < nnnn; i++)
	{
		ppp[i] = new object_ptr<string>(op_s.get_object());
	}
	c1e = clock();
	cerr << "step2\n";
	c2b = clock();
	for (int i = 0; i < nnnn; i += 2)
	{
		ppp[i]->destroy();
	}
	c2e = clock();
	cerr << "step3\n";
	c3b = clock();
	object_ptr<string> p1 = op_s.get_object(), p2 = op_s.get_object();
	c3e = clock();
	cout << double(c1e - c1b) / CLOCKS_PER_SEC << endl << double(c2e - c2b) / CLOCKS_PER_SEC << endl << double(c3e - c3b) / CLOCKS_PER_SEC << endl;
	for (int i = 0; i < nnnn; i++)
	{
		delete ppp[i];
	}
	delete[] ppp;
	system("pause");
	return 0;
}
