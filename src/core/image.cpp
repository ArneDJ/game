#include <iostream>
#include <cstring>

#include "image.h"

template <class T>
Image<T>::Image(uint16_t w, uint16_t h, uint8_t chan)
{
	width = w;
	height = h;
	size = w * h * chan;
	data = new T[size];
	clear();
}

template <class T>
Image<T>::~Image(void)
{
	delete data;
}
	
template <class T>
void Image<T>::clear(void)
{
	std::memset(data, 0, size);
}
