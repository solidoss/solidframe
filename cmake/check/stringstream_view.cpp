#include <iostream>
#include <sstream>
using namespace std;

int main()
{
    ostringstream oss;
    oss << "test";
    cout << oss.view() << endl;
    return 0;
}