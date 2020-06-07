#include <string>
#include <iostream>
#include "DesktopRecorder.hpp"

using namespace std;

int main()
{
	DesktopRecorder recorder;

	while (true) {
		string line;
		printf("(s)tart, s(t)op, (q)uit: ");
		getline(cin, line);

		if (line == "s") recorder.start("test.mp4", 0, 0, 1024, 768);
		if (line == "t") recorder.stop();
		if (line == "q") {
			recorder.terminate();
			break;
		}
	}
}