//#include "stdafx.h"
#include <vector>
#include "Renderer.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR, int nShowCmd) {
	Renderer* renderer = new Renderer(hInstance, hPrevInstance, nShowCmd);
	return 0;
}

