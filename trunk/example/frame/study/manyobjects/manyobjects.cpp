#include <iostream>
#include <deque>
#include <vector>
#include <unordered_map>
#include <string>

#include "system/common.hpp"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/string_generator.hpp"
#include "boost/uuid/uuid_io.hpp"

using namespace std;
using namespace solid;

struct Stub{
	Stub():uid(0){}
	boost::uuids::uuid	uuid;
	uint32				uid;
	string				name;
	vector<int>			idvec;
};
typedef std::deque<Stub>	StubVectorT;

int main(int argc, char *argv[]){
	StubVectorT	stubvec;
	int cnt = 1000000;
	if(argc == 2){
		cnt = atoi(argv[1]);
	}
	{
		int stime;
		long ltime;

		ltime = time(NULL); /* get current calendar time */
		stime = (unsigned int) ltime/2;
		srand(stime);
	}
	cout<<"Creating "<<cnt<<" elements. sizeof stub "<<sizeof(Stub)<<" uid "<<sizeof(boost::uuids::uuid)<<endl;
	
	boost::uuids::string_generator gen;
	for(size_t i = 0; i < cnt; ++i){
		boost::uuids::uuid u1 = gen("{01234567-89ab-cdef-0123-456789abcdef}");
		stubvec.push_back(Stub());
		stubvec.back().uuid = u1;
		int r = rand();
		{
			size_t veccnt = r % 32;
			stubvec.back().idvec.reserve(veccnt);
			for(size_t i = 0; i < veccnt; ++i){
				stubvec.back().idvec.push_back(r + i);
			}
			string s = to_string(u1);
			size_t strsz = r % 16;
			stubvec.back().name = s.substr(0, strsz);
		}
		if(i < 1000){
			cout<<"Created: "<<i<<'\r'<<flush;
		}else if(!(i % 1000)){
			cout<<"Created: "<<i<<'\r'<<flush;
		}
	}
	cout<<"\nDone creating"<<endl;
	char c;
	cin>>c;
	return 0;
}
