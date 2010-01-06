#ifndef UTILITY_STRING_HPP
#define UTILITY_STRING_HPP

const char * charToString(unsigned _c);

//! Some cross platform cstring utility functions
struct cstring{
    //! Equivalent to strcmp
    static int cmp(const char* _s1, const char *_s2);
    //! Equivalent to strncmp
    static int ncmp(const char* _s1, const char *_s2, uint _len);
    //! Equivalent to strcasecmp
    static int casecmp(const char* _s1, const char *_s2);
    //! Equivalent to strncasecmp
    static int ncasecmp(const char* _s1, const char *_s2, uint _len);
};

#endif
