#ifndef WL_MIRROR_UTIL_H_
#define WL_MIRROR_UTIL_H_

#define WLM_ARRAY_LENGTH(arr) ((sizeof ((arr))) / (sizeof (((arr))[0])))

#define __WLM_STRINGIFY_INTERNAL2(s) #s
#define __WLM_STRINGIFY_INTERNAL1(s) __WLM_STRINGIFY_INTERNAL2(s)
#define WLM_STRINGIFY(s) __WLM_STRINGIFY_INTERNAL1(s)

#define __WLM_CONCAT_INTERNAL2(s1, s2) s1 ## s2
#define __WLM_CONCAT_INTERNAL1(s1, s2) __WLM_CONCAT_INTERNAL2(s1, s2)
#define WLM_CONCAT(s1, s2) __WLM_CONCAT_INTERNAL1(s1, s2)

#endif
