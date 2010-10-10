#include "utility/mutualobjectstore.hpp"
#include <iostream>

using namespace std;

int main(int argc, char *argv[]){
	if(argc != 5){
		cout<<"./mutual _objpermutbts _mutrowsbts _mutcolsbts count"<<endl;
		return 0;
	}
	int objpermutbts(atoi(argv[1]));
	int mutrowsbts(atoi(argv[2]));
	int mutcolsbts(atoi(argv[3]));
	int count(atoi(argv[4]));
	MutualObjectStore<uint32> moc(objpermutbts, mutrowsbts, mutcolsbts);
	
	for(int i = 0; i < count; ++i){
		moc.safeObject(i) = i;
	}
	
	for(int i(0); i < count; ++i){
		cout<<"object("<<i<<") = "<<moc.object(i)<<endl;
	}
	
	return 0;
}
