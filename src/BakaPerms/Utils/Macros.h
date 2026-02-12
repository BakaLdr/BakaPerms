#pragma once
#ifdef BAKAPERMS_EXPORTS
#define BAKA_PERMAPI __declspec(dllexport)
#else
#define BAKA_PERMAPI __declspec(dllimport)
#endif