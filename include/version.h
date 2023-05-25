#ifndef _NEXRAN_VERSION_H_
#define _NEXRAN_VERSION_H_

#define NEXRAN_VERSION_MAJOR 0
#define NEXRAN_VERSION_MINOR 1
#define NEXRAN_VERSION_PATCH 0
#define NEXRAN_VERSION_STRING "0.1.0"

extern "C"
{
    extern const char *NEXRAN_GIT_COMMIT;
    extern const char *NEXRAN_GIT_TAG;
    extern const char *NEXRAN_GIT_BRANCH;
    extern const char *NEXRAN_BUILD_TIMESTAMP;
}

#endif /* _NEXRAN_VERSION_H_ */
