#include <stdio.h>
#include "protos.h"

int main(void) {

	ActivateVirtualTerminalEscapes();

	LPWSTR SERVER = L"www.python.org";
	LPWSTR ACCESS_POINT = L"/downloads/windows/";

	SCRHANDLES hScrStructs = HttpGet(SERVER, ACCESS_POINT);

	LPSTR pszHtml = ReadHttpResponse(hScrStructs);

	if (!pszHtml) {
		return 1;
	}

	// '\x1b[31m' is the implementation of ESC [ <n> m with <n> being 31.
	// printf_s("\x1b[33m%s\x1b[m\n", pszHtml);

	LPSTR pszStable = GetStableReleases(pszHtml, RESP_BUFF_SIZE);
	
	if (!pszStable) {
		return 1;
	}

	printf_s("\x1b[33m%s\x1b[m\n", pszStable);

	free(pszHtml);
	free(pszStable);

	return 0;
}