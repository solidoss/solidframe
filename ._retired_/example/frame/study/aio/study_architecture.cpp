#include <iostream>
#include "system/socketdevice.hpp"
using namespace std;

class Socket{
	
};

class Context{
	
};

template <class Sock, class Obj>
class Listener{
public:
	Listener(Obj &_robj):pobj(&_robj){}
	
	
	
	template <void(Obj::*TF)(Context&, solid::SocketDevice &)>
	bool accept(Context &_rctx){
		cout<<"1"<<endl;
		return true;
	}
	
	template <class P, void(Obj::*TF)(Context&, solid::SocketDevice &, P &)>
	bool accept(Context &_rctx, P _p){
		cout<<"2"<<endl;
		return true;
	}
	
	template <void(Obj::*TF)(Context&, solid::SocketDevice &)>
	bool accept(Context &_rctx, solid::SocketDevice &_rsd){
		cout<<"3"<<endl;
		return true;
	}
	
	template <class P, void(Obj::*TF)(Context&, solid::SocketDevice &, P &)>
	bool accept(Context &_rctx, solid::SocketDevice &_rsd, P _p){
		cout<<"4"<<endl;
		return true;
	}
	
	typedef void(Obj::*MethodT)(Context&, solid::SocketDevice &);
	
	bool acc(Context &_rctx, MethodT _pt){
		return false;
	}
	template <class P>
	bool acc(Context &_rctx, void(Obj::*pf)(Context&, solid::SocketDevice &, P &), P _p){
		return false;
	}
	
	void call(){
		
	}
	
private:
	Obj	*pobj;
};

class Object{
public:
	Object():lsnr(*this){}
	void accept(){
		Context 				ctx;
		solid::SocketDevice 	sd;
		std::string				str("mama mia");
		lsnr.accept<&Object::doneAccept>(ctx);
		lsnr.accept<&Object::doneAccept>(ctx, sd);
		lsnr.accept<std::string, &Object::doneAccept>(ctx, str);
		lsnr.accept<std::string, &Object::doneAccept>(ctx, sd, str);
		
		lsnr.acc(ctx, &Object::doneAcc);
		lsnr.acc(ctx, &Object::doneAcc, str);
	}
	
	void doneAccept(Context &_rctx, solid::SocketDevice &_rsd){
		
	}
	void doneAccept(Context &_rctx, solid::SocketDevice &_rsd, std::string &_str){
		
	}
	
	void doneAcc(Context &_rctx, solid::SocketDevice &_rsd){
		
	}
	void doneAcc(Context &_rctx, solid::SocketDevice &_rsd, std::string &_str){
		
	}
private:
	typedef Listener<Socket, Object>	ListenerT;
	ListenerT	lsnr;
};

int main(int argc, char *argv[]){
	Object obj;
	obj.accept();
	return 0;
}
