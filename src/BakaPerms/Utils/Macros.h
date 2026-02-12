#pragma once
#ifdef BAKA_PERM_EXPORT
#define BAKA_PERMAPI __declspec(dllexport)
#else
#define BAKA_PERMAPI __declspec(dllimport)
#endif