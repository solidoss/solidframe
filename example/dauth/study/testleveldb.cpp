#include "leveldb/db.h"
#include <iostream>
using namespace std;


int main(int argc, char *argv[]){
	leveldb::DB* db;
	leveldb::Options options;
	options.create_if_missing = true;
	leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
	if(status.ok()){
		cout<<"database open ok"<<endl;
	}else{
		cout<<"database not open"<<endl;
	}
	return 0;
}