#ifndef DEBUG_H
#define DEBUG_H

#include <vulkan/vulkan.h>
#include <iostream>
using namespace std;

const char* ResultToString(VkResult res);

template<class Tmsg>
void ErrorMessage(const Tmsg& msg)
{
	cout << msg << '\n';
}

template<class Tmsg>
void ErrorMessage(Tmsg& msg, VkResult res)
{
	cout <<  msg << "\nVkResult: " << ResultToString(res) << '\n';
}

#endif //DEBUG_H
