#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_MEAN
#endif // _WIN32

#include <stdlib.h>
#include <Windows.h>
#include <Winhttp.h>

#pragma comment(lib, "Winhttp")

typedef struct {
	HINTERNET hSession;
	HINTERNET hConnection;
	HINTERNET hRequest;
} SCRHANDLES;

#define RESP_BUFF_SIZE 1048576U		// 1 MiBs

/*
// Prototypes.
*/

BOOL ActivateVirtualTerminalEscapes(VOID);
SCRHANDLES HttpGet(LPCWSTR pswzServerName, LPCWSTR pswzAccessPoint);
LPSTR ReadHttpResponse(SCRHANDLES scrHandles);
LPSTR GetStableReleases(LPSTR pszHtmlBody, DWORD dwSize);