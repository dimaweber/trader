#include "http_query.h"

size_t HttpQuery::writeFunc(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	QByteArray* array = reinterpret_cast<QByteArray*>(userdata);
	int lenBeforeAppend = array->length();
	array->append(ptr, size * nmemb);
	return array->length() - lenBeforeAppend;
}
