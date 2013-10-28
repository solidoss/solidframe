#ifndef CLIPARSER_HPP
#define CLIPARSER_HPP

#include <string>

namespace cli{

class Parser{
public:
    Parser(const char *_pb);
    
    bool isAtEnd()const;
    
    void skipWhites();
    
    bool parse(int &_v);
    bool parse(std::string &_v);
    bool parse(char &_c);
    
private:
    const char *pb;
};

}//namespace cli

#endif
