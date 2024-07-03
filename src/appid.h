#ifndef APPID_H_
#define APPID_H_

#include <propkey.h>
#include <propvarutil.h>
#include <shobjidl.h>

typedef HRESULT(WINAPI* pPSStringFromPropertyKey)(REFPROPERTYKEY pkey,
                                                  LPWSTR psz,
                                                  UINT cch);
pPSStringFromPropertyKey RawPSStringFromPropertyKey = nullptr;

HRESULT WINAPI MyPSStringFromPropertyKey(REFPROPERTYKEY pkey,
                                         LPWSTR psz,
                                         UINT cch) {
  HRESULT result = RawPSStringFromPropertyKey(pkey, psz, cch);
  if (SUCCEEDED(result)) {
    if (pkey == PKEY_AppUserModel_ID) {
      // DebugLog(L"MyPSStringFromPropertyKey %s", psz);
      return -1;
    }
  }
  return result;
}

void SetAppId() {
  HMODULE Propsys = LoadLibrary(L"Propsys.dll");

  PBYTE PSStringFromPropertyKey =
      (PBYTE)GetProcAddress(Propsys, "PSStringFromPropertyKey");
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach((LPVOID*)&RawPSStringFromPropertyKey, MyPSStringFromPropertyKey);
  DetourTransactionCommit();
}

#endif  // APPID_H_
