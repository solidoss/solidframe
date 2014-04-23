*****************
Work in progress
*****************
namespace storage{

struct Record{
	uint16 kdebth;
	uint16 ksize;
	uint64 kdata;
	uint64 vsize;
	uint64 vdata;
};//28 B -> a node of 4096 will have at most 146 (* 28 = 4088) records


struct BaseKey{
	virtual int compare(void *_pdata) = 0;
	virtual int compare(void *_pdata, uint32 _size) = 0;
	//...
};

struct BaseData{
	virtual ~BaseData();
};


class Transaction{
};

class Manager{
public:
	int find(const std::vector<Key*> &_key, DataPointer &, const RequestUid &_rrequid, Transaction &_rt);
	int findUpper(const std::vector<Key*> &_key, DataPointer &, const RequestUid &_rrequid, Transaction &_rt);
	int findLower(const std::vector<Key*> &_key, DataPointer &, const RequestUid &_rrequid, Transaction &_rt);
	
	int insert(const std::vector<Key*> &_key, DataPointer &, const RequestUid &_rrequid, Transaction &_rt);
	int insert(const std::vector<Key*> &_key, DataPointer &, const RequestUid &_rrequid, Transaction &_rt);
	
	int dataOut(const DataPointer &, const char *_buf, uint32 _sz);
	int dataOut(const DataPointer &, StreamPointer<OutputStream> &_pos);
	
	int dataIn(const DataPointer &, char *_buf, uint32 _sz);
	int dataIn(const DataPointer &, StreamPointer<InputStream> &_pos);
	
	int find(Iterator &, DataPointer &, const RequestUid &_rrequid, Transaction &_rt);
	//the iterator will contain a vector with key pointers
	//these functions will reinitialize the last key with the real value.
	int findUpper(Iterator &, DataPointer &, const RequestUid &_rrequid, Transaction &_rt);
	int findLower(Iterator &, DataPointer &, const RequestUid &_rrequid, Transaction &_rt);
	
	virtual void sendData(const RequestUid &_rrequid, const DataPointer &);
	//the key is not allocated by Manager
	//it is the last key in the key vector contained by the iterator given to find*(Iterator..)
	virtual void sendData(const RequestUid &_rrequid, Key*, const DataPointer &);
};

}//namespace storage

//...

void printData(const storage::Iterator &_it){
	cassert(!_it);
	//first print the keys
	for(k_it(_it.keys().begin()); k_it != _it.keys().end(); ++k_it){
		cout<<'.';
		(*k_it)->print(cout);
	}
	cout<<'='<<' '<<'[';
	if(_it.data().isStream()){
		int rv;
		//we use DataStream buffer:
		while((rv = _it.data().stream().read(_it.data().buffer(), _it.data().capacity())) > 0){
			cout.write(_it.data().buffer(), rv);
		}
	}else{
		cout.write(_it.data().buffer(), _it.data().size());
	}
	cout<<']'<<endl;
}

void test(){
	storage::Manager mgr;
	//mgr initialization
	storage::InDataStream<1024>	ds;// if the value is bigger then 1024B the value will be retrieved from storage as a stream
	storage::Transaction t;
	
	{
		//we want to find the value for: "gigi".PERSONAL_DESCRIPTION
		//where PERSONAL_DESCRIPTION is an enum
		std::vector<Key*>	kv;
		kv.push_back(new storage::StringKey("gigi"));
		kv.push_back(new storage::UInt32Key(/*some enum*/PERSONAL_DESCRIPTION));
		
		
		int rv = mgr.find(kv, ds, t);
		if(!rv){//success: "gigi".PERSONAL_DESCRIPTION found
			if(ds.isStream()){
				//we use DataStream buffer:
				while((rv = ds.stream().read(ds.buffer(), ds.capacity())) > 0){
					cout.write(ds.buffer(), rv);
				}
			}else{
				cout.write(ds.buffer(), ds.size());
			}
		}
	}
	{
		//we want to iterate through: "gigi".*
		storage::Interator it(d);
		
		it.keys().push_back(new storage::StringKey("gigi"));
		it.keys().push_back(new storage::UInt32Key(0));//smallest value
		
		
		int rv = mgr.findUpper(it, t);
		while(!it){
			printData(it);
			mgr.findNext(it, t);
		}
		
	}
	{
		/*
		we want to insert some new values:
		"gigi".BIRTHDAY="10.10.2010"
		"gigi".SSL_CERT=[....some stream...]
		"gigi".FRIENDS
		"gigi".FRIENDS."titi"
		"gigi".FRIENDS."bibi"
		As you can see for now "gigi".FRIENDS."titi" and "gigi".FRIENDS."bibi" have no associated value
		FRIENDS is an enum.
		More we can do:
		"gigi" = UINT32_TYPEID
		"gigi".FRIENDS = STRING_TYPEID
		*/
		std::vector<Key*>	kv;
		string s;
		
		kv.push_back(new storage::StringKey("gigi"));
		cout>>"Full name: ";cin>>s;
		mgr.insert(
	}
	{
		/*
		Now we want to extend "gigi".FRIENDS."titi" with the folders titi is able to see from gigi:
		"gigi".FRIENDS."titi".SHARED_FOLDERS
		"gigi".FRIENDS."titi".SHARED_FOLDERS."jokes"
		"gigi".FRIENDS."titi".SHARED_FOLDERS."news"
		As you can see "jokes" and "news" have no associated value.
		SHARED_FOLDERS is an enum.
		*/
}

Some ideas:
1) First of all 

