#include <stdio.h>
#include "protos.h"



BOOL ActivateVirtualTerminalEscapes(VOID) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hConsole == INVALID_HANDLE_VALUE) {
		fprintf_s(stderr, "Error %ld in getting hConsole handle.\n", GetLastError());
		return FALSE;
	}
	DWORD dwMode = 0;
	if (!GetConsoleMode(hConsole, &dwMode)) {
		fprintf_s(stderr, "Error %ld in getting console mode.\n", GetLastError());
		return FALSE;
	}
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(hConsole, dwMode)) {
		fprintf_s(stderr, "Error %ld in enabling virtual terminal escapes.\n", GetLastError());
		return FALSE;
	}
	return TRUE;
}



SCRHANDLES HttpGet(LPCWSTR pswzServerName, LPCWSTR pswzAccessPoint) {

	/*
	* A convenient wrapper around WinHttp functions.
	* Allows to send a GET request and receive the response in one function call
	* without having to deal with the cascade of WinHttp callbacks.
	*/

	HINTERNET hSession = NULL, hConnection = NULL, hRequest = NULL;
	BOOL bHttpResults = FALSE;
	SCRHANDLES scrStruct = { .hSession = NULL, .hConnection = NULL, .hRequest = NULL };

	// Returns a valid session handle if successful, or NULL otherwise.
	// first of the WinHTTP functions called by an application. 
	// It initializes internal WinHTTP data structures and prepares for future calls from the application.
	hSession = WinHttpOpen(
		// impersonating Firefox to avoid request denials from automated clients by servers.
		L"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/110.0",
		WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0);

	if (hSession) {
		// Specifies the initial target server of an HTTP request and returns an HINTERNET connection handle
		// to an HTTP session for that initial target.
		// Returns a valid connection handle to the HTTP session if the connection is successful, or NULL otherwise.
		hConnection = WinHttpConnect(
			hSession,
			pswzServerName,
			INTERNET_DEFAULT_HTTP_PORT,		// Uses port 80 for HTTP and port 443 for HTTPS.
			0);

	}

	if (hConnection) {
		// Creates an HTTP request handle.
		// An HTTP request handle holds a request to send to an HTTP server and contains all 
		// RFC822/MIME/HTTP headers to be sent as part of the request.
		hRequest = WinHttpOpenRequest(
			hConnection,
			L"GET",
			pswzAccessPoint,
			NULL,	// Pointer to a string that contains the HTTP version. If this parameter is NULL, the function uses HTTP/1.1
			WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES, // Pointer to a null-terminated array of string pointers that 
			// specifies media types accepted by the client.
			// WINHTTP_DEFAULT_ACCEPT_TYPES, no types are accepted by the client. 
			// Typically, servers handle a lack of accepted types as indication that the client accepts 
			// only documents of type "text/*"; that is, only text documents & no pictures or other binary files
			0);
	}

	if (hRequest) {
		// Sends the specified request to the HTTP server.
		// Returns TRUE if successful, or FALSE otherwise.
		bHttpResults = WinHttpSendRequest(
			hRequest,
			WINHTTP_NO_ADDITIONAL_HEADERS,	// A pointer to a string that contains the additional headers to append to the request.
			0,	// An unsigned long integer value that contains the length, in characters, of the additional headers.
			WINHTTP_NO_REQUEST_DATA,	// A pointer to a buffer that contains any optional data to send immediately after the request headers
			0,	// An unsigned long integer value that contains the length, in bytes, of the optional data.
			0,	// An unsigned long integer value that contains the length, in bytes, of the total data sent.
			0);	// A pointer to a pointer-sized variable that contains an application-defined value that is passed, with the request handle, to any callback functions.
	}


	if (!bHttpResults) {
		fprintf_s(stderr, "Error %ld in the HttpGet procedure.\n", GetLastError());
		return scrStruct;
	}

	// these 3 handles need to be closed by the caller.
	scrStruct.hSession = hSession;
	scrStruct.hConnection = hConnection;
	scrStruct.hRequest = hRequest;

	return scrStruct;
}



LPSTR ReadHttpResponse(SCRHANDLES scrHandles) {

	// if the call to HttpGet() failed,
	if (scrHandles.hSession == NULL || scrHandles.hConnection == NULL || scrHandles.hRequest == NULL) {
		fprintf_s(stderr, "ReadHttpResponse failed. Possible errors in previous call to HttpGet.\n");
		return NULL;
	}

	// this procedure is required to close these three handles.
	HINTERNET hSession = scrHandles.hSession,
		hConnection = scrHandles.hConnection,
		hRequest = scrHandles.hRequest;

	// Calling malloc first and then calling realloc in a do while loop is terribly inefficient for a 
	// simple app sending a single GET request.
	// So, malloc all the needed memory beforehand and use a moving pointer to keep track of the
	// last write offset, so the next write operation can start from there such that we can prevent
	// overwriting previously written memory.

	LPSTR pszBuffer = (LPSTR) malloc(RESP_BUFF_SIZE);	// now that's 1 MiB.
	// if malloc() failed,
	if (!pszBuffer) {
		fprintf_s(stderr, "Failed in memory allocation. Error %ld\n", GetLastError());
		return NULL;
	}

	ZeroMemory(pszBuffer, RESP_BUFF_SIZE);		// zero out the buffer.
	LPSTR pszLastWriteOffset = pszBuffer;

	BOOL bReception = FALSE;
	bReception = WinHttpReceiveResponse(hRequest, NULL);

	if (!bReception) {
		fprintf_s(stderr, "Failed to read the HTTP response. Error %ld in WinHttpReceiveResponse\n",
			GetLastError());
		return NULL;
	}

	DWORD64 dw64TotalBytesInResponse = 0, dw64TotalBytesReadFromResponse = 0;
	DWORD dwBytesInCurrentQuery = 0, dwReadBytesInCurrentQuery = 0;

	do {

		dwBytesInCurrentQuery = 0;
		dwReadBytesInCurrentQuery = 0;

		if (!WinHttpQueryDataAvailable(hRequest, &dwBytesInCurrentQuery)) {
			fprintf_s(stderr, "Failed to query the response. Error %ld in WinHttpQueryDataAvailable\n",
				GetLastError());
			break;
		}

		// If there aren't any more bytes to read,
		if (!dwBytesInCurrentQuery) {
			break;
		}

		if (!WinHttpReadData(hRequest, pszLastWriteOffset,
			dwBytesInCurrentQuery, &dwReadBytesInCurrentQuery)) {
			fprintf_s(stderr, "Failed to read bytes from the response. Error %ld in WinHttpReadData\n",
				GetLastError());
			break;
		}

		// Increment the total counters.
		dw64TotalBytesInResponse += dwBytesInCurrentQuery;
		dw64TotalBytesReadFromResponse += dwReadBytesInCurrentQuery;

		// Move the caret for next write.
		pszLastWriteOffset += dwReadBytesInCurrentQuery;

#ifdef _DEBUG
		printf_s("Read %lu bytes in this iteration.\n", dwBytesInCurrentQuery);
#endif // _DEBUG

	} while (dwBytesInCurrentQuery > 0);


	// Using the base CloseHandle() here will (did) crash the debug session.
	WinHttpCloseHandle(hSession);
	WinHttpCloseHandle(hConnection);
	WinHttpCloseHandle(hRequest);

#ifdef _DEBUG
	printf_s("%llu bytes have been received in total.\n", dw64TotalBytesReadFromResponse);
#endif // _DEBUG

	return pszBuffer;
}



LPSTR GetStableReleases(LPSTR pszHtmlBody, DWORD dwSize) {

	DWORD dwStartOffset = 0, dwEndOffset = 0, dwBodyBytes = 0;
	LPSTR pszStable = NULL;

	for (DWORD i = 0; i < dwSize; ++i) {

		// if the text matches the <h2> tag,
		if (pszHtmlBody[i] == '<' && pszHtmlBody[i + 1] == 'h' &&
			pszHtmlBody[i + 2] == '2' && pszHtmlBody[i + 3] == '>') {
			
			// <h2>Stable Releases</h2>
			if (dwStartOffset == 0 && pszHtmlBody[i + 4] == 'S' && pszHtmlBody[i + 5] == 't' && 
				pszHtmlBody[i + 6] == 'a' && pszHtmlBody[i + 7] == 'b' && pszHtmlBody[i + 8] == 'l' &&
				pszHtmlBody[i + 9] == 'e') {
				// The HTML body contains only a single <h2> tag with an inner text that starts with "Stable"
				// so ignoring the " Releases</h2> part for cycle trimming.
				// If the start offset has already been found, do not waste time in this body in subsequent 
				// iterations -> short circuiting with the first conditional.
				dwStartOffset = (i + 24);
			}


			// <h2>Pre-releases</h2>
			if (pszHtmlBody[i + 4] == 'P' && pszHtmlBody[i + 5] == 'r' && pszHtmlBody[i + 6] == 'e' &&
				pszHtmlBody[i + 7] == '-' && pszHtmlBody[i + 8] == 'r' && pszHtmlBody[i + 9] == 'e'){
				// The HTML body contains only a single <h2> tag with an inner text that starts with "Pre"
				// so ignoring the "leases</h2> part for cycle trimming.
				dwEndOffset = (i - 1);
				// If found, break out of the loop.
				break;
			}
		}		
	}
	
	dwBodyBytes = dwEndOffset - dwStartOffset;

#ifdef _DEBUG
	printf_s("Start offset is %lu and stop offset id %lu. Stable releases string is %lu bytes long.\n",
		dwStartOffset, dwEndOffset, dwBodyBytes);
#endif // _DEBUG

	// Caller is expected to free this memory.
	pszStable = malloc(dwBodyBytes);
	if (!pszStable) {
		fprintf_s(stderr, "Call to malloc failed. Error %ld in GetStableReleases\n.", GetLastError());
		return NULL;
	}

	ZeroMemory(pszStable, dwBodyBytes);

	// Copy the needed number of bytes from the start offset to the new buffer.
	memcpy_s(pszStable, dwBodyBytes, (pszHtmlBody + dwStartOffset), dwBodyBytes);

	return pszStable;
}



ParsedPyStructs DeserializeStableReleases(LPSTR pszBody, DWORD dwSize) {

	// Caller is obliged to free the memory in return.pyStart.

	// A struct to be returned by this function that holds a pointer to the first Python struct
	// and the number of structs in the allocated memory.
	ParsedPyStructs ppsResult = { .pyStart = NULL, .dwStructCount = 0 };

	// Allocate memory for 30 Python structs.
	Python* pyContainer = (Python*) malloc(sizeof(Python) * 30U);

	// If malloc failed,
	if (!pyContainer) {
		fprintf_s(stderr, "Error %ld. Memory allocation error in DeserializeStableReleases.\n",
			GetLastError());
		return ppsResult;
	}

	// A counter to remember last deserialized Python struct.
	DWORD dwLastDeserializedOffset = 0;

	// Start and end offsets of the version and release date string.
	DWORD dwStart = 0, dwEnd = 0;

	// Template <a href="https://www.python.org/downloads/release/python-31010/">Python 3.10.10 - Feb. 8, 2023</a>
	for (DWORD i = 0; i < dwSize; ++i) {

		if (pszBody[i] == '<' && pszBody[i + 1] == 'a') {

			if (pszBody[i + 2] == ' ' && pszBody[i + 3] == 'h' && pszBody[i + 4] == 'r' && 
				pszBody[i + 5] == 'e' && pszBody[i + 6] == 'f' && pszBody[i + 7] == '=' &&
				pszBody[i + 8] == '?' && pszBody[i + 9] == 'h' && pszBody[i + 10] == 't' &&
				pszBody[i + 11] == 't' && pszBody[i + 12] == 'p' && pszBody[i + 13] == 's' &&
				pszBody[i + 14] == ':' && pszBody[i + 15] == '/' && pszBody[i + 16] == '/' &&
				pszBody[i + 17] == 'w' && pszBody[i + 18] == 'w' && pszBody[i + 19] == 'w' &&
				pszBody[i + 20] == '.' && pszBody[i + 21] == 'p' && pszBody[i + 22] == 'y' &&
				pszBody[i + 23] == 't' && pszBody[i + 24] == 'h' && pszBody[i + 25] == 'o' &&
				pszBody[i + 26] == 'n' && pszBody[i + 27] == '.' && pszBody[i + 28] == 'o' &&
				pszBody[i + 29] == 'r' && pszBody[i + 30] == 'g' && pszBody[i + 31] == '/' &&
				pszBody[i + 32] == 'd' && pszBody[i + 33] == 'o' && pszBody[i + 34] == 'w' &&
				pszBody[i + 35] == 'n' && pszBody[i + 36] == 'l' && pszBody[i + 37] == 'o' &&
				pszBody[i + 38] == 'a' && pszBody[i + 39] == 'd' && pszBody[i + 40] == 's' &&
				pszBody[i + 41] == '/') {

				for (DWORD j = 0; j < 50; ++j) {

					if (pszBody[i + j + 42] == '>') {
						dwStart = i + j + 42;
					}

					if (pszBody[i + j + 42] == '<') {
						dwEnd = i + j + 41;
						break;
					}

				}

				memcpy_s(
					(pyContainer[dwLastDeserializedOffset]).pszVersionAndReleaseDate,
					40U,
					(pszBody + dwStart),
					(dwEnd - dwStart));
			

				dwLastDeserializedOffset++;
				ppsResult.dwStructCount++;
			}
		}

		dwStart = 0;
		dwEnd = 0;

	}

	ppsResult.pyStart = pyContainer;

	return ppsResult;
}



VOID PrintPython(Python* pyStruct) {
	printf_s("%s\n", pyStruct->pszVersionAndReleaseDate);
	return;
}