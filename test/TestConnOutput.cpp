//
// Created by frank on 17-8-15.
//

#include "../Connection.h"

#include <string.h>

void output(Connection& conn, const char *data, size_t len)
{
	printf("output %lu bytes\n", len);
}

int main()
{
	Connection conn(1, 100, 10);
	conn.setOutputCallback(output);

	conn.update(0);

	std::string str(10, 'x');
	for (int i = 0; i < 10; ++i) {
		conn.send(str.c_str(), str.length());
	}
	conn.update(10);
}